/**************************************************************************
 * STUDENTS: DO NOT MODIFY.
 * 
 * C S 429 system emulator
 * 
 * machine.c - Module for initializing the guest machine.
 * 
 * Copyright (c) 2022, 2023, 2024, 2025. 
 * Authors: S. Chatterjee, Z. Leeper.
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 

#include <stdlib.h>
#include <string.h>
#include "machine.h"
#include "ptable.h"

/* Created from command-line arguments */
extern FILE *checkpoint;
extern int A, B, C, d;
extern uint64_t inflight_cycles;
extern uint64_t inflight_addr;
extern bool inflight;
extern mem_status_t dmem_status;
extern uint64_t num_instr;

// These may be changed by the ELF loader
uint64_t seg_starts[] = {
    0x0ULL,
    0x400000ULL,
    0x800000ULL,
    0x10000000ULL,
    0x400000000ULL,
    0x800000000ULL,
    0x1000000000000ULL
};

static char *GPR_names64[] = {
    " X0", " X1", " X2", " X3", " X4", " X5", " X6", " X7",
    " X8", " X9", "X10", "X11", "X12", "X13", "X14", "X15",
    "X16", "X17", "X18", "X19", "X20", "X21", "X22", "X23",
    "X24", "X25", "X26", "X27", "X28", "X29", "X30"
};

static uint8_t seg_prots[] = {0x0, 0x5, 0x6, 0x6, 0x5, 0x6, 0x0};

extern machine_t guest;

#define NUM_ADDR_BITS 64

void init_machine() {
    // guest.name = malloc(strlen(name)+1);
    // strcpy(guest.name, name);

    guest.proc = calloc(1, sizeof(proc_t));
    
    guest.mem = malloc(sizeof(mem_t));
    guest.mem->max_addr = UINT_FAST64_MAX;
    guest.mem->addr_size = NUM_ADDR_BITS;
    for (int i = 0; i <= KERNEL_SEG; i++) {
        guest.mem->seg_start_addr[i] = seg_starts[i];
        guest.mem->seg_prot[i] = seg_prots[i];
    }
    if (A == -1 || B == -1 || C == -1 || d == -1) {
        guest.cache = NULL;
    }
    else {
        guest.cache = create_cache(A, B, C, d);
        inflight_cycles = guest.cache->d;
        inflight_addr = 0;
        inflight = false;
        dmem_status = READY;
    }
}

static void get_stat_str(char *str, stat_t status) {
    switch (status) {
        case STAT_AOK:
            strcpy(str, "AOK");
            break;
        case STAT_BUB:
            strcpy(str, "BUB");
            break;
        case STAT_HLT:
            strcpy(str, "HLT");
            break;
        case STAT_ADR:
            strcpy(str, "ADR");
            break;
        case STAT_INS:
            strcpy(str, "INS");
            break;
    }
}

void log_machine_state() {
    if (checkpoint) {
        fprintf(checkpoint, "Machine state checkpoint after %ld cycles:\n", num_instr);
        // Log processor state
        fprintf(checkpoint, "\tProcessor state:\n");
        // PC and SP
        fprintf(checkpoint, "\t\tProgram Counter: %lx\n", guest.proc->PC);
        fprintf(checkpoint, "\t\tStack Pointer: %lx\n", guest.proc->SP);
        // NZCV
        uint8_t cc = guest.proc->NZCV;
        fprintf(checkpoint, "\t\tCondition Flags: [N,Z,C,V] = [%x, %x, %x, %x]\n",
                GET_NF(cc), GET_ZF(cc), GET_CF(cc), GET_VF(cc));
        // 64-bit registers
        fprintf(checkpoint, "\t\tGeneral Purpose Register File state:\n");
        for (int i = 0; i < 31; i++) {
            fprintf(checkpoint, "\t\t\tRegister %s: %lx\n", GPR_names64[i], guest.proc->GPR[i]);
        }
        // status  
        char buf[4];
        get_stat_str(buf, guest.proc->status);
        fprintf(checkpoint, "\t\tStatus: %s\n", buf);
        // Log memory state
        fprintf(checkpoint, "\tMemory state:\n");
        /* 
         * .text section
         * This isn't really needed since students don't 
         * write the ELF Loader and shouldn't modify the
         * instructions, but it was useful for debugging.
         */
        fprintf(checkpoint, "\t\tText segment:\n");
        pte_ptr_t page;
        uint64_t addr = guest.mem->seg_start_addr[TEXT_SEG];
        addr -= addr % PAGESIZE;
        uint64_t pnum = addr / PAGESIZE;
        while ((page = get_page(pnum))) {
            for (int i = 0; i < PAGESIZE; i += 8) {
                uint64_t data = *(uint64_t *)(page->p_data + i);
                if (data) {
                    fprintf(checkpoint, "\t\t\tAddress 0x%lx: 0x%lx\n", 
                        addr+i, data);
                }
            }
            addr += PAGESIZE;
            pnum = addr / PAGESIZE;
        }
        // .data section
        fprintf(checkpoint, "\t\tData segment:\n");
        addr = guest.mem->seg_start_addr[DATA_SEG];
        addr -= addr % PAGESIZE;
        pnum = addr / PAGESIZE;
        while ((page = get_page(pnum))) {
            for (int i = 0; i < PAGESIZE; i += 8) {
                uint64_t data = *(uint64_t *)(page->p_data + i);
                if (data) {
                    fprintf(checkpoint, "\t\t\tAddress 0x%lx: 0x%lx\n", 
                        addr+i, data);
                }
            }
            addr += PAGESIZE;
            pnum = addr / PAGESIZE;
        }
        // Heap memory
        fprintf(checkpoint, "\t\tHeap:\n");
        addr = guest.mem->seg_start_addr[HEAP_SEG];
        while ((page = get_page(pnum))) {
            for (int i = addr%PAGESIZE; i < PAGESIZE; i += 8) {
                uint64_t data = *(uint64_t *)(page->p_data + i);
                if (data) {
                    fprintf(checkpoint, "\t\t\tAddress 0x%lx: 0x%lx\n", 
                        addr+i, data);
                }
            }
            addr += PAGESIZE;
            pnum = addr / PAGESIZE;
        }
        // Stack memory
        fprintf(checkpoint, "\t\tStack:\n");
        addr = guest.mem->seg_start_addr[STACK_SEG]-PAGESIZE;
        addr -= addr % PAGESIZE;
        pnum = addr / PAGESIZE;
        while ((page = get_page(pnum))) {
            for (int i = addr%PAGESIZE; i < PAGESIZE; i += 8) {
                uint64_t data = *(uint64_t *)(page->p_data + i);
                if (data) {
                    fprintf(checkpoint, "\t\t\tAddress 0x%lx: 0x%lx\n", 
                        addr+i, data);
                }
            }
            addr -= PAGESIZE;
            pnum = addr / PAGESIZE;
        }
        extern int hit_count;
        extern int miss_count;
        int hits, misses;
        // mem.c code for this gives extra hits and misses
        if (guest.cache) {
            misses = miss_count / guest.cache->d;
            hits = hit_count-misses;
            fprintf(checkpoint, "\t\tNumber of cache hits, misses: %d, %d\n", hits, misses);
        }

        fprintf(checkpoint, "\n");
    }
}
