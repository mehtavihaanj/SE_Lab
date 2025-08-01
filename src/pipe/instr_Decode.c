/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Decode.c - Decode stage of instruction processing pipeline.
 **************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "instr.h"
#include "instr_pipeline.h"
#include "forward.h"
#include "machine.h"
#include "hw_elts.h"

#include "pthread.h"

#define SP_NUM 31
#define XZR_NUM 32

extern machine_t guest;
extern mem_status_t dmem_status;
extern pthread_barrier_t cycle_end;
extern pthread_barrier_t latch_end;

extern int64_t W_wval;
//set wwval in write back based on what value (val x or valm), set as parameter in decode
/*
   * Control signals for D, X, M, and W stages.
   * Generated by D stage logic.
   * D control signals are consumed locally.
   * Others must be buffered in pipeline registers.
   * STUDENT TO-DO:
   * Generate the correct control signals for this instruction's
   * future stages and write them to the corresponding struct.
   */
static comb_logic_t generate_DXMW_control(opcode_t op,
	x_ctl_sigs_t* X_sigs,
	m_ctl_sigs_t* M_sigs,
	w_ctl_sigs_t* W_sigs) {
	
	bool rr_instr = op == OP_ADDS_RR || op == OP_CMN_RR || op == OP_CMP_RR ||op == OP_SUBS_RR || op == OP_ANDS_RR
	|| op == OP_TST_RR || op == OP_ORR_RR || op == OP_EOR_RR || op == OP_MVN;

	bool write_enable = op == OP_ADDS_RR || op == OP_SUB_RI || op == OP_ANDS_RR || op == OP_ADD_RI
	|| op == OP_ORR_RR || op == OP_EOR_RR || op == OP_SUBS_RR || op == OP_BL || op == OP_MOVZ 
	|| op == OP_MOVK || op == OP_MVN || op == OP_ADRP || op == OP_LSL || op == OP_LSR || op == OP_ASR || op == OP_LDUR;


	M_sigs->dmem_write = op == OP_STUR;
	M_sigs->dmem_read = op == OP_LDUR;
	
	X_sigs->valb_sel = rr_instr;
	X_sigs->set_flags = rr_instr & !(op == OP_ORR_RR || op == OP_EOR_RR || op == OP_MVN);

	W_sigs->dst_sel = op == OP_BL;
	W_sigs->w_enable = write_enable;
	W_sigs->wval_sel = op == OP_LDUR;
}


/*
   * Logic for extracting the immediate value for M-, I-, and RI-format instructions.
   * STUDENT TO-DO:
   * Extract the immediate value and write it to *imm.
   */
static comb_logic_t extract_immval(uint32_t insnbits, opcode_t op,
	int64_t* imm) {
	
	switch (op) {
		case OP_ADD_RI:
		case OP_SUB_RI: {
			int64_t immVal = (int64_t)bitfield_u32(insnbits, 10, 12);
		    *imm = immVal;
			break;
		}

		case OP_MOVZ:
		case OP_MOVK: {
			*imm = (int64_t) bitfield_u32(insnbits, 5, 16);
			break;
		}
		case OP_ADRP: {
			uint64_t higherBits = (uint64_t) bitfield_s64(insnbits, 5, 19);
			uint64_t lowerBits = (uint64_t) bitfield_s64(insnbits, 29, 2);
			uint64_t first21 = (higherBits << 2) | lowerBits;
		//	int64_t signExtendedBits = ((int64_t)first21 << 43) >> 43;
		
			int64_t offset = first21 * 4096;
			*imm = offset;
			break;
		}
		case OP_LSL: {
			uint64_t immr = bitfield_u32(insnbits, 16, 6);
			*imm = (64 - immr) % 64;
			break;
		}
		case OP_LSR:
			*imm = bitfield_u32(insnbits, 16, 6);
			break;
		case OP_ASR: {
			*imm = bitfield_u32(insnbits, 9, 7);
			break;
		}
		default: {
			break;
		}

	}
}

/*
   * Logic for determining the ALU operation needed for this opcode.
   * STUDENT TO-DO:
   * Determine the ALU operation based on the given opcode
   * and write it to *ALU_op.
   */
static comb_logic_t decide_alu_op(opcode_t op, alu_op_t* ALU_op) {
	// Student TODO

	
	if (op == OP_ADDS_RR || op == OP_ADD_RI || op == OP_CMN_RR 
		|| op == OP_STUR || op == OP_LDUR || op == OP_ADRP) {
		*ALU_op = PLUS_OP;
	}
	else if (op == OP_NOP || op == OP_ERROR || op == OP_RET 
		|| op == OP_HLT || op == OP_B_COND || op == OP_B || op == OP_BL) {
		*ALU_op = PASS_A_OP;
	}
	else if (op == OP_MOVZ || op == OP_MOVK) {
		*ALU_op = MOV_OP;
	}
	else if (op == OP_MVN) {
		*ALU_op = INV_OP;
	}
	else if (op == OP_SUBS_RR || op == OP_SUB_RI || op == OP_CMP_RR) {
		*ALU_op = MINUS_OP;
	}
	else if (op == OP_ANDS_RR || op == OP_TST_RR) {
		*ALU_op = AND_OP;
	}
	else if (op == OP_ORR_RR) {
		*ALU_op = OR_OP;
	}
	else if (op == OP_EOR_RR) {
		*ALU_op = EOR_OP;
	}
	else if (op == OP_LSL)
	{
		*ALU_op = LSL_OP;
	}
	else if (op == OP_LSR)
	{
		*ALU_op = LSR_OP;
	}
	else if (op == OP_ASR)
	{
		*ALU_op = ASR_OP;
	}
}

/*
   * Utility functions for copying over control signals across a stage.
   * STUDENT TO-DO:
   * Copy the input signals from the input side of the pipeline
   * register to the output side of the register.
   */

comb_logic_t copy_m_ctl_sigs(m_ctl_sigs_t* dest, m_ctl_sigs_t* src) {
	dest->dmem_read = src->dmem_read;
	dest->dmem_write = src->dmem_write;
}

comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t* dest, w_ctl_sigs_t* src) {
	dest->dst_sel = src->dst_sel;
	dest->w_enable = src->w_enable;
	dest->wval_sel = src->wval_sel;
}

comb_logic_t extract_regs(uint32_t insnbits, opcode_t op, uint8_t* src1,
	uint8_t* src2, uint8_t* dst) {


	if (op == OP_ADDS_RR || op == OP_CMN_RR ||
		op == OP_SUBS_RR || op == OP_ANDS_RR
		|| op == OP_TST_RR || op == OP_CMP_RR || op == OP_ORR_RR || op == OP_EOR_RR) {

		uint8_t source1 = bitfield_u32(insnbits, 5, 5); // Xn, the first GPR 
		uint8_t source2 = bitfield_u32(insnbits, 16, 5); // Xm, the second GPR
		uint8_t destination = bitfield_u32(insnbits, 0, 5); // Destination register


		*src1 = source1;
		*src2 = source2;
		*dst = destination;
		
		// TST, CMP, ORR, and EOR cannot have SP as operand
		if (*src1 == SP_NUM && (op == OP_TST_RR || op == OP_CMP_RR || op == OP_ORR_RR || op == OP_EOR_RR)) {
			*src1 = XZR_NUM;
		}

		if (*src2 == SP_NUM && (op == OP_TST_RR || op == OP_CMP_RR || op == OP_ORR_RR || op == OP_EOR_RR)) {
			*src2 = XZR_NUM;
		}

		if (*dst == SP_NUM && (op == OP_TST_RR || op == OP_CMP_RR || op == OP_ORR_RR || op == OP_EOR_RR)) {
			*dst = XZR_NUM;
		}
//
	//	if (*src2 == SP_NUM) {
	//		*src2 = XZR_NUM;
	//	}
		
	}
	else if (op == OP_ADD_RI || op == OP_SUB_RI || 
		op == OP_LSL || op == OP_LSR || op == OP_ASR) {
		uint8_t source1 = bitfield_u32(insnbits, 5, 5); // Xn, the first GPR 
		uint8_t destination = bitfield_u32(insnbits, 0, 5); // Destination register
		*src1 = source1;
		*dst = destination;

	//	if (*src1 == SP_NUM) {
	//		*src1 = XZR_NUM;
	//	}


	}
	else if (op == OP_MOVZ || op == OP_MOVK) {
		*dst = bitfield_u32(insnbits, 0, 5);
	}
	else if (op == OP_MVN) {
		*dst = bitfield_u32(insnbits, 0, 5); // Destination register
		*src1 = bitfield_u32(insnbits, 16, 5);

		if (*src1 == SP_NUM) {
			*src1 = XZR_NUM;
		}
	}
	else if (op == OP_STUR || op == OP_LDUR) {
		*dst = bitfield_u32(insnbits, 0, 5); // Destination register Xt

		if (*dst == SP_NUM) {
			*dst = XZR_NUM;
		}
		*src1 = bitfield_u32(insnbits, 5, 5); // Base value Xn
		*src2 = bitfield_s64(insnbits, 12, 9); // 9 bit signed offset
	}
	else if (op == OP_ADRP) {
		*dst = bitfield_u32(insnbits, 0, 5);
	}
	else if (op == OP_BL) {
		*dst = 30;
	}
}

/*
   * Decode stage logic.
   * STUDENT TO-DO:
   * Implement the decode stage.
   *
   * Use `in` as the input pipeline register,
   * and update the `out` pipeline register as output.
   * Additionally, make sure the register file is updated
   * with W_out's output when you call it in this stage.
   *
   * You will also need the following helper functions:
   * generate_DXMW_control, regfile, extract_immval,
   * and decide_alu_op.
   */

comb_logic_t decode_instr(d_instr_impl_t* in, x_instr_impl_t* out) {
	
	generate_DXMW_control(in->op, &out->X_sigs, &out->M_sigs, &out->W_sigs);
	uint8_t src1 = 0;
	uint8_t src2 = 0;
	uint8_t dst = 0; 
	uint64_t vala = 0;
	uint64_t valb = 0;
	
	bool dst_sel = out->W_sigs.dst_sel;
	alu_op_t tempOp = ERROR_OP;
	extract_regs(in->insnbits, in->op, &src1, &src2, &dst);
	decide_alu_op(in->op, &tempOp);
	// On stur/ldur, vala becomes the base address to store/load at 
	
	regfile(src1, src2, dst, W_wval, dst_sel, &vala, &valb);

	// TODO: Fix bl_ret
				

	out->ALU_op = tempOp;
	out->op = in->op;
	out->print_op = in->print_op;
	out->status = in->status;
	out->dst = dst;
	out->val_a = vala;
	out->val_b = valb;
	out->val_hw = 0;

	if (in->op == OP_MOVK) {
		out->val_a = guest.proc->GPR[dst];
	}
	uint8_t forwardDest = 0;
	uint8_t forwardSource = 0;
	if (in->op == OP_MOVK) {
		forwardDest = dst;
	}
	else {
		forwardDest = src1;
	}

	if (in->op == OP_STUR || in->op == OP_LDUR) {
		forwardSource = dst;
		forwardDest = src1;
	}
	else {
		forwardSource = src2;
	}

	if (in->op == OP_STUR || in->op == OP_LDUR) {
		out->val_imm = src2;
	//	if (in->op == OP_LDUR) {
		if (dst != XZR_NUM) {
			out->val_b = guest.proc->GPR[dst];
		}
		else {
			out->val_b = 0;;
		}
		
	}
	if (in->op == OP_RET) {
		uint32_t regForReturn = bitfield_u32(in->insnbits, 5, 5);
		forwardDest = regForReturn;
		out->val_a = guest.proc->GPR[regForReturn];
	}
	

	forward_reg(forwardDest, forwardSource, X_out->dst, M_out->dst, W_out->dst, 

		M_in->val_ex, // X_val_ex
		M_out->val_ex, // M_val_ex
		W_in->val_mem, // M_val_mem
		W_out->val_ex, // W_val_ex
		W_out->val_mem, // W_val_mem

		M_out->W_sigs.wval_sel, // M_w_val_sel
		W_out->W_sigs.wval_sel, // W_w_val_sel
		X_out->W_sigs.w_enable, // X_w_enable
		M_out->W_sigs.w_enable, // M_w_enable
		W_out->W_sigs.w_enable, // W_w_enable
		&out->val_a, &out->val_b);

	if (in->op == OP_ADD_RI || in->op == OP_SUB_RI 
		|| in->op == OP_LSL || in->op == OP_LSR || in->op == OP_ASR 
		|| in->op == OP_MOVZ || in->op == OP_MOVK) {

		extract_immval(in->insnbits, in->op, &out->val_imm);
	}

	if (in->op == OP_B_COND) {
		out->cond = bitfield_u32(in->insnbits, 0, 4);
	}
	else if (in->op == OP_MOVZ || in->op == OP_MOVK) {
		uint32_t shiftVal = bitfield_u32(in->insnbits, 21, 2) * 16;
		out->val_hw = shiftVal;
		if (in->op == OP_MOVK) {
			//out->val_a = guest.proc->GPR[dst];
			uint64_t mask = ~(0xFFFFULL << shiftVal);
			out->val_a = out->val_a & mask;
		}
		else {
			out->val_a = 0;
		}
	}
	else if (in->op == OP_MVN) {
		uint32_t shiftVal = bitfield_u32(in->insnbits, 10, 6);
		out->val_hw = shiftVal;
		out->val_b = out->val_a;
		out->val_a = 0;
		//forwardDest = src2;
	}
	
	else if (in->op == OP_ADRP) {
		extract_immval(in->insnbits, in->op, &out->val_imm);
		out->val_a = in->multipurpose_val.adrp_val;
		out->val_b = 0;
	}
	 

	out->seq_succ_PC = in->op != OP_ADRP ? in->multipurpose_val.seq_succ_PC : in->multipurpose_val.adrp_val;
}
