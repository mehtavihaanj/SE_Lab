/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Fetch.c - Fetch stage of instruction processing pipeline.
 **************************************************************************/

#include "hw_elts.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

extern machine_t guest;
extern mem_status_t dmem_status;

/*
 * Select PC logic.
 * STUDENT TO-DO:
 * Write the next PC to *current_PC.
 */

static comb_logic_t
select_PC(uint64_t pred_PC,                  // The predicted PC
          opcode_t D_opcode, uint64_t val_a, // Possible correction from RET
          uint64_t D_seq_succ,               // this is only used in CBZ/CBNZ EC
          opcode_t M_opcode, bool M_cond_val, // b.cond correction
          uint64_t seq_succ, // Possible correction from B.cond
          uint64_t *current_PC) {
  /*
   * Students: Please leave this code
   * at the top of this function.
   * You may modify below it.
   */
  if (D_opcode == OP_RET && val_a == RET_FROM_MAIN_ADDR) {
    *current_PC = 0; // PC can't be 0 normally.
    return;
  }
 
  *current_PC = pred_PC;
  if (!M_cond_val && M_opcode == OP_B_COND) {
    *current_PC = seq_succ;
  }
  else if (D_opcode == OP_RET) {
    *current_PC = val_a;
  }  
}

/*
 * Predict PC logic. Conditional branches are predicted taken.
 * STUDENT TO-DO:
 * Write the predicted next PC to *predicted_PC
 * and the next sequential pc to *seq_succ.
 */

static comb_logic_t predict_PC(uint64_t current_PC, uint32_t insnbits,
                               opcode_t op, uint64_t *predicted_PC,
                               uint64_t *seq_succ) {
  /*
   * Students: Please leave this code
   * at the top of this function.
   * You may modify below it.
   */
  if (!current_PC) {
    return; // We use this to generate a halt instruction.
  }
  
  *seq_succ = current_PC + 4;
  int64_t offset = 0;

  switch (op) {
    case OP_B:
      offset = bitfield_s64(insnbits, 0, 26) * 4;
      break;
    case OP_BL:
      offset = bitfield_s64(insnbits, 0, 26) * 4;
     // guest.proc->GPR[30] = current_PC + 4;
      break;
    case OP_B_COND:
      offset = bitfield_s64(insnbits, 5, 19) * 4;
      break;
    default:
      offset = 4;
  }
  *predicted_PC = current_PC + offset;
}

/*
 * Helper function to recognize the aliased instructions:
 * LSL, LSR, CMP, CMN, and TST. We do this only to simplify the
 * implementations of the shift operations (rather than having
 * to implement UBFM in full).
 * STUDENT TO-DO
 */

static void fix_instr_aliases(uint32_t insnbits, opcode_t *op) {

  uint32_t opCodeBits = bitfield_u32(insnbits, 21, 11);
  opcode_t resultOp = itable[opCodeBits];

  if (resultOp == OP_UBFM) {
    
    uint32_t diffBits = bitfield_u32(insnbits, 10, 6);
    uint32_t immr = bitfield_u32(insnbits, 16, 6);

    if (!(diffBits ^ 0x3F)) {
      *op = OP_LSR;
    }
    else if (diffBits + 1 == immr) {
      *op  = OP_LSL;
    }
    else {
      printf("ERROR - UBFM did not decode into LSL or LSR.");
      assert(0);
    }
  }
  else if (resultOp == OP_SUBS_RR) {
    uint32_t regBits = bitfield_u32(insnbits, 0, 5);
    if (!(regBits ^ 0x1F)) {
      *op = OP_CMP_RR;
    }
  }
  else if (resultOp == OP_ADDS_RR) {
    uint32_t regBits = bitfield_u32(insnbits, 0, 5);
    if (!(regBits ^ 0x1F)) {
      *op = OP_CMN_RR;
    }
  }
  else if (resultOp == OP_ANDS_RR) {
    uint32_t regBits = bitfield_u32(insnbits, 0, 5);
    if (!(regBits ^ 0x1F)) {
      *op = OP_TST_RR;
    }
  }
}

/*
 * Fetch stage logic.
 * STUDENT TO-DO:
 * Implement the fetch stage.
 *
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 * Additionally, update PC for the next
 * cycle's predicted PC.
 *
 * You will also need the following helper functions:
 * select_pc, predict_pc, and imem.
 */

comb_logic_t fetch_instr(f_instr_impl_t *in, d_instr_impl_t *out) {

  
  uint64_t current_PC = 0;
  select_PC(in->pred_PC, X_out->op, X_out->val_a, X_out->seq_succ_PC, M_out->op, M_out->cond_holds, 
  M_out->seq_succ_PC, &current_PC);
  bool imem_err = 0;

  /*
   * Students: This case is for generating HLT instructions
   * to stop the pipeline. Only write your code in the **else** case.
   */
  if (!current_PC || F_in->status == STAT_HLT) {
    if (F_in->status == STAT_HLT) {
    printf("halting....\n"); 
  }
    out->insnbits = 0xD4400000U;
    out->op = OP_HLT;
    out->print_op = OP_HLT;
    imem_err = false;
  } else {
    uint32_t instruction = 0;    

    // Get instruction from current PC
    imem(current_PC, &instruction, &imem_err);

    // currentOp is bitfield_u32(instruction, 21, 11)
    opcode_t resultOp = itable[bitfield_u32(instruction, 21, 11)];
    fix_instr_aliases(instruction, &resultOp);

    uint64_t predictedPCVar = 0;

    // Get predicted PC value, set seq_succ and the current pc
    predict_PC(current_PC, instruction, resultOp, &predictedPCVar, &out->multipurpose_val.seq_succ_PC);
    guest.proc->PC = predictedPCVar;   
    out->op = resultOp;
    out->insnbits = instruction;
    out->print_op = resultOp;
    
    if (resultOp == OP_ADRP) {
      out->multipurpose_val.adrp_val = current_PC & ~0xFFF;
    }
  }

  if (imem_err || out->op == OP_ERROR) {
    in->status = STAT_INS;
    F_in->status = in->status;
  } else if (out->op == OP_HLT) {
    in->status = STAT_HLT;
    F_in->status = in->status;
  } else {
    in->status = STAT_AOK;
  }
  out->status = in->status;

  return;
}
