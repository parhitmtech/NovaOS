; context_switch.asm - low-level CPU context switch for NovaOS scheduler
;
; void context_switch(cpu_context_t* old, cpu_context_t* new_proc)
;	rdi = pointer to old process's cpu_context_t (save here)
;	rsi = pointer to new process's cpu_context_t (load from here)
;
; What we save/restore:
;	Callee-saved registers: rbx, rbp, r12, r13, r14, r15
;	Stack pointer: rsp
;	Instruction pointer: rip (via call/ret trick - see below)
;	Flags: rflags
;
; The rip trick:
;	We can't directly read or write rip. Instead:
;	- When we CALL context_switch, the CPU pushes rip onto the stack.
;	- We save that pushed rip into old->rip.
;	- To restore rip, we push new->rip onto the stack and RET to it.
;	This makes the new process "return" to wherever it was last preempted.
;
; Offsets into cpu_context_t (must match scheduler.h exactly):
;	rbx = 0
;	rbp = 8
;	r12 = 16
;	r13 = 24
;	r14 = 32
;	r15 = 40
;	rsp = 48
;	rip = 56
;	rflags = 64

bits 64
section .text
global context_switch

context_switch:
	; ── Save old process context ──────────────────────────────
	; rdi = old cpu_context_t*

	mov [rdi + 0],  rbx
	mov [rdi + 8],  rbp
	mov [rdi + 16], r12
	mov [rdi + 24], r13
	mov [rdi + 32], r14
	mov [rdi + 40], r15

	; Save rsp - points to our return address on the stack right now
	mov [rdi + 48], rsp

	; Save rip - it's sitting on top of the stack as our return address
	; (pushed there by the CALL instruction that called context_switch)
	mov rax, [rsp]
	mov [rdi + 56], rax

	; Save rflags
	pushfq
	pop rax
	mov [rdi + 64], rax

	; ── Load new process context ──────────────────────────────
	; rsi = new cpu_context_t*

	mov rbx, [rsi + 0]
	mov rbp, [rsi + 8]
	mov r12, [rsi + 16]
	mov r13, [rsi + 24]
	mov r14, [rsi + 32]
	mov r15, [rsi + 40]

	; Restore flags
	mov rax, [rsi + 64]
	push rax
	popfq

	; Restore rsp - switches to new process's stack
	mov rsp, [rsi + 48]

	; Restore rip - push new->rip onto the (now switched) stack,
	; then RET pops it and jumps there. The new process resumes.
	mov rax, [rsi + 56]
	push rax
	ret
