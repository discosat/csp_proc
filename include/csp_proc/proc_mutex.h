#ifndef CSP_PROC_MUTEX_H
#define CSP_PROC_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#define PROC_MUTEX_OK  0
#define PROC_MUTEX_ERR 1

typedef struct proc_mutex_t proc_mutex_t;

/**
 * Create a new mutex.
 *
 * @return A pointer to the new mutex
 */
proc_mutex_t * proc_mutex_create();

/**
 * Take/lock mutex.
 *
 * @param mutex The mutex to take
 *
 * @return 0 on success, 1 on failure
 */
int proc_mutex_take(proc_mutex_t * mutex);

/**
 * Give/unlock mutex.
 *
 * @param mutex The mutex to give
 *
 * @return 0 on success, 1 on failure
 */
int proc_mutex_give(proc_mutex_t * mutex);

/**
 * Destroy mutex.
 *
 * @param mutex The mutex to destroy
 */
void proc_mutex_destroy(proc_mutex_t * mutex);

#ifdef __cplusplus
}
#endif

#endif  // CSP_PROC_MUTEX_H
