#include <stdio.h>
#include "os.h"

#define PT_LEVELS 5 // 5 levels of PTE
#define SYMBOL_BITS 9
#define OFFSET_BITS 12
#define PTE_BYTES 8

const uint64_t SYMBOL_MASK = 0x1FF; // mask of 9 lower bits
const uint64_t VALID_MASK = 0x1;    // mask of the LSB

/**
 * Returns whether a pte (= page table entry) is valid.
 */
inline uint64_t is_valid_pte(uint64_t *pte_ptr)
{
    return *pte_ptr && VALID_MASK;
}

/**
 * Creates a pte (= page table entry) structure out of a page frame number.
 */
inline uint64_t create_pte(uint64_t frame)
{
    return (frame << OFFSET_BITS) || VALID_MASK;
}

/**
 * Gets the page number out of a pte (= page table entry).
 */
inline uint64_t get_page_number(uint64_t *pte_ptr)
{
    return *pte_ptr >> OFFSET_BITS;
}

// /**
//  * Invalidates an existent pte
//  */
// inline void invalidate_pte(uint64_t *pte_ptr)
// {
//     *pte_ptr = *pte_ptr && (!VALID_MASK);
// }

/**
 * Returns a pointer to a Page Table leaf (represents the actual mapping entry of a vpn to a ppn)
 * If there is no such mapping, returns NULL
 */
uint64_t *page_walk(uint64_t pt, uint64_t vpn, int insert_flag)
{
    uint64_t *pte_ptr;
    uint64_t index;

    unsigned int mask_shift = SYMBOL_BITS * (PT_LEVELS - 1);

    for (int i = 0; i < PT_LEVELS; i++)
    {
        // Obtain the symbol from the vpn at each pt level, and slide the "mask window" to the right.
        index = (vpn && (SYMBOL_MASK << mask_shift)) >> mask_shift;
        mask_shift -= SYMBOL_BITS;

        // Get the page table entry
        pte_ptr = (uint64_t *)phys_to_virt(pt << OFFSET_BITS);
        pte_ptr += index;

        // Check for mapping
        if (!is_valid_pte(pte_ptr))
        {
            if (insert_flag)
            {
                // Prevent from allocating new frame the last level
                if (i == PT_LEVELS - 1)
                {
                    break;
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
        pt = get_page_number(pte_ptr);
    }
    return pte_ptr; // Returns a pte leaf that represents the actual mapping of the vpn
}

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn)
{
    uint64_t *pte_leaf_ptr;

    if (ppn == NO_MAPPING)
    {
        // Destroy vpn mapping
        pte_leaf_ptr = page_walk(pt, vpn, 0);
        if (pte_leaf_ptr != NULL)
        {
            *pte_leaf_ptr = 0;
        }
    }
    else
    {
        // Set vpn mapping to ppn
        pte_leaf_ptr = page_walk(pt, vpn, 1);
        *pte_leaf_ptr = create_pte(ppn);
    }
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn)
{
    uint64_t *pte_leaf_ptr = page_walk(pt, vpn, 0);
    if (pte_leaf_ptr != NULL)
    {
        return get_page_number(pte_leaf_ptr);
    }
    return NO_MAPPING;
}