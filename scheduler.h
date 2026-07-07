#pragma once
#include <stdint.h>
#include <stddef.h>

#define PROCESS_STACK_SIZE  (16 * 1024)
#define SCHEDULER_QUANTUM   10
#define MAX_PROCESSES       64

typedef struct {
    uint64_t rbx, rbp, r12, r13, r14, r15;
    uint64_t rsp;
    uint64_t rip;
    uint64_t rflags;
} cpu_context_t;

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_DEAD
} process_state_t;

typedef struct process {
    uint32_t         pid;
    process_state_t  state;
    cpu_context_t    context;
    void*            stack;
    uint32_t         quantum;
    uint32_t         ticks_left;
    uint64_t         total_ticks;
    char             name[32];
    struct process*  next;
} pcb_t;

void   scheduler_init(void);
pcb_t* process_create(void (*entry)(void), const char* name);
void   scheduler_tick(void);
void   scheduler_yield(void);
void   process_exit(void);
void   scheduler_print_stats(void);
void   scheduler_do_switch(void);

extern pcb_t* current_process;
extern void context_switch(cpu_context_t* old, cpu_context_t* new_proc);
