/**************************************************************************
 * C S 429 system emulator
 * 
 * hw_elts.c - Module for emulating hardware elements.
 * 
 * Copyright (c) 2022, 2023, 2024, 2025. 
 * Authors: S. Chatterjee, Z. Leeper., P. Jamadagni 
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 

#include <assert.h>
#include "hw_elts.h"
#include "mem.h"
#include "machine.h"
#include "err_handler.h"

extern machine_t guest;

comb_logic_t 
imem(uint64_t imem_addr,
     uint32_t *imem_rval, bool *imem_err) {
    // imem_addr must be in "instruction memory" and a multiple of 4
    *imem_err = (!addr_in_imem(imem_addr) || (imem_addr & 0x3U));
    *imem_rval = (uint32_t) mem_read_I(imem_addr);
}

comb_logic_t
regfile(uint8_t src1, uint8_t src2, uint8_t dst, uint64_t val_w,
        bool w_enable,
        uint64_t *val_a, uint64_t *val_b) {
    // Student TODO
    
    if (src1 < 31 && src1 >= 0) {
        *val_a = guest.proc->GPR[src1];
    }
    else if (src1 == 31) {
        *val_a = guest.proc->SP;
    }
    else if (src1 == 32) {
        *val_a = 0;
    }
    
    if (src2 < 31 && src2 >= 0) {
        *val_b = guest.proc->GPR[src2];
    }
    else if (src2 == 31) {
        *val_b = guest.proc->SP;
    }
    else if (src2 == 32) {
        *val_b = 0;
    }
    

    if (w_enable) {
       if (dst < 31) {
           guest.proc->GPR[dst] = val_w;
       }   
       else if (dst == 31) {
           guest.proc->SP = val_w;
       }
    }
}

static bool 
cond_holds(cond_t cond, uint8_t flags) {
    // Student TODO

    bool negativeFlag = (flags & 0x08);
    bool zeroFlag = (flags & 0x04);
    bool carryFlag = (flags & 0x02);
    bool overFlowFlag = (flags & 0x01);


    switch (cond) {
        case C_EQ:
            return zeroFlag;
        case C_NE:
            return !zeroFlag;

        case C_CS:
            return carryFlag;
        case C_CC:
            return !carryFlag;

        case C_MI:
            return negativeFlag;
        case C_PL:
            return !negativeFlag;

        case C_VS:
            return overFlowFlag;
        case C_VC:
            return !overFlowFlag;

        case C_HI:
            return carryFlag && !zeroFlag;
        case C_LS:
            return !(carryFlag && !zeroFlag);

        case C_GE:
            return negativeFlag == overFlowFlag;
        case C_LT:
            return negativeFlag != overFlowFlag;

        case C_GT:
            return !zeroFlag && (negativeFlag == overFlowFlag);
        case C_LE:
            return !(!zeroFlag && (negativeFlag == overFlowFlag));

        case C_AL:
            return true;
        case C_NV:
            return true;
        
        default:
            printf("ERROR IN COND_HOLDS");
            return false;
    }
    
}

comb_logic_t 
alu(uint64_t alu_vala, uint64_t alu_valb, uint8_t alu_valhw, alu_op_t ALUop, bool set_flags, cond_t cond, 
    uint64_t *val_e, bool *cond_val, uint8_t *nzcv) {
    uint64_t res = 0xFEEDFACEDEADBEEF;  // To make it easier to detect errors.
    // Student TODO

   
    switch (ALUop) {
            
        case PLUS_OP:
            *val_e = alu_vala + alu_valb;
            break;
        case MINUS_OP:
            *val_e = alu_vala - alu_valb;
            break;
        case AND_OP:
            *val_e = alu_vala & alu_valb;
            break;

        case INV_OP:
            *val_e = alu_vala | (~alu_valb);
            break;

        case OR_OP:
            *val_e = alu_vala | alu_valb;
            break;
        case EOR_OP:
            *val_e = alu_vala ^ alu_valb;
            break;

        case MOV_OP:
            *val_e = alu_vala | (alu_valb << alu_valhw);
            break;

        case LSL_OP:
            *val_e = alu_vala << (alu_valb & 0x3FUL);
            break;
        case LSR_OP:
            *val_e = alu_vala >> (alu_valb & 0x3FUL);
            break;
        case ASR_OP:
            *val_e = (int64_t) alu_vala >> (alu_valb & 0x3FUL);
            break;

        case PASS_A_OP:
            *val_e = alu_vala;
            *cond_val = cond_holds(cond, *nzcv);
            break;
    }

    if (set_flags) {
        
        bool neg = 0;
        bool zero = 0;
        bool carry = 0;
        bool overflow = 0;
        uint64_t result = *val_e;

        if (result >> 63) {
            neg = 1; 
        }

        if (!result) {
            zero = 1;
        }

        if (ALUop == PLUS_OP && (result < alu_vala && result < alu_valb)
            || (ALUop == MINUS_OP && alu_vala >= alu_valb)) {
            carry = 1;
        }
        
        int64_t a = (int64_t) alu_vala;
        int64_t b = (int64_t) alu_valb;
        int64_t c = (int64_t) result;

        bool additionOverflow = ALUop == PLUS_OP && ((a > 0 && b > 0 && c <= 0) || (a < 0 && b < 0 && c >= 0));
        bool subtractionOverflow = ALUop == MINUS_OP && ((a > 0 && b < 0 && c <= 0) || (a < 0 && b > 0 && c >= 0) || 
        b == (-1 * (__INT64_MAX__) - 1));
        //bool subtractionOverflow = ALUop == MINUS_OP && ((a ^ b) < 0 && (a ^ c) < 0);
        overflow = additionOverflow || subtractionOverflow;
        
        *nzcv = neg << 3 | zero << 2 | carry << 1 | overflow;
        *cond_val = cond_holds(cond, *nzcv);
    }
}

comb_logic_t 
dmem(uint64_t dmem_addr, uint64_t dmem_wval, bool dmem_read, bool dmem_write, 
     uint64_t *dmem_rval, bool *dmem_err) {
    if(!dmem_read && !dmem_write) {
        return;
    }
    // dmem_addr must be in "data memory" and a multiple of 8
    *dmem_err = (!addr_in_dmem(dmem_addr) || (dmem_addr & 0x7U));
    if (is_special_addr(dmem_addr)) *dmem_err = false;
    if (dmem_read) *dmem_rval = (uint64_t) mem_read_L(dmem_addr);
    if (dmem_write) mem_write_L(dmem_addr, dmem_wval);
}
