#include <pthread.h>
#include <time.h>

#include <csp/csp.h>

#include <csp_proc/proc_types.h>
#include <csp_proc/proc_runtime.h>
#include <csp_proc/proc_analyze.h>

// forward declarations
int proc_runtime_ifelse(proc_instruction_t * instruction);
int proc_runtime_set(proc_instruction_t * instruction);
int proc_runtime_unop(proc_instruction_t * instruction);
int proc_runtime_binop(proc_instruction_t * instruction);
int proc_runtime_call(proc_instruction_t * instruction, proc_analysis_t ** analysis, proc_t ** proc, int * i, if_else_flag_t * _if_else_flag);

pthread_key_t recursion_depth_key;

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

	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += MAX_PROC_BLOCK_TIMEOUT_MS / 1000;
	timeout.tv_nsec += (MAX_PROC_BLOCK_TIMEOUT_MS % 1000) * 1000000;

	// Create a temporary ifelse instruction from the block instruction for compatibility with proc_runtime_ifelse
	proc_instruction_t ifelse_instruction;
	ifelse_instruction.type = PROC_IFELSE;
	ifelse_instruction.node = instruction->node;
	ifelse_instruction.instruction.ifelse = instruction->instruction.block;

	struct timespec current_time;
	while (clock_gettime(CLOCK_REALTIME, &current_time) == 0 && (current_time.tv_sec < timeout.tv_sec || (current_time.tv_sec == timeout.tv_sec && current_time.tv_nsec < timeout.tv_nsec))) {
		int ifelse_result = proc_runtime_ifelse(&ifelse_instruction);
		if (ifelse_result == IF_ELSE_FLAG_ERR) {
			csp_print("Error in if-else condition %d\n", ifelse_result);
			return -1;
		} else if (ifelse_result == IF_ELSE_FLAG_TRUE) {
			break;
		}

		struct timespec sleep_time = {0, MIN_PROC_BLOCK_PERIOD_MS * 1000000};
		nanosleep(&sleep_time, NULL);
	}

	if (clock_gettime(CLOCK_REALTIME, &current_time) == 0 && (current_time.tv_sec > timeout.tv_sec || (current_time.tv_sec == timeout.tv_sec && current_time.tv_nsec >= timeout.tv_nsec))) {
		csp_print("Timeout reached in proc_runtime_block\n");
		return -1;
	}

	return 0;
}

int proc_instructions_exec(proc_t * proc, proc_analysis_t * analysis) {
	int recursion_depth = (int)pthread_getspecific(recursion_depth_key);
	if (recursion_depth > MAX_PROC_RECURSION_DEPTH) {
		csp_print("Error: maximum recursion depth exceeded\n");
		return -1;
	}
	pthread_setspecific(recursion_depth_key, (void *)(recursion_depth + 1));

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

		if (ret != 0) {
			pthread_setspecific(recursion_depth_key, (void *)(recursion_depth));
			return ret;
		}
	}
	pthread_setspecific(recursion_depth_key, (void *)(recursion_depth));
	return ret;
}
