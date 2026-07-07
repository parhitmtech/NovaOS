/* scheduler.c - preemptive round robin scheduler */
#include "scheduler.h"
#include "heap.h"
#include"pmm.h"

static volatile int reschedule_needed = 0;

/* ── Scheduler state ─────────────────────────────────────── */
pcb_t* current_process = 0;  /* currently running process */
static pcb_t* ready_queue= 0;  /* head of circular ready queue */

static uint32_t next_pid = 1;  /* monotonically increasing PID counter */
static uint32_t process_count = 0;

static pcb_t* prev_process = 0;

/* ── Forward declarations ────────────────────────────────── */
extern void vga_print(const char* str, int fg);

/* ── String helpers (no libc) ────────────────────────────── */
static void str_copy(char* dst, const char* src, size_t max) {
	size_t i = 0;
	while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
	dst[i] = '\0';
}

static void print_dec(uint64_t n) {
	char buf[20];
	int i = 0;
	if (n == 0) { vga_print("0", 7); return; }
	while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
	for (int j = 0; j < i/2; j++) {
		char tmp = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = tmp;
	}
	buf[i] = '\0';
	vga_print(buf, 7);
}

/* ── Idle process ────────────────────────────────────────── */
/* Runs when no other process is ready. Just halts until next interrupt.
 * Without this, the schedulerhas nothing to switch to when the queue is empty, which causes undefined behavior. */
static void idle_process(void) {
	while (1) {
		__asm__ volatile ("hlt");
	}
}

/* ── Process creation ────────────────────────────────────── */
pcb_t* process_create(void (*entry)(void), const char* name) {
	extern void vga_print(const char* str, int fg);
	if (process_count >= MAX_PROCESSES) return 0;

	/* Allocate PCB from heap */
	pcb_t* proc = (pcb_t*) kmalloc(sizeof(pcb_t));
	if (!proc) return 0;

	/* Allocate stack */
	void* stack = kmalloc(PROCESS_STACK_SIZE);
	if (!stack) { kfree(proc); return 0; }

	/* Zero out the PCB */
	uint8_t* p = (uint8_t*) proc;
	for (size_t i = 0; i < sizeof(pcb_t); i++) p[i] = 0;

	/* Set up PCB fields */
	proc->pid = next_pid++;
	proc->state = PROCESS_READY;
	proc->stack = stack;
	proc->quantum = SCHEDULER_QUANTUM;
	proc->ticks_left = SCHEDULER_QUANTUM;
		proc->total_ticks = 0;
	str_copy(proc->name, name, 32);

	/* Set up initial CPU context so the process at entry()
	 * Stack layout for a new process (grows downward);
	 *   [stack_top - 8] = address of process_exit (return address)
	 *   [stack_top - 16] = address of entry (rip for context_switch ret trick)
	 * When context_switch restores this process for the first time:
	 *    rsp = stack_top - 16;
	 *    ret pops_entry() address and jumps there
	 *    if  entry() ever returns, it returns to process_exit()
	 */
	uint64_t stack_top = (uint64_t) stack + PROCESS_STACK_SIZE;

	/* Set up return chain on the new stack */
	uint64_t* sp = (uint64_t*) stack_top;
	*(--sp) = (uint64_t) process_exit;  /* if entry returns, call exit */
	*(--sp) = (uint64_t) entry;  /* rip: start here */

	proc->context.rsp = (uint64_t) sp;
	proc->context.rip = (uint64_t) entry;
	proc->context.rflags = 0x202;  /* IF=1 (interrupts enabled), reserved bit 1 */

	/* Callee-saved registers start at 0 - safe default for a new process */
	proc->context.rbx = 0;
	proc->context.rbp = 0;
	proc->context.r12 = 0;
	proc->context.r13 = 0;
	proc->context.r14 = 0;
	proc->context.r15 = 0;

	/* Add to circular ready queue */
	if (!ready_queue) {
		ready_queue = proc;
		proc->next = proc;  /* circular: points to itself */
	} else {
		/* Insert after current tail - walk to find tail */
		pcb_t* tail = ready_queue;
		while (tail->next != ready_queue) tail = tail->next;
		tail->next = proc;
		proc->next = ready_queue;
	}

	process_count++;
	return proc;
}

/* ── Scheduler init ──────────────────────────────────────── */
void scheduler_init(void) {
	extern void vga_print(const char* str, int fg);

	/* Create idle process first - always available to run */
	pcb_t* idle = process_create(idle_process, "idle");

	if (!idle) return;

	/* Idle process starts as current so the first tick has something
	 * to switch away from */
	current_process = idle;
	current_process->state = PROCESS_RUNNING;
}

/* ── Pick next process ───────────────────────────────────── */
static pcb_t* pick_next(void) {
	if (!ready_queue) return current_process;

	/* Round robin: walk from current_process forward;
	 * find the next READY process */
	pcb_t* candidate = current_process->next;
	pcb_t* start = candidate;

	do {
		if (candidate->state == PROCESS_READY) return candidate;
		candidate = candidate->next;
	} while (candidate != start);

	/* No ready process found - run idle */
	return ready_queue; /* idle is always first in queue */
}

void scheduler_do_switch(void) {
	extern void vga_print(const char* str, int fg);
	if (!reschedule_needed || !prev_process) return;
	vga_print("!", 2);
	reschedule_needed = 0;
	pcb_t* old = prev_process;
	prev_process = 0;
	context_switch(&old->context, &current_process->context);
}

/* ── Tick handler (called from PIT IRQ every 10ms) ──────── */
void scheduler_tick(void) {
	extern void vga_print(const char* str, int fg);
	if (!current_process) {
		vga_print("NO_PROC\n", 4);
		return;
	}
	vga_print("T", 7);

	current_process->total_ticks++;

	if (current_process->ticks_left > 0)
		current_process->ticks_left--;

	/* Time slice expired - preempt */
	if (current_process->ticks_left == 0) {
		current_process->ticks_left = current_process->quantum;

		if (current_process->state == PROCESS_RUNNING)
			current_process->state = PROCESS_READY;

		pcb_t* next = pick_next();
		if (next == current_process) return;  /* Only one process, no switch neede */

		pcb_t* old = current_process;
		prev_process = old;
		current_process = next;
		next->state = PROCESS_RUNNING;
		reschedule_needed = 1;
	}
}

/* ── Voluntary yield ─────────────────────────────────────── */
void scheduler_yield(void) {
	if (!current_process) return;

	current_process->ticks_left = 0;
	scheduler_tick();
}

/* ── Process exit ────────────────────────────────────────── */
void process_exit(void) {
	extern void vga_print(const char* str, int fg);
	vga_print("EXIT!\n", 4);
	if (!current_process) return;

	current_process->state = PROCESS_DEAD;

	/* Remove the circular queue */
	if (current_process->next == current_process) {
		/* Only process left - this shouldn't happen if idle exists */
		ready_queue = 0;
	} else {
		pcb_t* prev = current_process;
		while (prev->next != current_process) prev = prev->next;
		prev->next = current_process->next;
		if (ready_queue == current_process)
			ready_queue = current_process->next;
	}

	/* Free resources */
	kfree(current_process->stack);
	kfree(current_process);
	process_count--;

	/* Force switch to next process */
	current_process = pick_next();
	if (current_process) {
		current_process->state = PROCESS_RUNNING;
		/* Can't context_switch here since we freed old context.
		 * Instead, directly restore the new context. */
		cpu_context_t* ctx = &current_process->context;
		__asm__ volatile (
			"mov %0, %%rsp\n"
			"push %1\n"
			"popfq\n"
			"ret\n"
			: : "r"(ctx->rsp), "r"(ctx->rflags)
		);
	}

	/* Should never reach here */
	for (;;) __asm__ volatile ("hlt");
}

/* ── Stats ───────────────────────────────────────────────── */
void scheduler_print_stats(void) {
	print_dec(process_count);

	pcb_t* p = ready_queue;
	if (!p) return;
	do {
		print_dec(p->pid);
		print_dec(p->total_ticks);
		p = p->next;
	} while (p != ready_queue);
}
