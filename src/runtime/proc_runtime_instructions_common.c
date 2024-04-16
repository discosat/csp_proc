#include <csp/csp.h>
#include <csp/csp_iflist.h>
#include <param/param.h>
#include <param/param_client.h>
#include <param/param_string.h>

#include <csp_proc/proc_types.h>
#include <csp_proc/proc_runtime.h>
#include <csp_proc/proc_analyze.h>
#include <csp_proc/proc_memory.h>

#ifndef PARAM_REMOTE_TIMEOUT_MS
#define PARAM_REMOTE_TIMEOUT_MS (1000)
#endif

#ifndef PARAM_ACK_ON_PUSH
#define PARAM_ACK_ON_PUSH (1)
#endif

#ifndef PROC_FLOAT_EPSILON
#define PROC_FLOAT_EPSILON (1e-6)
#endif

// Forward declaration
int proc_instructions_exec(proc_t * proc, proc_analysis_t * analysis);

/**
 * Simplified parameter type for performing arithmetic & logical operations.
 * Types are cast to the largest compatible type for simplicity.
 *
 * Operations on these types are defined in the individual instruction handlers
 * since we aren't blessed with operator overloading in C.
 */
typedef union {
	uint64_t u64;
	int64_t i64;
	double d;
	char * s;
} operand_val_t;

typedef enum {
	OPERAND_TYPE_UINT,
	OPERAND_TYPE_INT,
	OPERAND_TYPE_FLOAT,
	OPERAND_TYPE_STRING,
} operand_type_t;

typedef struct {
	param_type_e source_type;
	operand_type_t type;
	operand_val_t value;
} operand_t;

typedef struct {
	operand_t operand;
	param_t * param;
} operand_param_pair_t;

/**
 * Parse a parameter to an operand.
 *
 * @param param The parameter to parse
 * @return The operand representation of the parameter (null if invalid/unsupported type)
 */
int parse_param_to_operand(param_t * param, operand_t * operand, int offset) {
	operand->source_type = param->type;

	offset = (offset < 0) ? 0 : offset;

	switch (param->type) {
		case PARAM_TYPE_XINT8:
		case PARAM_TYPE_UINT8: {
			uint8_t value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_UINT;
			operand->value.u64 = (uint64_t)value;
			break;
		}
		case PARAM_TYPE_INT8: {
			int8_t value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_INT;
			operand->value.i64 = (int64_t)value;
			break;
		}
		case PARAM_TYPE_XINT16:
		case PARAM_TYPE_UINT16: {
			uint16_t value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_UINT;
			operand->value.u64 = (uint64_t)value;
			break;
		}
		case PARAM_TYPE_INT16: {
			int16_t value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_INT;
			operand->value.i64 = (int64_t)value;
			break;
		}
		case PARAM_TYPE_XINT32:
		case PARAM_TYPE_UINT32: {
			uint32_t value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_UINT;
			operand->value.u64 = (uint64_t)value;
			break;
		}
		case PARAM_TYPE_INT32: {
			int32_t value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_INT;
			operand->value.i64 = (int64_t)value;
			break;
		}
		case PARAM_TYPE_XINT64:
		case PARAM_TYPE_UINT64: {
			uint64_t value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_UINT;
			operand->value.u64 = value;
			break;
		}
		case PARAM_TYPE_INT64: {
			int64_t value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_INT;
			operand->value.i64 = value;
			break;
		}
		case PARAM_TYPE_FLOAT: {
			float value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_FLOAT;
			operand->value.d = value;
			break;
		}
		case PARAM_TYPE_DOUBLE: {
			double value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_FLOAT;
			operand->value.d = value;
			break;
		}
		case PARAM_TYPE_STRING: {
			char * value;
			param_get(param, offset, &value);
			operand->type = OPERAND_TYPE_STRING;
			operand->value.s = value;
			break;
		}
		default:
			csp_print("Invalid or unsupported parameter type\n");
			return -1;
	}

	return 0;
}

double float_abs(double x) {
	return (x < 0) ? -x : x;
}

int proc_param_scan_offset(char * arg) {
	char * saveptr;
	char * token;
	int offset = -1;

	strtok_r(arg, "[", &saveptr);
	token = strtok_r(NULL, "[", &saveptr);
	if (token != NULL) {
		sscanf(token, "%d", &offset);
		*token = '\0';
	}

	return offset;
}

param_t * proc_fetch_param(char * param_name, int node) {
	int inf_loop_guard = 0;
	param_t * param = NULL;

	// Check if parameter is an array element
	char * param_name_copy = proc_strdup(param_name);
	int offset = proc_param_scan_offset(param_name_copy);
	if (offset != -1) {
		int index_length = snprintf(NULL, 0, "%d", offset);
		param_name_copy[strlen(param_name_copy) - index_length + 1] = '\0';
	}

	csp_iface_t * ifaces = csp_iflist_get();
	int local_addrs[10] = {-1};  // Assume max 10 local interfaces (normally only 2-3)
	int iface_idx = 0;
	int is_remote_param = node != 0;
	while (ifaces && iface_idx++ < 10) {
		local_addrs[iface_idx] = ifaces->addr;
		ifaces = ifaces->next;
		if (is_remote_param && local_addrs[iface_idx] == node) {
			is_remote_param = 0;
		}
	}

	if (is_remote_param) {  // TODO: Don't download list every time
		int remote_params = param_list_download(node, PARAM_REMOTE_TIMEOUT_MS, 2, 1);
		if (remote_params < 0) {
			proc_free(param_name_copy);
			return NULL;
		}
	}

	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL && (inf_loop_guard++ < 10000)) {
		int strmatch_ret = strmatch(param->name, param_name_copy, strlen(param->name), strlen(param_name_copy));

		if (strmatch_ret == 0) {
			continue;
		}

		if (param->node != node) {
			for (int i = 0; i < 10; i++) {
				if (node == local_addrs[i] && param->node == 0) {
					proc_free(param_name_copy);
					return param;
				}
			}
			continue;
		}

		if (param->node == 0) {
			break;
		}

		if (param_pull_single(param, offset, CSP_PRIO_NORM, 0, node, PARAM_REMOTE_TIMEOUT_MS, 2) < 0) {
			proc_free(param_name_copy);
			return NULL;
		}

		break;
	}

	proc_free(param_name_copy);
	return param;
}

int fetch_operand_param_pair(char * param_name, operand_param_pair_t * pair, int node) {
	int offset = proc_param_scan_offset(param_name);

	pair->param = proc_fetch_param(param_name, node);
	if (pair->param == NULL) {
		csp_print("Failed to fetch %s\n", param_name);
		return -1;
	}
	if (parse_param_to_operand(pair->param, &pair->operand, offset) != 0) {
		csp_print("Failed to parse %s\n", param_name);
		return -1;
	}
	return 0;
}

int operand_to_valuebuf(operand_t * operand, char * valuebuf) {
	switch (operand->source_type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
			*(uint8_t *)valuebuf = (uint8_t)operand->value.u64;
			break;
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
			*(uint16_t *)valuebuf = (uint16_t)operand->value.u64;
			break;
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
			*(uint32_t *)valuebuf = (uint32_t)operand->value.u64;
			break;
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
			*(uint64_t *)valuebuf = operand->value.u64;
			break;
		case PARAM_TYPE_INT8:
			*(int8_t *)valuebuf = (int8_t)operand->value.i64;
			break;
		case PARAM_TYPE_INT16:
			*(int16_t *)valuebuf = (int16_t)operand->value.i64;
			break;
		case PARAM_TYPE_INT32:
			*(int32_t *)valuebuf = (int32_t)operand->value.i64;
			break;
		case PARAM_TYPE_INT64:
			*(int64_t *)valuebuf = operand->value.i64;
			break;
		case PARAM_TYPE_FLOAT:
			*(float *)valuebuf = (float)operand->value.d;
			break;
		case PARAM_TYPE_DOUBLE:
			*(double *)valuebuf = operand->value.d;
			break;
		case PARAM_TYPE_STRING:
			strcpy(valuebuf, operand->value.s);
			break;
		default:
			csp_print("Invalid or unsupported parameter type\n");
			return -1;
	}
	return 0;
}

char * proc_value_str_to_valuebuf(param_t * param, char * valuebuf, char * value_str) {
	if (param_str_to_value(param->type, value_str, valuebuf) < 0) {
		csp_print("invalid parameter value\n");
		return -1;
	}
	return 0;
}

int proc_set_param(char * param_name, operand_t * operand, char * value_str, int node) {
	param_t * param = NULL;

	if (operand == NULL && value_str == NULL) {
		csp_print("No value provided\n");
		return -1;
	}

	param = proc_fetch_param(param_name, node);
	if (param == NULL) {
		// TODO: add ability to add new parameters?
		csp_print("Failed to fetch %s\n", param_name);
		return -1;
	}

	if (param->mask & PM_READONLY) {
		csp_print("%s is read-only\n", param->name);
		return -1;
	}

	char valuebuf[128] __attribute__((aligned(16))) = {};
	int ret = (value_str != NULL) ? proc_value_str_to_valuebuf(param, valuebuf, value_str) : operand_to_valuebuf(operand, valuebuf);
	if (ret != 0) {
		return ret;
	}

	int offset = proc_param_scan_offset(param_name);

	if (param->node == 0) {  // Local parameter
		if (offset < 0) {
			for (int i = 0; i < param->array_size; i++) {
				param_set(param, i, valuebuf);
			}
		} else {
			param_set(param, offset, valuebuf);
		}
	} else {  // Remote parameter
		csp_timestamp_t time_now;
		csp_clock_get_time(&time_now);
		*param->timestamp = 0;
		if (param_push_single(param, offset, valuebuf, 0, node, PARAM_REMOTE_TIMEOUT_MS, 2, PARAM_ACK_ON_PUSH) < 0 && PARAM_ACK_ON_PUSH) {
			csp_print("No response\n");
			return -1;
		}
	}

	return 0;
}

/**
 * Execute an if-else instruction.
 *
 * @param instruction The instruction to execute
 * @return if_else_flag_t flag indicating the result of the if-else instruction (true, false, error)
 */
int proc_runtime_ifelse(proc_instruction_t * instruction) {
	if (instruction->type != PROC_IFELSE) {
		csp_print("Invalid instruction type, expected PROC_IFELSE\n");
		return IF_ELSE_FLAG_ERR;
	}

	param_t * param_a = proc_fetch_param(instruction->instruction.ifelse.param_a, instruction->node);
	param_t * param_b = proc_fetch_param(instruction->instruction.ifelse.param_b, instruction->node);

	if (param_a == NULL || param_b == NULL || param_a->type == PARAM_TYPE_INVALID || param_b->type == PARAM_TYPE_DATA) {
		csp_print("Failed to fetch params or invalid param type\n");
		return IF_ELSE_FLAG_ERR;
	}

	operand_param_pair_t op_par_pair_a, op_par_pair_b;
	if (fetch_operand_param_pair(instruction->instruction.ifelse.param_a, &op_par_pair_a, instruction->node) != 0) {
		csp_print("Failed to fetch operand A\n");
		return IF_ELSE_FLAG_ERR;
	}
	if (fetch_operand_param_pair(instruction->instruction.ifelse.param_b, &op_par_pair_b, instruction->node) != 0) {
		csp_print("Failed to fetch operand B\n");
		return IF_ELSE_FLAG_ERR;
	}

	switch (op_par_pair_a.operand.type) {
		case OPERAND_TYPE_UINT: {
			if (op_par_pair_b.operand.type != OPERAND_TYPE_UINT) {
				return IF_ELSE_FLAG_ERR_TYPE;
			}
			uint64_t value_a = op_par_pair_a.operand.value.u64;
			uint64_t value_b = op_par_pair_b.operand.value.u64;
			switch (instruction->instruction.ifelse.op) {
				case OP_EQ:
					return (value_a == value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_NEQ:
					return (value_a != value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_LT:
					return (value_a < value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_GT:
					return (value_a > value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_LE:
					return (value_a <= value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_GE:
					return (value_a >= value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
			}
			break;
		}
		case OPERAND_TYPE_INT: {
			if (op_par_pair_b.operand.type != OPERAND_TYPE_INT) {
				return IF_ELSE_FLAG_ERR_TYPE;
			}
			int64_t value_a = op_par_pair_a.operand.value.i64;
			int64_t value_b = op_par_pair_b.operand.value.i64;
			switch (instruction->instruction.ifelse.op) {
				case OP_EQ:
					return (value_a == value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_NEQ:
					return (value_a != value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_LT:
					return (value_a < value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_GT:
					return (value_a > value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_LE:
					return (value_a <= value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_GE:
					return (value_a >= value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
			}
			break;
		}
		case OPERAND_TYPE_FLOAT: {
			if (op_par_pair_b.operand.type != OPERAND_TYPE_FLOAT) {
				return IF_ELSE_FLAG_ERR_TYPE;
			}
			double value_a = op_par_pair_a.operand.value.d;
			double value_b = op_par_pair_b.operand.value.d;
			switch (instruction->instruction.ifelse.op) {
				case OP_EQ:
					return (float_abs(value_a - value_b) < PROC_FLOAT_EPSILON) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_NEQ:
					return (float_abs(value_a - value_b) >= PROC_FLOAT_EPSILON) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_LT:
					return (value_a < value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_GT:
					return (value_a > value_b) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_LE:
					return (value_a < value_b || float_abs(value_a - value_b) < PROC_FLOAT_EPSILON) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_GE:
					return (value_a > value_b || float_abs(value_a - value_b) < PROC_FLOAT_EPSILON) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
			}
			break;
		}
		case OPERAND_TYPE_STRING: {
			if (param_b->type != OPERAND_TYPE_STRING) {
				return IF_ELSE_FLAG_ERR_TYPE;
			}
			char * value_a = (char *)(param_a->addr);
			char * value_b = (char *)(param_b->addr);
			int cmp = strcmp(value_a, value_b);
			switch (instruction->instruction.ifelse.op) {
				case OP_EQ:
					return (cmp == 0) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_NEQ:
					return (cmp != 0) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_LT:
					return (cmp < 0) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_GT:
					return (cmp > 0) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_LE:
					return (cmp <= 0) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
				case OP_GE:
					return (cmp >= 0) ? IF_ELSE_FLAG_TRUE : IF_ELSE_FLAG_FALSE;
			}
			break;
		}
		default:
			csp_print("Invalid or unsupported operand type %d\n", op_par_pair_a.operand.type);
			break;
	}
	return IF_ELSE_FLAG_ERR;
}

int proc_runtime_set(proc_instruction_t * instruction) {
	if (instruction->type != PROC_SET) {
		csp_print("Invalid instruction type, expected PROC_SET\n");
		return -1;
	}

	return proc_set_param(instruction->instruction.set.param,
						  NULL,
						  instruction->instruction.set.value,
						  instruction->node);
}

int proc_runtime_unop(proc_instruction_t * instruction) {
	if (instruction->type != PROC_UNOP) {
		csp_print("Invalid instruction type, expected PROC_UNOP\n");
		return -1;
	}

	int fetch_node, result_node;
	if (instruction->instruction.unop.op == OP_RMT) {
		fetch_node = 0;
		result_node = instruction->node;
	} else {
		fetch_node = instruction->node;
		result_node = 0;
	}

	operand_param_pair_t op_par_pair;
	if (fetch_operand_param_pair(instruction->instruction.unop.param, &op_par_pair, fetch_node) != 0) {
		csp_print("Failed to fetch operand\n");
		return -1;
	}

	switch (instruction->instruction.unop.op) {
		case OP_INC:
		case OP_DEC:
			if (op_par_pair.operand.type == OPERAND_TYPE_FLOAT) {
				op_par_pair.operand.value.d = (instruction->instruction.unop.op == OP_INC) ? op_par_pair.operand.value.d + 1.0 : op_par_pair.operand.value.d - 1.0;
			} else if (op_par_pair.operand.type == OPERAND_TYPE_INT) {
				op_par_pair.operand.value.i64 = (instruction->instruction.unop.op == OP_INC) ? op_par_pair.operand.value.i64 + 1 : op_par_pair.operand.value.i64 - 1;
			} else if (op_par_pair.operand.type == OPERAND_TYPE_UINT) {
				op_par_pair.operand.value.u64 = (instruction->instruction.unop.op == OP_INC) ? op_par_pair.operand.value.u64 + 1 : op_par_pair.operand.value.u64 - 1;
			} else {
				csp_print("Error: Cannot increment or decrement type (%d)\n", op_par_pair.operand.type);
				return -1;
			}
			break;
		case OP_NOT:
			if (op_par_pair.operand.source_type == OPERAND_TYPE_INT) {
				op_par_pair.operand.value.i64 = ~op_par_pair.operand.value.i64;
			} else if (op_par_pair.operand.source_type == OPERAND_TYPE_UINT) {
				op_par_pair.operand.value.u64 = ~op_par_pair.operand.value.u64;
			} else {
				csp_print("Error: Cannot perform bitwise NOT on type (%d)\n", op_par_pair.operand.type);
				return -1;
			}
			break;
		case OP_NEG:
			if (op_par_pair.operand.type == OPERAND_TYPE_FLOAT) {
				op_par_pair.operand.value.d = -op_par_pair.operand.value.d;
			} else if (op_par_pair.operand.type == OPERAND_TYPE_INT) {
				op_par_pair.operand.value.i64 = -op_par_pair.operand.value.i64;
			} else {
				csp_print("Error: Cannot negate type (%d)\n", op_par_pair.operand.type);
				return -1;
			}
			break;
		case OP_IDT:
		case OP_RMT:
			break;
		default:
			csp_print("Invalid or unsupported unary operation (%d)\n", instruction->instruction.unop.op);
			return -1;
	}

	int ret = proc_set_param(
		instruction->instruction.unop.result,
		&op_par_pair.operand,
		NULL,
		result_node);

	return ret;
}

int proc_runtime_binop(proc_instruction_t * instruction) {
	if (instruction->type != PROC_BINOP) {
		csp_print("Invalid instruction type, expected PROC_BINOP\n");
		return -1;
	}

	operand_param_pair_t op_par_pair_a, op_par_pair_b;
	if (fetch_operand_param_pair(instruction->instruction.binop.param_a, &op_par_pair_a, instruction->node) != 0 ||
		fetch_operand_param_pair(instruction->instruction.binop.param_b, &op_par_pair_b, instruction->node) != 0) {
		csp_print("Failed to fetch operands\n");
		return -1;
	}

	switch (instruction->instruction.binop.op) {
		case OP_ADD:
		case OP_SUB:
		case OP_MUL:
		case OP_DIV:
			if (op_par_pair_a.operand.type == OPERAND_TYPE_FLOAT && op_par_pair_b.operand.type == OPERAND_TYPE_FLOAT) {
				switch (instruction->instruction.binop.op) {
					case OP_ADD:
						op_par_pair_a.operand.value.d += op_par_pair_b.operand.value.d;
						break;
					case OP_SUB:
						op_par_pair_a.operand.value.d -= op_par_pair_b.operand.value.d;
						break;
					case OP_MUL:
						op_par_pair_a.operand.value.d *= op_par_pair_b.operand.value.d;
						break;
					case OP_DIV:
						if (op_par_pair_b.operand.value.d == 0) {
							csp_print("Error: Division by zero\n");
							return -1;
						}
						op_par_pair_a.operand.value.d /= op_par_pair_b.operand.value.d;
						break;
					default:
						csp_print("Invalid or unsupported binary operation (%d)\n", instruction->instruction.binop.op);
						return -1;
				}
			} else if (op_par_pair_a.operand.type == OPERAND_TYPE_INT && op_par_pair_b.operand.type == OPERAND_TYPE_INT) {
				switch (instruction->instruction.binop.op) {
					case OP_ADD:
						op_par_pair_a.operand.value.i64 += op_par_pair_b.operand.value.i64;
						break;
					case OP_SUB:
						op_par_pair_a.operand.value.i64 -= op_par_pair_b.operand.value.i64;
						break;
					case OP_MUL:
						op_par_pair_a.operand.value.i64 *= op_par_pair_b.operand.value.i64;
						break;
					case OP_DIV:
						if (op_par_pair_b.operand.value.i64 == 0) {
							csp_print("Error: Division by zero\n");
							return -1;
						}
						op_par_pair_a.operand.value.i64 /= op_par_pair_b.operand.value.i64;
						break;
					default:
						csp_print("Invalid or unsupported binary operation (%d)\n", instruction->instruction.binop.op);
						return -1;
				}
			} else if (op_par_pair_a.operand.type == OPERAND_TYPE_UINT && op_par_pair_b.operand.type == OPERAND_TYPE_UINT) {
				switch (instruction->instruction.binop.op) {
					case OP_ADD:
						op_par_pair_a.operand.value.u64 += op_par_pair_b.operand.value.u64;
						break;
					case OP_SUB:
						op_par_pair_a.operand.value.u64 -= op_par_pair_b.operand.value.u64;
						break;
					case OP_MUL:
						op_par_pair_a.operand.value.u64 *= op_par_pair_b.operand.value.u64;
						break;
					case OP_DIV:
						if (op_par_pair_b.operand.value.u64 == 0) {
							csp_print("Error: Division by zero\n");
							return -1;
						}
						op_par_pair_a.operand.value.u64 /= op_par_pair_b.operand.value.u64;
						break;
					default:
						csp_print("Invalid or unsupported binary operation (%d)\n", instruction->instruction.binop.op);
						return -1;
				}
			} else {
				csp_print("Error: Cannot perform arithmetic operation on types (%d, %d)\n", op_par_pair_a.operand.type, op_par_pair_b.operand.type);
				return -1;
			}
			break;
		case OP_MOD:
		case OP_LSH:
		case OP_RSH:
		case OP_AND:
		case OP_OR:
		case OP_XOR:
			if (op_par_pair_a.operand.type == OPERAND_TYPE_INT && op_par_pair_b.operand.type == OPERAND_TYPE_INT) {
				switch (instruction->instruction.binop.op) {
					case OP_MOD:
						if (op_par_pair_b.operand.value.i64 == 0) {
							csp_print("Error: Division by zero\n");
							return -1;
						}
						op_par_pair_a.operand.value.i64 %= op_par_pair_b.operand.value.i64;
						break;
					case OP_LSH:
						op_par_pair_a.operand.value.i64 <<= op_par_pair_b.operand.value.i64;
						break;
					case OP_RSH:
						op_par_pair_a.operand.value.i64 >>= op_par_pair_b.operand.value.i64;
						break;
					case OP_AND:
						op_par_pair_a.operand.value.i64 &= op_par_pair_b.operand.value.i64;
						break;
					case OP_OR:
						op_par_pair_a.operand.value.i64 |= op_par_pair_b.operand.value.i64;
						break;
					case OP_XOR:
						op_par_pair_a.operand.value.i64 ^= op_par_pair_b.operand.value.i64;
						break;
					default:
						csp_print("Invalid or unsupported binary operation (%d)\n", instruction->instruction.binop.op);
						return -1;
				}
			} else if (op_par_pair_a.operand.type == OPERAND_TYPE_UINT && op_par_pair_b.operand.type == OPERAND_TYPE_UINT) {
				switch (instruction->instruction.binop.op) {
					case OP_MOD:
						if (op_par_pair_b.operand.value.u64 == 0) {
							csp_print("Error: Division by zero\n");
							return -1;
						}
						op_par_pair_a.operand.value.u64 %= op_par_pair_b.operand.value.u64;
						break;
					case OP_LSH:
						op_par_pair_a.operand.value.u64 <<= op_par_pair_b.operand.value.u64;
						break;
					case OP_RSH:
						op_par_pair_a.operand.value.u64 >>= op_par_pair_b.operand.value.u64;
						break;
					case OP_AND:
						op_par_pair_a.operand.value.u64 &= op_par_pair_b.operand.value.u64;
						break;
					case OP_OR:
						op_par_pair_a.operand.value.u64 |= op_par_pair_b.operand.value.u64;
						break;
					case OP_XOR:
						op_par_pair_a.operand.value.u64 ^= op_par_pair_b.operand.value.u64;
						break;
					default:
						csp_print("Invalid or unsupported binary operation (%d)\n", instruction->instruction.binop.op);
						return -1;
				}
			} else {
				csp_print("Error: Cannot perform operation on types (%d, %d)\n", op_par_pair_a.operand.type, op_par_pair_b.operand.type);
				return -1;
			}
			break;
		default:
			csp_print("Invalid or unsupported binary operation (%d)\n", instruction->instruction.binop.op);
			return -1;
	}

	int ret = proc_set_param(
		instruction->instruction.binop.result,
		&op_par_pair_a.operand,
		NULL,
		instruction->node);

	return ret;
}

int proc_runtime_call(proc_instruction_t * instruction, proc_analysis_t ** analysis, proc_t ** proc, int * i, if_else_flag_t * _if_else_flag) {
	if (instruction->type != PROC_CALL) {
		csp_print("Invalid instruction type, expected PROC_CALL\n");
		return -1;
	}
	proc_instruction_analysis_t * instruction_analysis = &((*analysis)->instruction_analyses[(*i)]);

	int analysis_index = -1;
	for (int j = 0; j < (*analysis)->procedure_slot_count; j++) {
		if ((*analysis)->procedure_slots[j] == instruction->instruction.call.procedure_slot) {
			analysis_index = j;
			break;
		}
	}
	if (analysis_index == -1) {
		csp_print("Error: procedure not found %d\n", instruction->instruction.call.procedure_slot);
		return -1;
	}

	if (instruction_analysis->analysis.call.is_tail_call) {
		// Avoid nesting procedure execution when tail call (reuse outer stack frame)
		*analysis = (*analysis)->sub_analyses[analysis_index];
		*proc = (*analysis)->proc;
		*_if_else_flag = IF_ELSE_FLAG_NONE;
		*i = -1;  // It will be incremented to 0 at the start of the next loop iteration
	} else {
		proc_analysis_t * called_proc_analysis = (*analysis)->sub_analyses[analysis_index];
		return proc_instructions_exec(called_proc_analysis->proc, called_proc_analysis);
	}
	return 0;
}
