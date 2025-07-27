/**************************************************************************
 * C S 429 system emulator
 *
 * Bubble and stall checking logic.
 * STUDENT TO-DO:
 * Implement logic for hazard handling.
 *
 * handle_hazards is called from proc.c with the appropriate
 * parameters already set, you must implement the logic for it.
 *
 * You may optionally use the other three helper functions to
 * make it easier to follow your logic.
 **************************************************************************/

#include "hazard_control.h"
#include "machine.h"
 
extern machine_t guest;
extern mem_status_t dmem_status;
 
/* Use this method to actually bubble/stall a pipeline stage.
 * Call it in handle_hazards(). Do not modify this code. */
void pipe_control_stage(proc_stage_t stage, bool bubble, bool stall) {
  pipe_reg_t *pipe;
  switch (stage) {
  case S_FETCH:
    pipe = F_instr;
    break;
  case S_DECODE:
    pipe = D_instr;
    break;
  case S_EXECUTE:
    pipe = X_instr;
    break;
  case S_MEMORY:
    pipe = M_instr;
    break;
  case S_WBACK:
    pipe = W_instr;
    break;
  default:
    printf("Error: incorrect stage provided to pipe control.\n");
    return;
  }
  if (bubble && stall) {
    printf("Error: cannot bubble and stall at the same time.\n");
    pipe->ctl = P_ERROR;
  }
  // If we were previously in an error state, stay there.
  if (pipe->ctl == P_ERROR)
    return;

  if (bubble) {
    pipe->ctl = P_BUBBLE;
  } else if (stall) {
    pipe->ctl = P_STALL;
  } else {
    pipe->ctl = P_LOAD;
  }
}
 
bool check_ret_hazard(opcode_t D_opcode ) {
  return D_opcode == OP_RET;
}
 
 #ifdef EC
bool check_br_hazard(opcode_t D_opcode ) {
  return false;
}
 
bool check_cb_hazard(opcode_t D_opcode , uint64_t D_val_a) {
  return false;
}
 #endif
 
bool check_mispred_branch_hazard(opcode_t X_opcode, bool X_condval) {

  if (X_opcode == OP_B_COND && !X_condval) {
    return true;
  }
  // Student TODO
  return false;
}
 
bool check_load_use_hazard(opcode_t D_opcode , uint8_t D_src1, uint8_t D_src2,
                            opcode_t X_opcode, uint8_t X_dst) {
  // Student TODO
  bool rr_instr = D_opcode == OP_ADDS_RR || D_opcode == OP_CMN_RR || D_opcode == OP_CMP_RR ||D_opcode == OP_SUBS_RR || D_opcode == OP_ANDS_RR
	|| D_opcode == OP_TST_RR || D_opcode == OP_ORR_RR || D_opcode == OP_EOR_RR || D_opcode == OP_MVN || D_opcode == OP_STUR;

	bool write_enable = D_opcode == OP_ADDS_RR || D_opcode == OP_SUB_RI || D_opcode == OP_ANDS_RR || D_opcode == OP_ADD_RI
	|| D_opcode == OP_ORR_RR || D_opcode == OP_EOR_RR || D_opcode == OP_SUBS_RR || D_opcode == OP_BL || D_opcode == OP_MOVZ 
	|| D_opcode == OP_MOVK || D_opcode == OP_MVN || D_opcode == OP_ADRP || D_opcode == OP_LSL || D_opcode == OP_LSR || D_opcode == OP_ASR;

  bool dstUsage = rr_instr || write_enable;
  if ((X_opcode == OP_LDUR && dstUsage && (X_dst == D_src1 || X_dst == D_src2)))
  {
    return true;
  }
  return false;
}
 
bool error(stat_t status) {
  // Student TODO
  return status != STAT_AOK && status != STAT_BUB;
}
 
comb_logic_t handle_hazards(opcode_t D_opcode , uint8_t D_src1, uint8_t D_src2,
                             uint64_t D_val_a, opcode_t X_opcode, uint8_t X_dst,
                             bool X_condval) {
  /* Students: Change this code */
  // This will need to be updated in week 3, good enough for week 1-2
 bool retBubble = check_ret_hazard(D_opcode );
 bool mispredBranchHazard = check_mispred_branch_hazard(X_opcode, X_condval);
 bool loadUseHazard = check_load_use_hazard(D_opcode , D_src1, D_src2, X_opcode, X_dst);

 bool fetchError =  false;//error(D_in->status);
 bool decodeError = error(X_in->status);
 bool execError =   error(M_in->status);
 bool memError =    error(W_in->status);
 bool wbError =     error(W_out->status);
 bool memFlightError = false;
 if (dmem_status == IN_FLIGHT) {
 // dmem_status =
  memFlightError = true;
 }
 // stall memory and before, and bubble writeback}
 
 if (decodeError) {
  fetchError = true;
 }

 else if (execError) {
  fetchError = true;
  decodeError = true;
  
 }

 else if (memError) {
  fetchError = true;
  decodeError = true;
  execError = true;
 }

 else if (wbError) {
  fetchError = true;
  decodeError = true;
  execError = true;
  memError = true;
 }
              
  

#ifdef PIPE
//bool f_stall = F_out->status == STAT_HLT || F_out->status == STAT_INS;
// void pipe_control_stage(proc_stage_t stage, bool bubble, bool stall) {

pipe_control_stage(S_FETCH, false, retBubble || loadUseHazard || fetchError || memFlightError);//f_stall);
pipe_control_stage(S_DECODE, (retBubble || mispredBranchHazard) && !decodeError && !memFlightError, loadUseHazard || decodeError
|| memFlightError);
pipe_control_stage(S_EXECUTE, (mispredBranchHazard || loadUseHazard) && !execError, execError
|| memFlightError);
pipe_control_stage(S_MEMORY, false, memError
  || memFlightError);
pipe_control_stage(S_WBACK, memFlightError, wbError);  
  
#else
 // removing for now, check if we need to comment it back in later
  bool f_stall = F_out->status == STAT_HLT || F_out->status == STAT_INS;
  pipe_control_stage(S_FETCH, false, f_stall);
  pipe_control_stage(S_DECODE, false, false);
  pipe_control_stage(S_EXECUTE, false, false);
  pipe_control_stage(S_MEMORY, false, false);
  pipe_control_stage(S_WBACK, false, false);                         
#endif
}