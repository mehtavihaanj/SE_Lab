/**************************************************************************
 * STUDENTS: DO NOT MODIFY.
 * 
 * C S 429 system emulator
 * 
 * proc.c - The top-level instruction processing loop of the processor.
 * 
 * Copyright (c) 2022, 2023, 2024, 2025. 
 * Authors: S. Chatterjee, Z. Leeper., K. Chandrasekhar
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 

#include "archsim.h"
#include "hw_elts.h"
#include "hazard_control.h"
#include "forward.h"
#include <unistd.h>

#include <pthread.h>
#include <bits/pthreadtypes.h>

bool running_sim = true; // should be no need to make this atomic
pthread_barrier_t cycle_start;
pthread_barrier_t latch_end;
pthread_barrier_t cycle_end;

extern uint32_t bitfield_u32(int32_t src, unsigned frompos, unsigned width);
extern int64_t bitfield_s64(int32_t src, unsigned frompos, unsigned width);

extern machine_t guest;
extern mem_status_t dmem_status;

void* start_fetch(void* unused) {
    pthread_barrier_wait(&cycle_start);
    do {
        fetch_instr(F_out, D_in);
        pthread_barrier_wait(&cycle_end);
        pthread_barrier_wait(&cycle_start);
    } while(running_sim);
    pthread_exit(NULL);
}

void* start_decode(void* unused) { 
    pthread_barrier_wait(&cycle_start); //Guarded do :)
    do {
        decode_instr(D_out, X_in);
        // Cycle end called from within
        // pthread_barrier_wait(&cycle_end);
        pthread_barrier_wait(&cycle_start);
    } while(running_sim);
    pthread_exit(NULL);
}

void* start_execute(void* unused) {
    pthread_barrier_wait(&cycle_start);
    do {
        execute_instr(X_out, M_in);   
        pthread_barrier_wait(&cycle_end);
        pthread_barrier_wait(&cycle_start);
    } while(running_sim);
    pthread_exit(NULL);
}

void* start_memory(void* unused) {
    pthread_barrier_wait(&cycle_start);
    do {
        memory_instr(M_out, W_in);
        pthread_barrier_wait(&cycle_end);
        pthread_barrier_wait(&cycle_start);
    } while(running_sim);
    pthread_exit(NULL);
}

void* start_writeback(void* unused) {
    pthread_barrier_wait(&cycle_start);
    do {
        wback_instr(W_out);
        pthread_barrier_wait(&cycle_end);
        pthread_barrier_wait(&cycle_start);
    } while(running_sim);
    pthread_exit(NULL);
}

int runElf(const uint64_t entry) {
    logging(LOG_INFO, "Running ELF executable");
    guest.proc->PC = entry;
    guest.proc->SP = guest.mem->seg_start_addr[STACK_SEG]-8;
    guest.proc->NZCV = PACK_CC(0, 1, 0, 0);
    guest.proc->GPR[30] = RET_FROM_MAIN_ADDR;

    pipe_reg_t **pipes[] = {&F_instr, &D_instr, &X_instr, &M_instr, &W_instr};

    uint64_t sizes[5] = {sizeof(f_instr_impl_t), sizeof(d_instr_impl_t), sizeof(x_instr_impl_t),
                         sizeof(m_instr_impl_t), sizeof(w_instr_impl_t)};
    for (int i = 0; i < 5; i++) {
        *pipes[i] = (pipe_reg_t *)calloc(1, sizeof(pipe_reg_t));
        (*pipes[i])->size = sizes[i];
        (*pipes[i])->in = (pipe_reg_implt_t) calloc(1, sizes[i]);
        (*pipes[i])->out = (pipe_reg_implt_t) calloc(1, sizes[i]);
        (*pipes[i])->ctl = P_BUBBLE;
    }

    /* Will be selected as the first PC */
    F_out->pred_PC = guest.proc->PC;
    F_out->status = STAT_AOK;
    dmem_status = READY;

    num_instr = 0;

#ifdef PARALLEL
    pthread_t stage_threads[5];
    void *(*stages[5]) (void* args) = {&start_fetch, &start_decode, 
        &start_execute, &start_memory, &start_writeback};

    running_sim = true;

    pthread_barrier_init(&cycle_start, NULL, 6);
    pthread_barrier_init(&cycle_end, NULL, 6);
    pthread_barrier_init(&latch_end, NULL, 2);

    for(int stage = 0; stage < 5; stage++) {
        pthread_create(stage_threads + stage, NULL, stages[stage], NULL);
    }
#endif

    do {        
        /* Run each stage (in reverse order, to get the correct effect) */
        /* TODO: rewrite as independent threads */
#ifndef PARALLEL
        wback_instr(W_out);
        memory_instr(M_out, W_in);
        execute_instr(X_out, M_in);   
        decode_instr(D_out, X_in);   
        fetch_instr(F_out, D_in);
#else
        // Start a cycle
        pthread_barrier_wait(&cycle_start);

        // Wait for the cycle to end
        pthread_barrier_wait(&cycle_end);
        pthread_barrier_wait(&latch_end);
#endif

        // Latch

        F_in->pred_PC = guest.proc->PC;

        /* Set machine state to either continue executing or shutdown */
        guest.proc->status = W_out->status;

        uint8_t D_src1 = (D_out->op == OP_MOVZ) ? 0x1F : bitfield_u32(D_out->insnbits, 5, 5);
        uint8_t D_src2 = (D_out->op != OP_STUR) ? bitfield_u32(D_out->insnbits, 16, 5) : bitfield_u32(D_out->insnbits, 0, 5);
        uint64_t D_val_a = X_in->val_a;

        /* Hazard handling and pipeline control */
        handle_hazards(D_out->op, D_src1, D_src2, D_val_a, X_out->op, X_out->dst, M_in->cond_holds);

        /* Print debug output */
        if(debug_level > 0)
            printf("\nPipeline state at end of cycle %ld:\n", num_instr);


        show_instr(S_FETCH, debug_level);
        show_instr(S_DECODE, debug_level);
        show_instr(S_EXECUTE, debug_level);
        show_instr(S_MEMORY, debug_level);
        show_instr(S_WBACK, debug_level);

        if(debug_level > 0)
            printf("\n\n");

        for (int i = 0; i < 5; i++) {
            pipe_reg_t *pipe = *pipes[i];
            switch(pipe->ctl) {
                case P_LOAD: // Normal, cycle stage
                    memcpy(pipe->out.generic, pipe->in.generic, pipe->size);
                    break;
                case P_ERROR:  // Error, bubble this stage
                    guest.proc->status = STAT_HLT;
                case P_BUBBLE: // Hazard, needs to bubble
                    memset(pipe->out.generic, 0, pipe->size);
                    break;
                case P_STALL: // Hazard, needs to stall
                    break;
            }
        }

        num_instr++;
    } while ((guest.proc->status == STAT_AOK || guest.proc->status == STAT_BUB)
             && num_instr < cycle_max);

    running_sim = false;

#ifdef PARALLEL
    // Start threads to send end 'signal' (could also just use pthread_kill...)
    pthread_barrier_wait(&cycle_start);

    for(int stage = 0; stage < 5; stage++) {
        pthread_join(stage_threads[stage], NULL);
    }

    pthread_barrier_destroy(&cycle_start);
    pthread_barrier_destroy(&cycle_end);
#endif

    return EXIT_SUCCESS;
}