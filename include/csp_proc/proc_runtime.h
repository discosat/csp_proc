#ifndef CSP_PROC_RUNTIME_H
#define CSP_PROC_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <csp_proc/proc_types.h>
#include <csp_proc/proc_store.h>

// TODO: configurable as libparam params

#ifndef MAX_PROC_BLOCK_TIMEOUT_MS
#define MAX_PROC_BLOCK_TIMEOUT_MS (5000000U)
#endif  // ~83 minutes

#ifndef MIN_PROC_BLOCK_PERIOD_MS
#define MIN_PROC_BLOCK_PERIOD_MS (250U)
#endif

#ifndef MAX_PROC_RECURSION_DEPTH
#define MAX_PROC_RECURSION_DEPTH (1000U)
#endif

#ifndef MAX_PROC_CONCURRENT
#define MAX_PROC_CONCURRENT (16U)
#endif

/**
 * Initialize the procedure runtime and any necessary resources.
 * This function should only be called once. Any per-procedure configuration should be done in `proc_runtime_run`.
 *
 * @return 0 on success, -1 on failure
 */
int proc_runtime_init();

/**
 * Run a procedure stored in a given slot.
 *
 * @param proc_slot The slot of the procedure to run
 *
 * @return 0 on success, -1 on failure
 */
int proc_runtime_run(uint8_t proc_slot);

/**
 * Used to indicate the result of an if-else instruction in an instruction handler.
 */
typedef enum {
	IF_ELSE_FLAG_TRUE = 1,
	IF_ELSE_FLAG_FALSE = 0,
	IF_ELSE_FLAG_NONE = -1,
	IF_ELSE_FLAG_ERR = -2,
	IF_ELSE_FLAG_ERR_TYPE = -3,
} if_else_flag_t;

#ifdef __cplusplus
}
#endif

#endif  // CSP_PROC_RUNTIME_H
