#include <stdio.h>
#include "os.h"

// 5 levels of PTE
#define PT_LEVELS 5
#define SYMBOL_BITS 9
#define OFFSET_BITS 12
#define PTE_BYTES 8

const uint64_t SYMBOL_MASK = 0x1FF; // mask of 9 lower bits
const uint64_t VALID_MASK = 0x1;    // mask of the LSB

enum walk_mode
{
    Search = 0,
    Destroy = 0,
    Insert = 1
};

/**
 * Returns whether a pte (= page table entry) is valid.
 */
inline uint64_t is_valid_pte(uint64_t pte)
{
    return pte & VALID_MASK;
}

/**
 * Creates a pte (= page table entry) structure out of a page frame number.
 */
inline uint64_t create_pte(uint64_t frame_number)
{
    return (frame_number << OFFSET_BITS) + VALID_MASK;
}

/**
 * Gets the page number out of a pte (= page table entry).
 */
inline uint64_t get_frame_number(uint64_t pte)
{
    return pte >> OFFSET_BITS;
}

int get_index(uint64_t vpn, int level)
{
    int offset = SYMBOL_BITS * (PT_LEVELS - (level + 1));
    return (vpn >> offset) & SYMBOL_MASK;
}

/**
 * Returns a pointer to a Page Table leaf (represents the actual mapping entry of a vpn to a ppn)
 * If there is no such mapping, returns NULL
 */
uint64_t *page_walk(uint64_t pt, uint64_t vpn, enum walk_mode mode)
{
    int index;
    uint64_t *node;
    uint64_t *pte_ptr;

    for (int i = 0; i < PT_LEVELS; i++)
    {
        // Obtain the symbol from the vpn at each pt level, and slide the "mask window" to the right.
        index = get_index(vpn, i);

        // Get the page table node
        node = (uint64_t *)phys_to_virt(pt << OFFSET_BITS);

        // Get the page table entry pointer
        pte_ptr = &(node[index]);

        // Check for mapping
        if (!is_valid_pte(*pte_ptr))
        {
            if (mode == Insert)
            {
                // Prevent from allocating new frame the last level
                if (i == PT_LEVELS - 1)
                {
                    return pte_ptr;
                }

                // Create a new pte
                uint64_t new_frame = alloc_page_frame();
                *pte_ptr = create_pte(new_frame);
            }
            else
            {
                return NULL;
            }
        }

        // Update the pt to one level ahead
        pt = get_frame_number(*pte_ptr);
    }
    return pte_ptr; // Returns a pte leaf that represents the actual mapping of the vpn
}

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn)
{
    uint64_t *pte_leaf_ptr;

    if (ppn == NO_MAPPING)
    {
        // Destroy vpn mapping
        pte_leaf_ptr = page_walk(pt, vpn, Destroy);
        if (pte_leaf_ptr != NULL)
        {
            *pte_leaf_ptr = 0ULL;
        }
    }
    else
    {
        // Set vpn mapping to ppn
        pte_leaf_ptr = page_walk(pt, vpn, Insert);
        *pte_leaf_ptr = create_pte(ppn);
    }
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn)
{
    uint64_t *pte_leaf_ptr = page_walk(pt, vpn, Search);
    if (pte_leaf_ptr != NULL)
    {
        return get_frame_number(*pte_leaf_ptr);
    }
    return NO_MAPPING;
}