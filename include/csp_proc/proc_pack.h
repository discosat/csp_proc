#ifndef CSP_PROC_PACK_H
#define CSP_PROC_PACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <csp/csp_types.h>
#include <csp_proc/proc_types.h>

int calc_proc_size(proc_t * procedure);

/**
 * Pack a proc_t procedure into a CSP packet.
 *
 * @param procedure The procedure to pack
 * @param packet The packet to pack the procedure into
 * @return 0 on success, -1 on failure
 */
int pack_proc_into_csp_packet(proc_t * procedure, csp_packet_t * packet);

/**
 * Unpack a proc_t procedure from a CSP packet.
 *
 * @param procedure The procedure to pack
 * @param packet The packet to pack the procedure into
 * @return 0 on success, -1 on failure
 */
int unpack_proc_from_csp_packet(proc_t * procedure, csp_packet_t * packet);

void proc_free_instruction(proc_instruction_t * instruction);

void free_proc(proc_t * procedure);

int proc_copy_instruction(proc_instruction_t * instruction, proc_instruction_t * copy);

int deepcopy_proc(proc_t * original, proc_t * copy);

#ifdef __cplusplus
}
#endif

#endif  // CSP_PROC_PACK_H
