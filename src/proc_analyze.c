#include <csp_proc/proc_analyze.h>
#include <csp_proc/proc_store.h>
#include <csp_proc/proc_memory.h>

/**
 * Free all memory associated with a proc_analysis_t.
 *
 * @param analysis The proc_analysis_t to free
 */
void free_proc_analysis(proc_analysis_t * analysis) {
	analysis->deallocation_mark = 1;
	for (size_t i = 0; i < analysis->sub_analysis_count; i++) {
		if (analysis->sub_analyses[i] != NULL && analysis->sub_analyses[i] != analysis && analysis->sub_analyses[i]->deallocation_mark == 0) {
			free_proc_analysis(analysis->sub_analyses[i]);
		}
	}

	proc_free(analysis->sub_analyses);
	proc_free(analysis->procedure_slots);
	proc_free(analysis->instruction_analyses);
	proc_free(analysis);
}

int analyze_tail_call(proc_t * proc, uint8_t i, proc_instruction_analysis_t * instruction_analysis) {
	// Assume it's a tail call
	instruction_analysis->analysis.call.is_tail_call = 1;

	// If the previous instruction is PROC_IFELSE and there are more than one instruction left, check the remaining instructions
	if (i > 0 && proc->instructions[i - 1].type == PROC_IFELSE && i + 1 < proc->instruction_count) {
		for (uint8_t j = i + 2; j < proc->instruction_count; j++) {
			if (proc->instructions[j].type != PROC_NOOP) {
				instruction_analysis->analysis.call.is_tail_call = 0;
				break;
			}
		}
	}
	// If the previous instruction is not PROC_IFELSE, check the remaining instructions
	else if (i == 0 || proc->instructions[i - 1].type != PROC_IFELSE) {
		for (uint8_t j = i + 1; j < proc->instruction_count; j++) {
			if (proc->instructions[j].type != PROC_NOOP) {
				instruction_analysis->analysis.call.is_tail_call = 0;
				break;
			}
		}
	}

	return 0;
}

int analyze_instruction(proc_t * proc, uint8_t instruction_index, proc_instruction_analysis_t * instruction_analysis) {
	proc_instruction_t * instruction = &proc->instructions[instruction_index];
	switch (instruction->type) {
		case PROC_BLOCK:
			break;
		case PROC_IFELSE:
			break;
		case PROC_SET:
			break;
		case PROC_UNOP:
			break;
		case PROC_BINOP:
			break;
		case PROC_CALL:
			if (analyze_tail_call(proc, instruction_index, instruction_analysis) != 0) {
				printf("Error analyzing tail call\n");
				return -1;
			}
			break;
		default:
			break;
	}

	return 0;
}

/**
 * Run static analysis on a procedure and populate a proc_analysis_t with the results.
 *
 * @param procedure The procedure to analyze
 * @param analysis The proc_analysis_t to populate
 * @param config The configuration for the analysis
 */
int proc_analyze(proc_t * proc, proc_analysis_t * analysis, proc_analysis_config_t * config) {
	printf("Analyzing procedure\n");
	analysis->proc = proc;
	analysis->deallocation_mark = 0;

	analysis->sub_analyses = NULL;
	analysis->sub_analysis_count = 0;

	analysis->procedure_slots = NULL;
	analysis->procedure_slot_count = 0;

	analysis->instruction_analyses = proc_calloc(proc->instruction_count, sizeof(proc_instruction_analysis_t));
	if (analysis->instruction_analyses == NULL) {
		printf("Error allocating memory for instruction_analyses\n");
		return -1;
	}

	for (uint8_t i = 0; i < proc->instruction_count; i++) {
		proc_instruction_t * instruction = &proc->instructions[i];
		proc_instruction_analysis_t * instruction_analysis = &analysis->instruction_analyses[i];
		instruction_analysis->type = instruction->type;

		if (analyze_instruction(proc, i, instruction_analysis) != 0) {
			printf("Error analyzing instruction %d with type %d\n", i, instruction->type);
			return -1;
		}

		if (instruction->type == PROC_CALL) {
			// Special handling for call instructions which require recursive analysis

			analysis->procedure_slots = proc_realloc(analysis->procedure_slots, (analysis->procedure_slot_count + 1) * sizeof(uint8_t));
			if (analysis->procedure_slots == NULL) {
				printf("Error reallocating memory for procedure_slots\n");
				return -1;
			}

			analysis->procedure_slots[analysis->procedure_slot_count] = instruction->instruction.call.procedure_slot;
			analysis->procedure_slot_count++;

			analysis->sub_analyses = proc_realloc(analysis->sub_analyses, (analysis->sub_analysis_count + 1) * sizeof(proc_analysis_t *));
			if (analysis->sub_analyses == NULL) {
				printf("Error reallocating memory for sub_analyses\n");
				return -1;
			}

			proc_t * sub_proc = get_proc(instruction->instruction.call.procedure_slot);
			if (sub_proc == NULL) {
				printf("Error fetching sub-procedure from procedure store\n");
				return -1;
			}

			proc_analysis_t * sub_analysis = NULL;

			if (config->analyzed_procs[instruction->instruction.call.procedure_slot] == 1) {
				// Procedure is already in the call stack
				sub_analysis = config->analyses[instruction->instruction.call.procedure_slot];
			} else {
				sub_analysis = proc_malloc(sizeof(proc_analysis_t));
				if (sub_analysis == NULL) {
					printf("Error allocating memory for sub_analysis\n");
					return -1;
				}

				// Mark the procedure as analyzed
				config->analyzed_procs[instruction->instruction.call.procedure_slot] = 1;
				config->analyses[instruction->instruction.call.procedure_slot] = sub_analysis;
				proc_analyze(sub_proc, sub_analysis, config);
			}

			analysis->sub_analyses[analysis->sub_analysis_count] = sub_analysis;
			analysis->sub_analysis_count++;
		}
	}

	return 0;
}

/**
 * Collect all procedure slots from a proc_analysis_t and its sub-analyses.
 *
 * @param analysis The proc_analysis_t to collect from
 * @param slots The array to populate with procedure slots
 * @param slot_count The number of procedure slots in the array
 */
void collect_proc_slots(proc_analysis_t * analysis, uint8_t ** slots, size_t * slot_count) {
	*slots = proc_realloc(*slots, (*slot_count + analysis->procedure_slot_count) * sizeof(uint8_t));
	memcpy(*slots + *slot_count, analysis->procedure_slots, analysis->procedure_slot_count * sizeof(uint8_t));
	*slot_count += analysis->procedure_slot_count;

	for (size_t i = 0; i < analysis->sub_analysis_count; i++) {
		collect_proc_slots(analysis->sub_analyses[i], slots, slot_count);
	}
}
