#include <csp_proc/proc_pack.h>
#include <csp_proc/proc_memory.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/**
 * Calculate the size of a proc_t procedure in bytes
 *
 * @param procedure The procedure to calculate the size of
 * @return The size of the procedure in bytes or -1 on failure
 */
int calc_proc_size(proc_t * procedure) {
	int total_size = 0;
	total_size += sizeof(procedure->instruction_count);

	for (int i = 0; i < procedure->instruction_count; i++) {
		total_size += sizeof(procedure->instructions[i].node);
		total_size += sizeof(uint8_t);

		switch (procedure->instructions[i].type) {
			case PROC_BLOCK:
			case PROC_IFELSE:
				total_size += sizeof(procedure->instructions[i].instruction.block.op);
				total_size += strlen(procedure->instructions[i].instruction.block.param_a) + 1;
				total_size += strlen(procedure->instructions[i].instruction.block.param_b) + 1;
				break;
			case PROC_SET:
				total_size += strlen(procedure->instructions[i].instruction.set.param) + 1;
				total_size += strlen(procedure->instructions[i].instruction.set.value) + 1;
				break;
			case PROC_UNOP:
				total_size += sizeof(procedure->instructions[i].instruction.unop.op);
				total_size += strlen(procedure->instructions[i].instruction.unop.param) + 1;
				total_size += strlen(procedure->instructions[i].instruction.unop.result) + 1;
				break;
			case PROC_BINOP:
				total_size += sizeof(procedure->instructions[i].instruction.binop.op);
				total_size += strlen(procedure->instructions[i].instruction.binop.param_a) + 1;
				total_size += strlen(procedure->instructions[i].instruction.binop.param_b) + 1;
				total_size += strlen(procedure->instructions[i].instruction.binop.result) + 1;
				break;
			case PROC_CALL:
				total_size += sizeof(procedure->instructions[i].instruction.call.procedure_slot);
				break;
			case PROC_NOOP:
				break;
			default:
				printf("Unknown instruction type %d\n", procedure->instructions[i].type);
				return -1;
		}
	}

	return total_size;
}

int pack_proc_into_csp_packet(proc_t * procedure, csp_packet_t * packet) {
	int total_size = calc_proc_size(procedure);

	if ((total_size + 2) > CSP_BUFFER_SIZE) {
		return -1;  // Procedure is too large to fit into the packet
	}

	int offset = 2;  // Skip the first byte for the packet type and flags, and the second byte for the procedure slot

	// Pack instruction count
	uint8_t instruction_count = (uint8_t)procedure->instruction_count;  // explicit uint8_t cast to save space in packet
	memcpy(packet->data + offset, &instruction_count, sizeof(uint8_t));
	offset += sizeof(uint8_t);

	for (int i = 0; i < procedure->instruction_count; i++) {
		// Pack node
		memcpy(packet->data + offset, &(procedure->instructions[i].node), sizeof(uint16_t));
		offset += sizeof(uint16_t);

		// Pack type
		uint8_t type = (uint8_t)procedure->instructions[i].type;  // explicit uint8_t cast to save space in packet
		memcpy(packet->data + offset, &type, sizeof(uint8_t));
		offset += sizeof(uint8_t);

		// Pack instruction
		switch (procedure->instructions[i].type) {
			// Ensure all strings are null-terminated !
			case PROC_BLOCK:
			case PROC_IFELSE:
				memcpy(packet->data + offset, procedure->instructions[i].instruction.block.param_a, strlen(procedure->instructions[i].instruction.block.param_a) + 1);
				offset += strlen(procedure->instructions[i].instruction.block.param_a) + 1;
				memcpy(packet->data + offset, &(procedure->instructions[i].instruction.block.op), sizeof(comparison_op_t));
				offset += sizeof(comparison_op_t);
				memcpy(packet->data + offset, procedure->instructions[i].instruction.block.param_b, strlen(procedure->instructions[i].instruction.block.param_b) + 1);
				offset += strlen(procedure->instructions[i].instruction.block.param_b) + 1;
				break;
			case PROC_SET:
				memcpy(packet->data + offset, procedure->instructions[i].instruction.set.param, strlen(procedure->instructions[i].instruction.set.param) + 1);
				offset += strlen(procedure->instructions[i].instruction.set.param) + 1;
				memcpy(packet->data + offset, procedure->instructions[i].instruction.set.value, strlen(procedure->instructions[i].instruction.set.value) + 1);
				offset += strlen(procedure->instructions[i].instruction.set.value) + 1;
				break;
			case PROC_UNOP:
				memcpy(packet->data + offset, procedure->instructions[i].instruction.unop.param, strlen(procedure->instructions[i].instruction.unop.param) + 1);
				offset += strlen(procedure->instructions[i].instruction.unop.param) + 1;
				memcpy(packet->data + offset, &(procedure->instructions[i].instruction.unop.op), sizeof(unary_op_t));
				offset += sizeof(unary_op_t);
				memcpy(packet->data + offset, procedure->instructions[i].instruction.unop.result, strlen(procedure->instructions[i].instruction.unop.result) + 1);
				offset += strlen(procedure->instructions[i].instruction.unop.result) + 1;
				break;
			case PROC_BINOP:
				memcpy(packet->data + offset, procedure->instructions[i].instruction.binop.param_a, strlen(procedure->instructions[i].instruction.binop.param_a) + 1);
				offset += strlen(procedure->instructions[i].instruction.binop.param_a) + 1;
				memcpy(packet->data + offset, &(procedure->instructions[i].instruction.binop.op), sizeof(binary_op_t));
				offset += sizeof(binary_op_t);
				memcpy(packet->data + offset, procedure->instructions[i].instruction.binop.param_b, strlen(procedure->instructions[i].instruction.binop.param_b) + 1);
				offset += strlen(procedure->instructions[i].instruction.binop.param_b) + 1;
				memcpy(packet->data + offset, procedure->instructions[i].instruction.binop.result, strlen(procedure->instructions[i].instruction.binop.result) + 1);
				offset += strlen(procedure->instructions[i].instruction.binop.result) + 1;
				break;
			case PROC_CALL:
				memcpy(packet->data + offset, &(procedure->instructions[i].instruction.call.procedure_slot), sizeof(uint8_t));
				offset += sizeof(uint8_t);
				break;
			case PROC_NOOP:
				break;
			default:
				printf("Unknown instruction type %d\n", procedure->instructions[i].type);
				return -1;
		}
	}

	packet->length = total_size + 2;

	return 0;
}

int unpack_proc_from_csp_packet(proc_t * procedure, csp_packet_t * packet) {
	int offset = 2;  // Skip the first byte for the packet type and flags, and the second byte for the procedure slot

	// Unpack instruction count
	memcpy(&procedure->instruction_count, packet->data + offset, sizeof(uint8_t));
	offset += sizeof(uint8_t);

	for (int i = 0; i < procedure->instruction_count; i++) {
		// Unpack node
		memcpy(&procedure->instructions[i].node, packet->data + offset, sizeof(uint16_t));
		offset += sizeof(uint16_t);

		// Unpack type
		uint8_t type;
		memcpy(&type, packet->data + offset, sizeof(uint8_t));
		procedure->instructions[i].type = (int)type;
		offset += sizeof(uint8_t);

		// Unpack instruction
		switch (procedure->instructions[i].type) {
			case PROC_BLOCK:
			case PROC_IFELSE:
				procedure->instructions[i].instruction.block.param_a = proc_strdup(packet->data + offset);
				offset += strlen(packet->data + offset) + 1;
				memcpy(&procedure->instructions[i].instruction.block.op, packet->data + offset, sizeof(comparison_op_t));
				offset += sizeof(comparison_op_t);
				procedure->instructions[i].instruction.block.param_b = proc_strdup(packet->data + offset);
				offset += strlen(packet->data + offset) + 1;
				break;
			case PROC_SET:
				procedure->instructions[i].instruction.set.param = proc_strdup(packet->data + offset);
				offset += strlen(packet->data + offset) + 1;
				procedure->instructions[i].instruction.set.value = proc_strdup(packet->data + offset);
				offset += strlen(packet->data + offset) + 1;
				break;
			case PROC_UNOP:
				procedure->instructions[i].instruction.unop.param = proc_strdup(packet->data + offset);
				offset += strlen(packet->data + offset) + 1;
				memcpy(&procedure->instructions[i].instruction.unop.op, packet->data + offset, sizeof(unary_op_t));
				offset += sizeof(unary_op_t);
				procedure->instructions[i].instruction.unop.result = proc_strdup(packet->data + offset);
				offset += strlen(packet->data + offset) + 1;
				break;
			case PROC_BINOP:
				procedure->instructions[i].instruction.binop.param_a = proc_strdup(packet->data + offset);
				offset += strlen(packet->data + offset) + 1;
				memcpy(&procedure->instructions[i].instruction.binop.op, packet->data + offset, sizeof(binary_op_t));
				offset += sizeof(binary_op_t);
				procedure->instructions[i].instruction.binop.param_b = proc_strdup(packet->data + offset);
				offset += strlen(packet->data + offset) + 1;
				procedure->instructions[i].instruction.binop.result = proc_strdup(packet->data + offset);
				offset += strlen(packet->data + offset) + 1;
				break;
			case PROC_CALL:
				memcpy(&procedure->instructions[i].instruction.call.procedure_slot, packet->data + offset, sizeof(uint8_t));
				offset += sizeof(uint8_t);
				break;
			case PROC_NOOP:
				break;
			default:
				printf("Unknown instruction type %d\n", procedure->instructions[i].type);
				return -1;
		}
	}

	return 0;
}

void proc_free_instruction(proc_instruction_t * instruction) {
	switch (instruction->type) {
		case PROC_BLOCK:
		case PROC_IFELSE:
			proc_free(instruction->instruction.block.param_a);
			proc_free(instruction->instruction.block.param_b);
			break;
		case PROC_SET:
			proc_free(instruction->instruction.set.param);
			proc_free(instruction->instruction.set.value);
			break;
		case PROC_UNOP:
			proc_free(instruction->instruction.unop.param);
			proc_free(instruction->instruction.unop.result);
			break;
		case PROC_BINOP:
			proc_free(instruction->instruction.binop.param_a);
			proc_free(instruction->instruction.binop.param_b);
			proc_free(instruction->instruction.binop.result);
			break;
		case PROC_CALL:
		case PROC_NOOP:
			break;
		default:
			printf("Unknown instruction type %d\n", instruction->type);
			break;
	}
}

void free_proc(proc_t * procedure) {
	for (int i = 0; i < procedure->instruction_count; i++) {
		proc_free_instruction(&procedure->instructions[i]);
	}
	proc_free(procedure);
}

int proc_copy_instruction(proc_instruction_t * instruction, proc_instruction_t * copy) {
	if (instruction == NULL || copy == NULL) {
		printf("proc_copy_instruction: instruction or copy is NULL\n");
		return -1;
	}

	copy->node = instruction->node;
	copy->type = instruction->type;

	switch (instruction->type) {
		case PROC_BLOCK:
		case PROC_IFELSE:
			copy->instruction.block.param_a = proc_strdup(instruction->instruction.block.param_a);
			copy->instruction.block.param_b = proc_strdup(instruction->instruction.block.param_b);
			copy->instruction.block.op = instruction->instruction.block.op;
			break;
		case PROC_SET:
			copy->instruction.set.param = proc_strdup(instruction->instruction.set.param);
			copy->instruction.set.value = proc_strdup(instruction->instruction.set.value);
			break;
		case PROC_UNOP:
			copy->instruction.unop.param = proc_strdup(instruction->instruction.unop.param);
			copy->instruction.unop.result = proc_strdup(instruction->instruction.unop.result);
			copy->instruction.unop.op = instruction->instruction.unop.op;
			break;
		case PROC_BINOP:
			copy->instruction.binop.param_a = proc_strdup(instruction->instruction.binop.param_a);
			copy->instruction.binop.param_b = proc_strdup(instruction->instruction.binop.param_b);
			copy->instruction.binop.result = proc_strdup(instruction->instruction.binop.result);
			copy->instruction.binop.op = instruction->instruction.binop.op;
			break;
		case PROC_CALL:
			copy->instruction.call.procedure_slot = instruction->instruction.call.procedure_slot;
			break;
		case PROC_NOOP:
			break;
		default:
			printf("Unknown instruction type %d\n", instruction->type);
			return -1;  // Unknown instruction type
	}

	return 0;  // Success
}

int deepcopy_proc(proc_t * original, proc_t * copy) {
	if (original == NULL || copy == NULL) {
		printf("deepcopy_proc: original or copy is NULL\n");
		return -1;
	}

	copy->instruction_count = original->instruction_count;

	for (int i = 0; i < original->instruction_count; i++) {
		if (proc_copy_instruction(&original->instructions[i], &copy->instructions[i]) != 0) {
			printf("Failed to copy instruction %d\n", i);
			return -1;
		}
	}

	return 0;
}
