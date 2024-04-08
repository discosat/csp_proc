/*
This group of commands allows the user to create procedures with control-flow and arithmetic operations that can be stored and executed in a lightweight, customizable runtime on a given CSP node.
Small enough to fit in a single CSP packet!

- proc new
	- Create a new procedure and activate as currently active procedure context.
- proc del <procedure slot> [node]
	- Delete the procedure stored in the specified slot (0-255) on the node. Some slots may be reserved for predefined procedures.
- proc pull <procedure slot> [node]
	- Switch the context to the procedure stored in the specified slot (0-255) on the node. Some slots may be reserved for predefined procedures.
- proc push <procedure slot> [node]
	- Push the currently active procedure to the specified slot on the node.
- proc size
	- Size (in bytes) of the currently active procedure.
- proc pop [instruction index]
	- Remove the instruction at index [instruction index] (default to latest instruction) in currently active procedure.
- proc list
	- List instructions of the currently active procedure.
- proc slots [node]
	- List occupied procedure slots on node.
- proc run <procedure slot> [node]
	- Run the procedure in the specified slot.

Additionally, this adds the following commands to handle control-flow and operations within procedures. Result is always a parameter stored on the node hosting the corresponding procedure server (node 0 from its perspective) - Except when using the `rmt` unop operation, where it's switched with [node]!
- proc block <param a> <op> <param b> [node]
	- Block execution of the procedure until the condition is met. <op> is one of: ==, !=, <, >, <=, >=
- proc ifelse <param a> <op> <param b> [node]
	- Skip the next instruction if the condition is not met. Skip the next instruction after that if the condition is met. This instruction cannot be nested, i.e. the following two instructions cannot be ifelse. <op> is one of: ==, !=, <, >, <=, >=
- proc noop
	- No operation. Useful in combination with ifelse instructions.
- proc set <param> <value> [node]
	- Set the value of a parameter. The type of value is always inferred.
- proc unop <param> <op> <result> [node]
	- Apply unary operator on a parameter and store the result in <result>. <op> is one of: ++, --, !, -, idt, rmt
- proc binop <param a> <op> <param b> <result> [node]
	- Apply binary operator on parameters `a` and `b' and store the result in <result>. <op> is one of: +, -, *, /, %, <<, >>, &, |, ^
- proc call <procedure slot> [node]
	- Insert instruction to run the procedure in the specified slot.
*/

// TODO: implement functionality to name procedures ?
// TODO: implement functionality to add new parameters with `set`?
// TODO: mark certain procedures to be run on boot? (to handle mid-flight reboots)
// TODO: better / more comprehensive ACK?
// TODO: configure things like exponential backoff for block instruction?
// TODO: not quite a guarantee that procedures will fit in a single CSP packet, mainly because of the char* parameters. Can test with `proc size` to verify before pushing. Might have to support splitting procedures across multiple packets. Can char* arrays be compressed/referenced in a table sent separately? otherwise csp_sfp_send maybe?

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <csp_proc/proc_types.h>
#include <csp_proc/proc_client.h>
#include <csp_proc/proc_pack.h>

slash_command_group(proc, "Stored procedures");

proc_t * current_procedure;

comparison_op_t parse_comparison_op_enum(const char * str) {
	if (strcmp(str, "==") == 0) return OP_EQ;
	if (strcmp(str, "!=") == 0) return OP_NEQ;
	if (strcmp(str, "<") == 0) return OP_LT;
	if (strcmp(str, ">") == 0) return OP_GT;
	if (strcmp(str, "<=") == 0) return OP_LE;
	if (strcmp(str, ">=") == 0) return OP_GE;
	return -1;
}

unary_op_t parse_unary_op_enum(const char * str) {
	if (strcmp(str, "++") == 0) return OP_INC;
	if (strcmp(str, "--") == 0) return OP_DEC;
	if (strcmp(str, "!") == 0) return OP_NOT;
	if (strcmp(str, "-") == 0) return OP_NEG;
	if (strcmp(str, "idt") == 0) return OP_IDT;
	if (strcmp(str, "rmt") == 0) return OP_RMT;
	return -1;
}

binary_op_t parse_binary_op_enum(const char * str) {
	if (strcmp(str, "+") == 0) return OP_ADD;
	if (strcmp(str, "-") == 0) return OP_SUB;
	if (strcmp(str, "*") == 0) return OP_MUL;
	if (strcmp(str, "/") == 0) return OP_DIV;
	if (strcmp(str, "%") == 0) return OP_MOD;
	if (strcmp(str, "<<") == 0) return OP_LSH;
	if (strcmp(str, ">>") == 0) return OP_RSH;
	if (strcmp(str, "&") == 0) return OP_AND;
	if (strcmp(str, "|") == 0) return OP_OR;
	if (strcmp(str, "^") == 0) return OP_XOR;
	return -1;
}

const char * comparison_op_str[] = {
	"==",  // OP_EQ
	"!=",  // OP_NEQ
	"<",   // OP_LT
	">",   // OP_GT
	"<=",  // OP_LE
	">="   // OP_GE
};

const char * unary_op_str[] = {
	"++",   // OP_INC
	"--",   // OP_DEC
	"!",    // OP_NOT
	"-",    // OP_NEG
	"idt",  // OP_IDT
	"rmt"   // OP_RMT
};

const char * binary_op_str[] = {
	"+",   // OP_ADD
	"-",   // OP_SUB
	"*",   // OP_MUL
	"/",   // OP_DIV
	"%",   // OP_MOD
	"<<",  // OP_LSH
	">>",  // OP_RSH
	"&",   // OP_AND
	"|",   // OP_OR
	"^",   // OP_XOR
};

int instruction_can_be_added() {
	if (current_procedure == NULL) {
		printf("No active procedure. Use 'proc new' to create one.\n");
		return 0;
	}
	if (current_procedure->instruction_count >= MAX_INSTRUCTIONS) {
		printf("Maximum number of instructions reached for this procedure.\n");
		return 0;
	}
	return 1;
}

int proc_new(struct slash * slash) {
	if (current_procedure != NULL) {
		free_proc(current_procedure);
		current_procedure = NULL;
	}

	proc_t * _new_proc = malloc(sizeof(proc_t));
	if (_new_proc == NULL) {
		printf("Failed to allocate memory for new procedure\n");
		return SLASH_ENOMEM;
	}
	_new_proc->instruction_count = 0;

	current_procedure = _new_proc;
	printf("Created new procedure\n");

	return SLASH_SUCCESS;
}
slash_command_sub(proc, new, proc_new, "", "");

int proc_del(struct slash * slash) {
	unsigned int proc_slot;
	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;

	optparse_t * parser = optparse_new("proc del", "<procedure slot> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'p', "proc_slot", "NUM", 0, &proc_slot, "procedure slot");
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <procedure slot> (uint8) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	proc_slot = atoi(slash->argv[argi]);

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	if (proc_slot < RESERVED_PROC_SLOTS || proc_slot > MAX_PROC_SLOT) {
		printf("Invalid procedure slot %d\n", proc_slot);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	int ret = proc_del_request(proc_slot, node, timeout);
	if (ret != 0) {
		printf("Failed to delete procedure in slot %d on node %d with return code %d\n", proc_slot, node, ret);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	printf("Deleted procedure in slot %d\n", proc_slot);

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, del, proc_del, "<procedure slot> [node]", "");

int proc_pull(struct slash * slash) {
	unsigned int proc_slot;
	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;

	optparse_t * parser = optparse_new("proc pull", "<procedure slot> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'p', "proc_slot", "NUM", 0, &proc_slot, "procedure slot");
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <procedure slot> (uint8) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	proc_slot = atoi(slash->argv[argi]);

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	if (proc_slot < RESERVED_PROC_SLOTS || proc_slot > MAX_PROC_SLOT) {
		printf("Invalid procedure slot %d\n", proc_slot);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (current_procedure != NULL) {
		free_proc(current_procedure);
		current_procedure = NULL;
	}

	current_procedure = malloc(sizeof(proc_t));
	int ret = proc_pull_request(current_procedure, proc_slot, node, timeout);
	if (ret != 0) {
		printf("Failed to pull procedure from slot %d on node %d with return code %d\n", proc_slot, node, ret);
		free_proc(current_procedure);
		current_procedure = NULL;
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	printf("Switched context to procedure %d on node %d\n", proc_slot, node);

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, pull, proc_pull, "<procedure slot> [node]", "");

int proc_push(struct slash * slash) {
	if (current_procedure == NULL) {
		printf("No active procedure. Use 'proc new' to create one.\n");
		return SLASH_EINVAL;
	}
	unsigned int proc_slot;
	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;

	optparse_t * parser = optparse_new("proc push", "<procedure slot> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'p', "proc_slot", "NUM", 0, &proc_slot, "procedure slot");
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <procedure slot> (uint8) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	proc_slot = atoi(slash->argv[argi]);

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	if (proc_slot < RESERVED_PROC_SLOTS || proc_slot > MAX_PROC_SLOT) {
		printf("Invalid procedure slot %d\n", proc_slot);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	int ret = proc_push_request(current_procedure, proc_slot, node, timeout);
	if (ret != 0) {
		printf("Failed to push procedure to slot %d on node %d with return code %d\n", proc_slot, node, ret);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	printf("Pushed procedure to slot %d on node %d\n", proc_slot, node);

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, push, proc_push, "<procedure slot> [node]", "");

int proc_size(struct slash * slash) {
	if (current_procedure == NULL) {
		printf("No active procedure. Use 'proc new' to create one.\n");
		return SLASH_EINVAL;
	}

	printf("Size of current procedure: %d\n", calc_proc_size(current_procedure));

	return SLASH_SUCCESS;
}
slash_command_sub(proc, size, proc_size, "", "");

int proc_pop(struct slash * slash) {
	if (current_procedure == NULL) {
		printf("No active procedure. Use 'proc new' to create one.\n");
		return SLASH_EINVAL;
	}
	uint8_t step = current_procedure->instruction_count - 1;

	optparse_t * parser = optparse_new("proc pop", "[instruction index]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'i', "instruction", "NUM", 0, &step, "step (default = latest)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi < slash->argc) {
		step = atoi(slash->argv[argi]);
	}

	if (step == current_procedure->instruction_count - 1) {
		current_procedure->instruction_count--;
		printf("Removed latest instruction from procedure\n");
	} else if (step < current_procedure->instruction_count - 1) {
		// free the memory of the removed instruction
		proc_free_instruction(&current_procedure->instructions[step]);
		for (int i = step; i < current_procedure->instruction_count - 1; i++) {
			// shift remaining instructions one position left
			if (proc_copy_instruction(&current_procedure->instructions[i + 1], &current_procedure->instructions[i]) != 0) {
				printf("Failed to copy instruction at index %d\n", i + 1);
				optparse_del(parser);
				return SLASH_EINVAL;
			}
		}
		current_procedure->instruction_count--;
		printf("Removed instruction at index %d from procedure\n", step);
	} else {
		printf("Invalid instruction index %d\n", step);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, pop, proc_pop, "[instruction index]", "");

int proc_list(struct slash * slash) {
	if (current_procedure == NULL) {
		printf("No active procedure. Use 'proc new' to create one.\n");
		return SLASH_EINVAL;
	}

	printf("Current procedure contains the following %d instruction(s):\n", current_procedure->instruction_count);
	for (int i = 0; i < current_procedure->instruction_count; i++) {
		proc_instruction_t instruction = current_procedure->instructions[i];
		printf("%d:\t", i);
		switch (instruction.type) {
			case PROC_BLOCK:
				printf("[node %d]\tblock : %s %s %s\n", instruction.node, instruction.instruction.block.param_a, comparison_op_str[instruction.instruction.block.op], instruction.instruction.block.param_b);
				break;
			case PROC_IFELSE:
				printf("[node %d]\tifelse: %s %s %s\n", instruction.node, instruction.instruction.ifelse.param_a, comparison_op_str[instruction.instruction.ifelse.op], instruction.instruction.ifelse.param_b);
				break;
			case PROC_NOOP:
				printf("-\t\tnoop\n");
				break;
			case PROC_SET:
				printf("[node %d]\tset   : %s = %s\n", instruction.node, instruction.instruction.set.param, instruction.instruction.set.value);
				break;
			case PROC_UNOP:
				printf("[node %d]\tunop  : %s = %s(%s)\n", instruction.node, instruction.instruction.unop.result, unary_op_str[instruction.instruction.unop.op], instruction.instruction.unop.param);
				break;
			case PROC_BINOP:
				printf("[node %d]\tbinop : %s = %s %s %s\n", instruction.node, instruction.instruction.binop.result, instruction.instruction.binop.param_a, binary_op_str[instruction.instruction.binop.op], instruction.instruction.binop.param_b);
				break;
			case PROC_CALL:
				printf("[node %d]\tcall  : %d\n", instruction.node, instruction.instruction.call.procedure_slot);
				break;
			default:
				printf("Unknown instruction type %d\n", instruction.type);
				break;
		}
	}

	return SLASH_SUCCESS;
}
slash_command_sub(proc, list, proc_list, "", "");

int proc_slots(struct slash * slash) {
	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;
	uint8_t slots[MAX_PROC_SLOT + 1];
	uint8_t slot_count = 0;

	optparse_t * parser = optparse_new("proc slots", "[node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	int ret = proc_slots_request(slots, &slot_count, node, timeout);
	if (ret != 0) {
		printf("Failed to list procedure slots on node %d with return code %d\n", node, ret);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	printf("%d occupied procedure slots on node %d:\n", slot_count, node);
	for (int i = 0; i < slot_count; i++) {
		printf("%d\n", slots[i]);
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, slots, proc_slots, "[node]", "");

int proc_run(struct slash * slash) {
	unsigned int proc_slot;
	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;

	optparse_t * parser = optparse_new("proc run", "<procedure slot> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'p', "proc_slot", "NUM", 0, &proc_slot, "procedure slot");
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <procedure slot> (uint8) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	proc_slot = atoi(slash->argv[argi]);

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	if (proc_slot < RESERVED_PROC_SLOTS || proc_slot > MAX_PROC_SLOT) {
		printf("Invalid procedure slot %d\n", proc_slot);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	int ret = proc_run_request(proc_slot, node, timeout);
	if (ret != 0) {
		printf("Failed to run procedure in slot %d on node %d with return code %d\n", proc_slot, node, ret);
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	printf("Running procedure in slot %d on node %d\n", proc_slot, node);

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, run, proc_run, "<procedure slot> [node]", "");

int proc_block(struct slash * slash) {
	unsigned int node = slash_dfl_node;
	if (!instruction_can_be_added()) {
		return SLASH_EINVAL;
	}

	optparse_t * parser = optparse_new("proc block", "<param a> <op> <param b> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <param a> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * param_a = strdup(slash->argv[argi]);

	if (++argi >= slash->argc) {
		printf("Argument <op> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	int _parsed_op = parse_comparison_op_enum(slash->argv[argi]);
	if (_parsed_op == -1) {
		printf("Invalid comparison operator: %s\n", slash->argv[argi]);
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	comparison_op_t op = (comparison_op_t)_parsed_op;

	if (++argi >= slash->argc) {
		printf("Argument <param b> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * param_b = strdup(slash->argv[argi]);

	if (param_a == NULL || param_b == NULL) {
		printf("Failed to allocate memory for parameters\n");
		optparse_del(parser);
		return SLASH_ENOMEM;
	}

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	proc_block_t block = {param_a, op, param_b};

	proc_instruction_t proc_instruction;
	proc_instruction.node = node;
	proc_instruction.type = PROC_BLOCK;
	proc_instruction.instruction.block = block;

	current_procedure->instructions[current_procedure->instruction_count] = proc_instruction;
	current_procedure->instruction_count++;

	printf("Added block instruction to procedure\n");

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, block, proc_block, "<param a> <op> <param b> [node]", "");

int proc_ifelse(struct slash * slash) {
	unsigned int node = slash_dfl_node;
	if (!instruction_can_be_added()) {
		return SLASH_EINVAL;
	}

	optparse_t * parser = optparse_new("proc ifelse", "<param a> <op> <param b> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <param a> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * param_a = strdup(slash->argv[argi]);

	if (++argi >= slash->argc) {
		printf("Argument <op> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	int _parsed_op = parse_comparison_op_enum(slash->argv[argi]);
	if (_parsed_op == -1) {
		printf("Invalid comparison operator: %s\n", slash->argv[argi]);
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	comparison_op_t op = (comparison_op_t)_parsed_op;

	if (++argi >= slash->argc) {
		printf("Argument <param b> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * param_b = strdup(slash->argv[argi]);

	if (param_a == NULL || param_b == NULL) {
		printf("Failed to allocate memory for parameters\n");
		optparse_del(parser);
		return SLASH_ENOMEM;
	}

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	proc_ifelse_t ifelse = {param_a, op, param_b};

	proc_instruction_t proc_instruction;
	proc_instruction.node = node;
	proc_instruction.type = PROC_IFELSE;
	proc_instruction.instruction.ifelse = ifelse;

	current_procedure->instructions[current_procedure->instruction_count] = proc_instruction;
	current_procedure->instruction_count++;

	printf("Added ifelse instruction to procedure\n");

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, ifelse, proc_ifelse, "<param a> <op> <param b> [node]", "");

int proc_noop(struct slash * slash) {
	if (!instruction_can_be_added()) {
		return SLASH_EINVAL;
	}

	optparse_t * parser = optparse_new("proc noop", "");
	optparse_add_help(parser);

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	proc_instruction_t proc_instruction;
	proc_instruction.node = 0;
	proc_instruction.type = PROC_NOOP;

	current_procedure->instructions[current_procedure->instruction_count] = proc_instruction;
	current_procedure->instruction_count++;

	printf("Added noop instruction to procedure\n");

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, noop, proc_noop, "", "");

int proc_set(struct slash * slash) {
	unsigned int node = slash_dfl_node;
	if (!instruction_can_be_added()) {
		return SLASH_EINVAL;
	}

	optparse_t * parser = optparse_new("proc set", "<param> <value> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <param> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * param = strdup(slash->argv[argi]);

	if (++argi >= slash->argc) {
		printf("Argument <value> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * value = strdup(slash->argv[argi]);

	if (param == NULL || value == NULL) {
		printf("Failed to allocate memory for parameters\n");
		optparse_del(parser);
		return SLASH_ENOMEM;
	}

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	proc_set_t set = {param, value};

	proc_instruction_t proc_instruction;
	proc_instruction.node = node;
	proc_instruction.type = PROC_SET;
	proc_instruction.instruction.set = set;

	current_procedure->instructions[current_procedure->instruction_count] = proc_instruction;
	current_procedure->instruction_count++;

	printf("Added set instruction to procedure\n");

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, set, proc_set, "<param> <value> [node]", "");

int proc_unop(struct slash * slash) {
	unsigned int node = slash_dfl_node;
	if (!instruction_can_be_added()) {
		return SLASH_EINVAL;
	}

	optparse_t * parser = optparse_new("proc unop", "<param> <op> <result> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <param> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * param = strdup(slash->argv[argi]);

	if (++argi >= slash->argc) {
		printf("Argument <op> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	int _parsed_op = parse_unary_op_enum(slash->argv[argi]);
	if (_parsed_op == -1) {
		printf("Invalid unary operator: %s\n", slash->argv[argi]);
		return SLASH_EINVAL;
	}
	unary_op_t op = (unary_op_t)_parsed_op;

	if (++argi >= slash->argc) {
		printf("Argument <result> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * result = strdup(slash->argv[argi]);

	if (param == NULL || result == NULL) {
		printf("Failed to allocate memory for parameters\n");
		optparse_del(parser);
		return SLASH_ENOMEM;
	}

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	proc_unop_t unop = {param, op, result};

	proc_instruction_t proc_instruction;
	proc_instruction.node = node;
	proc_instruction.type = PROC_UNOP;
	proc_instruction.instruction.unop = unop;

	current_procedure->instructions[current_procedure->instruction_count] = proc_instruction;
	current_procedure->instruction_count++;

	printf("Added unary operation instruction to procedure\n");

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, unop, proc_unop, "<param> <op> <result> [node]", "");

int proc_binop(struct slash * slash) {
	unsigned int node = slash_dfl_node;
	if (!instruction_can_be_added()) {
		return SLASH_EINVAL;
	}

	optparse_t * parser = optparse_new("proc binop", "<param a> <op> <param b> <result> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <param a> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * param_a = strdup(slash->argv[argi]);

	if (++argi >= slash->argc) {
		printf("Argument <op> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	int _parsed_op = parse_binary_op_enum(slash->argv[argi]);
	if (_parsed_op == -1) {
		printf("Invalid binary operator: %s\n", slash->argv[argi]);
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	binary_op_t op = (binary_op_t)_parsed_op;

	if (++argi >= slash->argc) {
		printf("Argument <param b> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * param_b = strdup(slash->argv[argi]);

	if (++argi >= slash->argc) {
		printf("Argument <result> (char*) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * result = strdup(slash->argv[argi]);

	if (param_a == NULL || param_b == NULL || result == NULL) {
		printf("Failed to allocate memory for parameters\n");
		optparse_del(parser);
		return SLASH_ENOMEM;
	}

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	proc_binop_t binop = {param_a, op, param_b, result};

	proc_instruction_t proc_instruction;
	proc_instruction.node = node;
	proc_instruction.type = PROC_BINOP;
	proc_instruction.instruction.binop = binop;

	current_procedure->instructions[current_procedure->instruction_count] = proc_instruction;
	current_procedure->instruction_count++;

	printf("Added binary operation instruction to procedure\n");

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, binop, proc_binop, "<param a> <op> <param b> <result> [node]", "");

int proc_call(struct slash * slash) {
	unsigned int node = slash_dfl_node;
	if (!instruction_can_be_added()) {
		return SLASH_EINVAL;
	}

	optparse_t * parser = optparse_new("proc call", "<procedure slot> [node]");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Argument <procedure slot> (uint8_t) required\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	uint8_t procedure_slot = (uint8_t)atoi(slash->argv[argi]);

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	proc_call_t call = {procedure_slot};

	proc_instruction_t proc_instruction;
	proc_instruction.node = node;
	proc_instruction.type = PROC_CALL;
	proc_instruction.instruction.call = call;

	current_procedure->instructions[current_procedure->instruction_count] = proc_instruction;
	current_procedure->instruction_count++;

	printf("Added call instruction to procedure\n");

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(proc, call, proc_call, "<procedure slot> [node]", "");
