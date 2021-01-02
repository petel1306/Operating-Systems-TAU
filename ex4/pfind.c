#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <threads.h>
#include <unistd.h>

#define printerr(message, ...) fprintf(stderr, "Error : " message, ##__VA_ARGS__)

typedef char path_t[PATH_MAX];

// ----------------- Queue implementation using doubly linked list --------------------------
struct dir_node
{
    struct dir_node *prev;
    struct dir_node *next;
    path_t path;
};
typedef struct dir_node dir_node_t;

dir_node_t *queue_head;
unsigned int queue_amount;

void create_queue(void)
{
    queue_amount = 0;

    queue_head = (dir_node_t *)malloc(sizeof(dir_node_t));
    queue_head->next = queue_head;
    queue_head->prev = queue_head;
}

int is_empty_queue(void)
{
    return (queue_amount == 0) ? 1 : 0;
}

void enqueue(const char *path)
{
    // Creates a new node
    dir_node_t *const new = (dir_node_t *)malloc(sizeof(dir_node_t));
    strcpy(new->path, path);

    // Add node to the start of the list
    dir_node_t *const first = queue_head->next;
    new->next = first;
    first->prev = new;
    new->prev = queue_head;
    queue_head->next = new;

    queue_amount++;
}

void dequeue(char *path)
{
    // Check if dequeue operation is possible
    if (is_empty_queue())
    {
        *path = '\0';
        return;
    }

    // Remove node from the end
    dir_node_t *rem = queue_head->prev;
    dir_node_t *last = queue_head->prev->prev;
    queue_head->prev = last;
    last->next = queue_head;

    strcpy(path, rem->path);
    free(rem);

    queue_amount--;
}

void destroy_queue(void)
{
    path_t spam;
    for (int i = 0; i < queue_amount; i++)
    {
        dequeue(spam);
    }

    free(queue_head);
}

// ------------------------ Threads & Synchronization setup ---------------------

#define THREAD_OK ((void *)0)
#define THREAD_ERROR ((void *)1)

pthread_mutex_t queue_mutex;
pthread_cond_t queue_cond;

unsigned int active_threads; // Protected by a queue_mutex lock
int is_finished;             // Protected by a queue_mutex lock

void err_thread_exit(void)
{
    pthread_mutex_lock(&queue_mutex);
    active_threads--;
    pthread_mutex_unlock(&queue_mutex);

    pthread_exit(THREAD_ERROR);
}

// -------------------------- Main logic --------------------------------
extern int errno;

const char *search_term;
atomic_uint files_found = 0; // Holds the number of matching files found

typedef enum
{
    SEARCHABLE_DIR,
    UNSEARCHABLE_DIR,
    NOT_DIR
} file_type_t;

/**
 * Returns essential info about the file located in <path>
 */
file_type_t file_info(const char *path)
{
    struct stat stat_buf;
    int status = lstat(path, &stat_buf);
    if (status == -1)
    {
        printerr("can't use lstat(%s) : %s\n", path, strerror(errno));
        // my_thread_exit()
        exit(1);
    }

    // Check if directory
    if (!S_ISDIR(stat_buf.st_mode))
    {
        return NOT_DIR; // Not directory
    }

    // check if directory is accessible
    if (access(path, R_OK) == 0 && access(path, X_OK) == 0)
    {
        return SEARCHABLE_DIR; // Accessible
    }
    else
    {
        return UNSEARCHABLE_DIR; // Inaccessible
    }
}

/**
 * Scans the directory.
 * If encounter an error, exists the thread
 */
void scan_directory(const char *dir_path)
{
    DIR *dirp = opendir(dir_path);
    if (dirp == NULL)
    {
        printerr("can't use open_dir(%s) : %s\n", dir_path, strerror(errno));
        // my_thread_exit();
        exit(1);
    }

    struct dirent *file_entry;
    while ((file_entry = readdir(dirp)) != NULL)
    {
        // Get file name and path
        const char *fname = file_entry->d_name;
        path_t fpath;
        sprintf(fpath, "%s/%s", dir_path, fname);

        // Ignore ".", ".." directories
        if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
        {
            continue;
        }

        // Get essential info on the file
        file_type_t ftype = file_info(fpath);

        int rc;
        switch (ftype)
        {
        case SEARCHABLE_DIR:

            // Enter the critical section
            rc = pthread_mutex_lock(&queue_mutex);
            if (rc != 0)
            {
                printerr("pthread_mutex_lock() failed : %s\n", strerror(rc));
                err_thread_exit();
            }

            // Put directory path in the queue, and send signal
            enqueue(fpath);
            pthread_cond_signal(&queue_cond);

            // Exit the critical section
            rc = pthread_mutex_unlock(&queue_mutex);
            if (rc != 0)
            {
                printerr("pthread_mutex_unlock() failed : %s\n", strerror(rc));
                err_thread_exit();
            }

            break;

        case UNSEARCHABLE_DIR:
            // Notify the directory can't be searched
            printf("Directory %s: Permission denied.\n", fpath);
            break;

        case NOT_DIR:
            // Check for a match with the search term
            if (strstr(fname, search_term) != NULL)
            {
                printf("%s\n", fpath);
                files_found++;
            }
            break;
        }
    }
    closedir(dirp);
}

/**
 * Threads lunch function
 */
void *thread_routine(void *bla)
{
    path_t dir_path;
    while (1)
    {
        // Enter the critical section
        int rc = pthread_mutex_lock(&queue_mutex);
        if (rc != 0)
        {
            printerr("pthread_mutex_lock() failed : %s\n", strerror(rc));
            err_thread_exit();
        }

        // Thread goes to sleep if queue is empty
        active_threads--;
        while (is_empty_queue())
        {
            // Notify if finished work
            if (active_threads == 0)
            {
                is_finished = 1;                     // set true
                pthread_cond_broadcast(&queue_cond); // Signal the other threads to exit
                pthread_exit(THREAD_OK);
            }

            // Wait for signal
            pthread_cond_wait(&queue_cond, &queue_mutex);

            // Exit if finished work
            if (is_finished)
            {
                pthread_exit(THREAD_OK);
            }
        }

        // Thread wakes up - queue is non-empty
        active_threads++;
        dequeue(dir_path);

        // Exit the critical section
        rc = pthread_mutex_unlock(&queue_mutex);
        if (rc != 0)
        {
            printerr("pthread_mutex_unlock() failed : %s\n", strerror(rc));
            err_thread_exit();
        }

        // Scans the directory
        scan_directory(dir_path);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int rc;

    // Validate arguments
    if (argc != 4)
    {
        printerr("Invalid command-line arguments amount\n");
        return 1;
    }

    // Extract arguments
    const char *root_dir = argv[1];
    search_term = argv[2];
    const unsigned int thread_amount = atoi(argv[3]);

    // Validates root directory
    if (access(root_dir, F_OK) != 0)
    {
        printerr("No such file '%s'\n", root_dir);
        return 1;
    }
    if (file_info(root_dir) != SEARCHABLE_DIR)
    {
        printerr("Root directory '%s' is not searchable\n", root_dir);
        return 1;
    }

    // --------------------- Prepare for launch -----------------------

    active_threads = thread_amount;
    is_finished = 0; // set false

    // Allocate array
    pthread_t *threads = (pthread_t *)calloc(thread_amount, sizeof(pthread_t));

    // Initialize queue
    create_queue();

    // Initialize mutex
    rc = pthread_mutex_init(&queue_mutex, NULL);
    if (rc)
    {
        printerr("pthread_mutex_init() failed : %s\n", strerror(rc));
        return 1;
    }

    // Initialize condition variable
    rc = pthread_cond_init(&queue_cond, NULL);
    if (rc)
    {
        printerr("pthread_cond_init() failed : %s\n", strerror(rc));
        return 1;
    }

    // -------------------- Launch threads -----------------------
    for (int t = 0; t < thread_amount; t++)
    {
        rc = pthread_create(&threads[t], NULL, thread_routine, NULL);
        if (rc)
        {
            printerr("pthread_create() #%d failed : %s\n", t, strerror(rc));
            return 1;
        }
    }

    // -------------------------- Starts scan --------------------------
    pthread_mutex_lock(&queue_mutex);

    enqueue(root_dir);
    pthread_cond_signal(&queue_cond);

    printf("Lets go!"); // Debug print

    pthread_mutex_unlock(&queue_mutex);

    // ------------------- Wait for threads to finish ------------------
    void *status;
    long err_threads = 0;
    for (int t = 0; t < thread_amount; t++)
    {
        rc = pthread_join(threads[t], &status);
        if (rc)
        {
            printerr("pthread_join() #%d failed : %s\n", t, strerror(rc));
            return 1;
        }
        err_threads += (long)status;
    }

    // ------------- Release resources ------------------
    destroy_queue();
    free(threads);
    pthread_cond_destroy(&queue_cond);
    pthread_mutex_destroy(&queue_mutex);

    // ----------------- Epilogue ---------------------
    printf("Done searching, found %d files\n", files_found);

    // If one of the threads has failed, than the exit code is 1
    return (err_threads > 0) ? 1 : 0;
}