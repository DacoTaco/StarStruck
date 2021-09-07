.extern handle_syscall
.extern exc_handler
.extern SaveUserModeState
.extern RestoreAndReturnToUserMode
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
#normally our current sp should be pointing to the saves registers state
#RestoreAndReturnToUserMode(return_value, registers, new sp, swi_mode)
#	mov		r0, r0
	mov		r1, sp
	mov		r2, #1
	b		RestoreAndReturnToUserMode