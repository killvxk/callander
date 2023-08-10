#ifndef X86_H
#define X86_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "x86_64_length_disassembler.h"

typedef int16_t x86_int16 __attribute__((aligned(1)));
typedef uint16_t x86_uint16 __attribute__((aligned(1)));
typedef int32_t x86_int32 __attribute__((aligned(1)));
typedef uint32_t x86_uint32 __attribute__((aligned(1)));
typedef int64_t x86_int64 __attribute__((aligned(1)));
typedef uint64_t x86_uint64 __attribute__((aligned(1)));

// x86_is_syscall_instruction checks if the instruction at address is a syscall
bool x86_is_syscall_instruction(const uint8_t *addr);

// x86_is_nop_instruction checks if the instruction at address is a nop
bool x86_is_nop_instruction(const uint8_t *addr);

struct x86_ins_prefixes {
	bool has_lock:1;
	bool has_repne:1;
	bool has_rep:1;
	bool has_w:1;
	bool has_r:1;
	bool has_x:1;
	bool has_b:1;
	bool has_any_rex:1;
	bool has_segment_override:1;
	bool has_notrack:1;
	bool has_operand_size_override:1;
	bool has_address_size_override:1;
	// bool has_taken_hint:1;
	// bool has_not_taken_hint:1;
	// bool has_three_byte_vex:1;
	bool has_vex:1;
	// bool has_xop:1;
};

__attribute__((always_inline))
__attribute__((nonnull(1)))
static inline struct x86_ins_prefixes x86_decode_ins_prefixes(const uint8_t **ins) {
	struct x86_ins_prefixes result = { 0 };
	if (**ins == 0x3e) {
		// notrack prefix for CET. not used
		result.has_notrack = true;
		++(*ins);
	}
#pragma GCC unroll 16
	for (int i = 0; i < 16; i++) {
		uint8_t value = **ins;
		if (value == 0xf0) {
			// lock prefix
			result.has_lock = true;
		} else if (value == 0xf2) {
			// repne prefix
			result.has_repne = true;
		} else if (value == 0xf3) {
			// rep prefix
			result.has_rep = true;
		} else if ((value & 0xf0) == 0x40) {
			// rex prefix
			result.has_w = (value & 0x8) != 0;
			result.has_r = (value & 0x4) != 0;
			result.has_x = (value & 0x2) != 0;
			result.has_b = (value & 0x1) != 0;
			result.has_any_rex = true;
		} else if (value == 0x66) {
			// operand size override
			result.has_operand_size_override = true;
		} else if (value == 0x67) {
			// address size override
			result.has_address_size_override = true;
			break;
		} else if (value == 0x2e) {
			// cs segment override
			result.has_segment_override = true;
		} else if (value == 0x36) {
			// ss segment override
			result.has_segment_override = true;
		} else if (value == 0x3e) {
			// ds segment override
			result.has_segment_override = true;
		} else if (value == 0x26) {
			// es segment override
			result.has_segment_override = true;
		} else if (value == 0x64) {
			// fs segment override
			result.has_segment_override = true;
		} else if (value == 0x65) {
			// gs segment override
			result.has_segment_override = true;
		} else if (value == 0x2e) {
			// not taken hint
			// result.has_not_taken_hint = true;
		} else if (value == 0x3e) {
			// taken hint
			// result.has_taken_hint = true;
		} else if (value == 0xc4) {
			// three byte vex
			result.has_vex = true;
			// skip two data bytes
			++(*ins);
			++(*ins);
		} else if (value == 0xc5) {
			// two byte vex
			result.has_vex = true;
			// skip one data bytes
			++(*ins);
		} else if (value == 0x8f) {
			// three-byte xop
			// result.has_xop = true;
			// skip two data bytes
			++(*ins);
			++(*ins);
		} else {
			break;
		}
		++(*ins);
	}
	return result;
}

struct x86_instruction {
	const uint8_t *unprefixed;
	int length;
	struct x86_ins_prefixes prefixes;
};

__attribute__((always_inline))
static inline bool x86_decode_instruction(const uint8_t *addr, struct x86_instruction *out_ins)
{
	int length = InstructionSize_x86_64(addr, 0xf);
	out_ins->length = length;
	out_ins->unprefixed = addr;
	if (length == INSTRUCTION_INVALID) {
		out_ins->prefixes = (struct x86_ins_prefixes){ 0 };
		return false;
	}
	out_ins->prefixes = x86_decode_ins_prefixes(&out_ins->unprefixed);
	return true;
}

static inline bool x86_is_endbr64_instruction(const struct x86_instruction *ins)
{
	return ins->prefixes.has_rep && ins->unprefixed[0] == 0x0f && ins->unprefixed[1] == 0x1e && ins->unprefixed[2] == 0xfa;
}

static inline const uint8_t *x86_next_instruction(const uint8_t *addr, const struct x86_instruction *ins)
{
	return addr + ins->length;
}

// x86_is_return_instruction checks if the instruction is a return
static inline bool x86_is_return_instruction(struct x86_instruction *ins)
{
	switch (*ins->unprefixed) {
		case 0xc3: // ret
		case 0xc2: // ret imm16
		case 0xcb: // far ret
		case 0xca: // far ret imm16
			return true;
		default:
			return false;
	}
}

// x86_decode_jump_instruction determines if an instruction jumps, and
// fills the jump target
__attribute__((warn_unused_result))
__attribute__((nonnull(1, 2)))
enum ins_jump_behavior x86_decode_jump_instruction(const struct x86_instruction *ins, const uint8_t **out_jump);

static inline bool x86_is_jo_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x70 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x80);
}

static inline bool x86_is_jno_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x71 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x81);
}

static inline bool x86_is_jb_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x72 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x82);
}

static inline bool x86_is_jae_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x73 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x83);
}

static inline bool x86_is_je_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x74 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x84);
}

static inline bool x86_is_jne_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x75 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x85);
}

static inline bool x86_is_jbe_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x76 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x86);
}

static inline bool x86_is_ja_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x77 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x87);
}

static inline bool x86_is_js_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x78 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x88);
}

static inline bool x86_is_jns_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x79 || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x89);
}

static inline bool x86_is_jp_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x7a || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x8a);
}

static inline bool x86_is_jpo_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x7b || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x8b);
}

static inline bool x86_is_jl_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x7c || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x8c);
}

static inline bool x86_is_jge_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x7d || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x8d);
}

static inline bool x86_is_jng_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x7e || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x8e);
}

static inline bool x86_is_jg_instruction(const struct x86_instruction *ins)
{
	return *ins->unprefixed == 0x7f || (*ins->unprefixed == 0x0f && ins->unprefixed[1] == 0x8f);
}

typedef struct {
    uint8_t rm : 3;
    uint8_t reg : 3;
    uint8_t mod : 2;
} x86_mod_rm_t;

static inline x86_mod_rm_t x86_read_modrm(const uint8_t *byte) {
	union {
		uint8_t byte;
		x86_mod_rm_t modrm;
	} pun;
	pun.byte = *byte;
	return pun.modrm;
}

enum x86_register_index {
	X86_REGISTER_AX,
	X86_REGISTER_CX,
	X86_REGISTER_DX,
	X86_REGISTER_BX,
	X86_REGISTER_SP,
	X86_REGISTER_BP,
	X86_REGISTER_SI,
	X86_REGISTER_DI,
	X86_REGISTER_8,
	X86_REGISTER_9,
	X86_REGISTER_10,
	X86_REGISTER_11,
	X86_REGISTER_12,
	X86_REGISTER_13,
	X86_REGISTER_14,
	X86_REGISTER_15,
};

static inline int x86_read_reg(x86_mod_rm_t modrm, struct x86_ins_prefixes rex) {
	return modrm.reg + (rex.has_r << 3);
}

static inline int x86_read_rm(x86_mod_rm_t modrm, struct x86_ins_prefixes rex) {
	return modrm.rm + (rex.has_b << 3);
}

__attribute__((always_inline))
static inline bool x86_modrm_is_direct(x86_mod_rm_t modrm) {
	return modrm.mod == 3;
}

typedef struct {
    uint8_t base : 3;
    uint8_t index : 3;
    uint8_t scale : 2;
} x86_sib_t;

static inline x86_sib_t x86_read_sib(const uint8_t *byte) {
	union {
		uint8_t byte;
		x86_sib_t sib;
	} pun;
	pun.byte = *byte;
	return pun.sib;
}

static inline int x86_read_base(x86_sib_t sib, struct x86_ins_prefixes rex) {
	return sib.base + (rex.has_b << 3);
}

static inline int x86_read_index(x86_sib_t sib, struct x86_ins_prefixes rex) {
	return sib.index + (rex.has_x << 3);
}

static inline int x86_read_opcode_register_index(uint8_t opcode_value, uint8_t opcode_start, struct x86_ins_prefixes rex)
{
	return opcode_value - opcode_start + ((int)rex.has_b << 3);
}

#endif
