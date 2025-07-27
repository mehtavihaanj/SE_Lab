/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Execute.c - Execute stage of instruction processing pipeline.
 **************************************************************************/

 #include <stdint.h>
 #include <stdbool.h>
 #include <assert.h>
 #include "instr.h"
 #include "instr_pipeline.h"
 #include "machine.h"
 #include "hw_elts.h"
 
 extern machine_t guest;
 extern mem_status_t dmem_status;
 
 extern comb_logic_t copy_m_ctl_sigs(m_ctl_sigs_t *, m_ctl_sigs_t *);
 extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);
 
 /*
	* Execute stage logic.
	* STUDENT TO-DO:
	* Implement the execute stage.
	*
	* Use in as the input pipeline register,
	* and update the out pipeline register as output.
	*
	* You will need the following helper functions:
	* copy_m_ctl_signals, copy_w_ctl_signals, and alu.
	*/
 
 comb_logic_t execute_instr(x_instr_impl_t *in, m_instr_impl_t *out) {
	 // Student TODO
	 uint64_t vala = in->val_a;
	 uint64_t valb = 0;
	 if (in->op == OP_ADDS_RR || in->op == OP_CMN_RR
		|| in->op == OP_SUBS_RR || in->op == OP_ANDS_RR 
		|| in->op == OP_TST_RR || in->op == OP_MVN || in->op == OP_CMP_RR || 
		in->op == OP_ORR_RR || in->op == OP_EOR_RR) {
	 	valb = in->val_b;
	 }
	 else if (in->op == OP_ADD_RI || in->op == OP_SUB_RI 
		|| in->op == OP_MOVZ || in->op == OP_MOVK || in->op == OP_STUR 
		|| in->op == OP_LDUR || in->op == OP_ADRP || in->op == OP_LSL 
		|| in->op == OP_LSR || in->op == OP_ASR) {
		valb = in->val_imm;
	 }


	 
	 alu_op_t aluOp = in->ALU_op;
	 cond_t condition = in->cond;
	 uint64_t outputValue = 0;
	 bool conditionValue = true;
	 uint8_t nzcv = guest.proc->NZCV;

	bool setFlags = in->X_sigs.set_flags;
	if (in->op != OP_NOP && in->op != OP_HLT) {
	 alu(vala, valb, in->val_hw, aluOp, setFlags, 
		condition, &outputValue, &conditionValue, &nzcv);

		if (in->op != OP_ERROR) {
			out->cond_holds = conditionValue;
		}
	}
	

	if (setFlags) {
		guest.proc->NZCV = nzcv;
	}
	
	out->dst = in->dst;
	out->op = in->op;
	out->print_op = in->print_op;
	out->status = in->status;
	out->val_b = in->val_b;
	out->val_ex = outputValue;

	if (in->op == OP_BL) {
		out->val_ex = in->seq_succ_PC;
	}
	out->seq_succ_PC = in->seq_succ_PC;
	copy_m_ctl_sigs(&out->M_sigs, &in->M_sigs);
	copy_w_ctl_sigs(&out->W_sigs, &in->W_sigs);
 }