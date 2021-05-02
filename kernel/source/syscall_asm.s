.extern handle_syscall
.extern exc_handler
.extern SaveUserModeState
.extern RestoreUserModeState
.globl v_swi

v_swi:
#store registers from before the call. r0 = return, r1 - r12 is parameters. current lr is the return address
	stmdb	sp!, {lr}
	mov		r8, lr
#load address of our state into r1 , which we will use to retrieve the state
	bl 		SaveUserModeState
	mov		r1, sp
#load syscall number into r0 (from r8/lr) and cut off the first few bytes of the instruction
	ldr		r0,[r8,#-4]
	bic		r0,r0,#0xFF000000

#syscall handler
	blx		handle_syscall
	
#load registers back and return
	mov		r1, #1
	bl		RestoreUserModeState
	ldmia	sp!, {lr}
	ldr		sp, =__swistack_addr
	movs	pc, lr
	
#old swi handler from mini. basically just throws the exception
v_swi_old:
	stmfd	sp!, {lr}
	stmfd	sp, {r0-lr}^
#substract 16*4 to the stack pointer,so we have a pointer to the register values. 
	sub		sp, sp, #0x3c
	mov		r2, sp
	mrs		r1, spsr
	mov		r0, #2

	blx		exc_handler

	ldmfd	sp!, {r0-r12}
	add		sp, sp, #8
	ldmfd	sp!, {lr}
	movs	pc, lr
