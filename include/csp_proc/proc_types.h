#ifndef CSP_PROC_PROC_TYPES_H
#define CSP_PROC_PROC_TYPES_H

#include <stdint.h>
#include <string.h>

#define MAX_INSTRUCTIONS 255  // one less than 2^8 because instruction_count would overflow back to 0 when instruction buffer is full
#define MAX_PROC_SLOT    255

#ifndef RESERVED_PROC_SLOTS
#define RESERVED_PROC_SLOTS 0
#endif

typedef enum {
	PROC_BLOCK,
	PROC_IFELSE,
	PROC_SET,
	PROC_UNOP,
	PROC_BINOP,
	PROC_CALL,
	PROC_NOOP,
} proc_instruction_type_t;

typedef enum {
	OP_EQ,   // ==
	OP_NEQ,  // !=
	OP_LT,   // <
	OP_GT,   // >
	OP_LE,   // <=
	OP_GE,   // >=
} comparison_op_t;

typedef enum {
	OP_INC,  // ++
	OP_DEC,  // --
	OP_NOT,  // !
	OP_NEG,  // -
	OP_IDT,  // idt (identity - practically a copy operation)
	OP_RMT,  // rmt (remote - same as identity, but <param> is local and <result> is remote)
} unary_op_t;

typedef enum {
	OP_ADD,  // +
	OP_SUB,  // -
	OP_MUL,  // *
	OP_DIV,  // /
	OP_MOD,  // %
	OP_LSH,  // <<
	OP_RSH,  // >>
	OP_AND,  // &
	OP_OR,   // |
	OP_XOR,  // ^
} binary_op_t;

typedef struct {
	char * param_a;
	comparison_op_t op;
	char * param_b;
} proc_block_t, proc_ifelse_t;

typedef struct {
	char * param;
	char * value;
} proc_set_t;

typedef struct {
	char * param;
	unary_op_t op;
	char * result;
} proc_unop_t;

typedef struct {
	char * param_a;
	binary_op_t op;
	char * param_b;
	char * result;
} proc_binop_t;

typedef struct {
	uint8_t procedure_slot;
} proc_call_t;

typedef struct {
	uint16_t node;
	proc_instruction_type_t type;
	union {
		proc_block_t block;
		proc_ifelse_t ifelse;
		proc_set_t set;
		proc_unop_t unop;
		proc_binop_t binop;
		proc_call_t call;
	} instruction;
} proc_instruction_t;

// Note: Using __attribute__((packed)) would be unnecessary given the manual serialization of the struct in proc_pack.c
typedef struct {
	proc_instruction_t instructions[MAX_INSTRUCTIONS];
	uint8_t instruction_count;
} proc_t;

#endif  // CSP_PROC_PROC_TYPES_H
