#include "FreeRTOS.h"

#include <csp/csp.h>

#include <csp_proc/proc_types.h>
#include <csp_proc/proc_runtime.h>
#include <csp_proc/proc_analyze.h>

// NOTE: requires configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 in FreeRTOSConfig.h
#ifndef TASK_STORAGE_RECURSION_DEPTH_INDEX
#define TASK_STORAGE_RECURSION_DEPTH_INDEX 0
#endif

// forward declarations
int proc_runtime_ifelse(proc_instruction_t * instruction);
int proc_runtime_set(proc_instruction_t * instruction);
int proc_runtime_unop(proc_instruction_t * instruction);
int proc_runtime_binop(proc_instruction_t * instruction);
int proc_runtime_call(proc_instruction_t * instruction, proc_analysis_t ** analysis, proc_t ** proc, int * i, if_else_flag_t * _if_else_flag);

/**
 * Execute a block instruction.
 *
 * @param instruction The instruction to execute
 * @return int flag indicating the result of the block instruction (0 for success, -1 for error)
 */
int proc_runtime_block(proc_instruction_t * instruction) {
	if (instruction->type != PROC_BLOCK) {
		csp_print("Invalid instruction type, expected PROC_BLOCK\n");
		return -1;
	}

	TickType_t timeout_tick = xTaskGetTickCount() + pdMS_TO_TICKS(MAX_PROC_BLOCK_TIMEOUT_MS);

	// Create a temporary ifelse instruction from the block instruction for compatibility with proc_runtime_ifelse
	proc_instruction_t ifelse_instruction;
	ifelse_instruction.type = PROC_IFELSE;
	ifelse_instruction.node = instruction->node;
	ifelse_instruction.instruction.ifelse = instruction->instruction.block;

	while (xTaskGetTickCount() < timeout_tick) {
		int ifelse_result = proc_runtime_ifelse(&ifelse_instruction);
		if (ifelse_result == IF_ELSE_FLAG_ERR) {
			csp_print("Error in if-else condition %d\n", ifelse_result);
			return -1;
		} else if (ifelse_result == IF_ELSE_FLAG_TRUE) {
			break;
		}

		vTaskDelay(pdMS_TO_TICKS(MIN_PROC_BLOCK_PERIOD_MS));
	}

	if (xTaskGetTickCount() >= timeout_tick) {
		csp_print("Timeout reached in proc_runtime_block\n");
		return -1;
	}

	return 0;
}

int proc_instructions_exec(proc_t * proc, proc_analysis_t * analysis) {
	// NOTE: task local storage requires configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 in FreeRTOSConfig.h
	int recursion_depth = (int)pvTaskGetThreadLocalStoragePointer(NULL, TASK_STORAGE_RECURSION_DEPTH_INDEX);
	if (recursion_depth > MAX_PROC_RECURSION_DEPTH) {
		csp_print("Error: maximum recursion depth exceeded\n");
		return -1;
	}
	vTaskSetThreadLocalStoragePointer(NULL, TASK_STORAGE_RECURSION_DEPTH_INDEX, (void *)(recursion_depth + 1));

	if_else_flag_t _if_else_flag = IF_ELSE_FLAG_NONE;
	int ret = 0;

	for (int i = 0; i < proc->instruction_count; i++) {

		proc_instruction_t instruction = proc->instructions[i];

		if (_if_else_flag == IF_ELSE_FLAG_FALSE) {  // skip instruction
			_if_else_flag = IF_ELSE_FLAG_NONE;
			continue;
		} else if (_if_else_flag == IF_ELSE_FLAG_TRUE) {  // if-clause active, skip else-clause
			_if_else_flag = IF_ELSE_FLAG_FALSE;
		}

		switch (instruction.type) {
			case PROC_BLOCK:
				ret = proc_runtime_block(&instruction);
				break;
			case PROC_IFELSE:
				_if_else_flag = proc_runtime_ifelse(&instruction);
				ret = (_if_else_flag <= IF_ELSE_FLAG_ERR) ? _if_else_flag : 0;
				break;
			case PROC_SET:
				ret = proc_runtime_set(&instruction);
				break;
			case PROC_UNOP:
				ret = proc_runtime_unop(&instruction);
				break;
			case PROC_BINOP:
				ret = proc_runtime_binop(&instruction);
				break;
			case PROC_CALL:
				ret = proc_runtime_call(&instruction, &analysis, &proc, &i, &_if_else_flag);
				break;
			case PROC_NOOP:
				break;
			default:
				ret = -1;
				break;
		}

		// Error handling - TODO: smarter way to differentiate between errors from different instructions - maybe return a struct with error code and instruction index?
		if (ret != 0) {
			vTaskSetThreadLocalStoragePointer(NULL, TASK_STORAGE_RECURSION_DEPTH_INDEX, (void *)(recursion_depth));
			return ret;
		}
	}
	vTaskSetThreadLocalStoragePointer(NULL, TASK_STORAGE_RECURSION_DEPTH_INDEX, (void *)(recursion_depth));
	return ret;
}
