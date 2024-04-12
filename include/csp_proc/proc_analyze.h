#ifndef CSP_PROC_PROC_ANALYZE_H
#define CSP_PROC_PROC_ANALYZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <csp_proc/proc_types.h>

typedef struct {
	int is_tail_call;
} call_analysis_t;

typedef struct {
} block_analysis_t, ifelse_analysis_t, set_analysis_t, unop_analysis_t, binop_analysis_t;

typedef struct {
	proc_instruction_type_t type;
	union {
		block_analysis_t block;
		ifelse_analysis_t ifelse;
		set_analysis_t set;
		unop_analysis_t unop;
		binop_analysis_t binop;
		call_analysis_t call;
	} analysis;
} proc_instruction_analysis_t;

typedef struct proc_analysis_t proc_analysis_t;

struct proc_analysis_t {
	proc_t * proc;
	proc_analysis_t ** sub_analyses;
	size_t sub_analysis_count;
	uint8_t * procedure_slots;
	size_t procedure_slot_count;
	proc_instruction_analysis_t * instruction_analyses;
	int deallocation_mark;
};

/**
 * Configuration for proc_analyze.
 * This struct is also used to store state during analysis, so care should be taken if reusing it.
 */
typedef struct proc_analysis_config_t {
	int * analyzed_procs;
	proc_analysis_t ** analyses;
	size_t analyzed_proc_count;
} proc_analysis_config_t;

void free_proc_analysis(proc_analysis_t * analysis);

int proc_analyze(proc_t * proc, proc_analysis_t * analysis, proc_analysis_config_t * config);

#ifdef __cplusplus
}
#endif

#endif  // CSP_PROC_PROC_ANALYZE_H
