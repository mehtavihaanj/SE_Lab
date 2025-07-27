/**************************************************************************
 * C S 429 architecture emulator
 * 
 * instr_Writeback.c - Writeback stage of instruction processing pipeline.
 **************************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err_handler.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include "hw_elts.h"

#define SP_NUM 31
#define XZR_NUM 32
extern machine_t guest;
extern mem_status_t dmem_status;

extern int64_t W_wval;

/*
* Write-back stage logic.
* STUDENT TO-DO:
* Implement the writeback stage.
* 
* Use in as the input pipeline register.
* 
* You will need the global variable W_wval.
*/
comb_logic_t wback_instr(w_instr_impl_t *in) {

    W_wval = in->val_ex;
    if (in->op == OP_BL) {
      guest.proc->GPR[30] = W_wval;
    }
    else if (in->W_sigs.w_enable) {
     
        if (in->dst < SP_NUM) {
          guest.proc->GPR[in->dst] = W_wval;
        }
        else if (in->dst == SP_NUM) {
          guest.proc->SP = W_wval;
        }
    }
  //  else if (in->op != OP_NOP && in->op != OP_HLT
  //       && in->op != OP_ERROR && in->op != OP_RET
  //       && in->op != OP_TST_RR && in->op != OP_CMN_RR
  //       && in->op != OP_CMP_RR && in->op != OP_B_COND 
  //       && in->op != OP_B && in->op != OP_BL && in->op != OP_LDUR && 
  //       in->op != OP_STUR) {
  //    
  //      if (in->dst < SP_NUM) {
  //        guest.proc->GPR[in->dst] = in->val_ex;
  //      }
  //      else if (in->dst == SP_NUM) {
  //        guest.proc->SP = in->val_ex;
  //      }
  //      
  //  }
     if (in->op == OP_LDUR) {
      if (in->dst < SP_NUM) {
        guest.proc->GPR[in->dst] = in->val_mem;  
      }
      else if (in->dst == SP_NUM) {
        guest.proc->SP = in->val_mem;
      }
      W_wval = in->val_mem;
    }
}