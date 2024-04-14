#ifndef CSP_PROC_PROC_STORE_H
#define CSP_PROC_PROC_STORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <csp_proc/proc_mutex.h>
#include <csp_proc/proc_types.h>

extern proc_mutex_t * proc_store_mutex;

/**
 * Delete/reinitialize a fresh procedure from the procedure storage.
 *
 * @param slot The slot to delete the procedure from
 */
int __attribute__((weak)) delete_proc(uint8_t slot);

/**
 * Reset the procedure storage.
 */
void __attribute__((weak)) reset_proc_store();

/**
 * Initialize the procedure storage and any necessary resources.
 */
int __attribute__((weak)) proc_store_init();

/**
 * Add a procedure to the procedure storage at the specified slot.
 *
 * @param proc The procedure to add
 * @param slot The slot to add the procedure to
 * @param overwrite If the slot is already occupied, overwrite the procedure
 *
 * @return The slot the procedure was added to, or -1 if the slot was occupied and overwrite was false
 */
int __attribute__((weak)) set_proc(proc_t * proc, uint8_t slot, int overwrite);

/**
 * Get a procedure from the procedure storage.
 *
 * @param slot The slot to get the procedure from
 *
 * @return The procedure at the specified slot, or NULL if the slot is empty
 */
proc_t * __attribute__((weak)) get_proc(uint8_t slot);

/**
 * Get the slots of the procedures in the procedure storage with an instruction count greater than 0.
 *
 * @return A dynamically allocated array containing the slots of the procedures, terminated by -1. The caller is responsible for freeing this array.
 */
int * __attribute__((weak)) get_proc_slots();

#ifdef __cplusplus
}
#endif

#endif  // CSP_PROC_PROC_STORE_H
