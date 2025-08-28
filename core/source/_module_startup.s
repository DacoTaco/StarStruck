.arm

.extern main
.extern __stackEnd
.extern __priority
.extern OSSetThreadPriority
.globl _module_startup
.section .module.init

_module_startup:
#setup stack
ldr		sp, =__stackEnd
#setup thread priority
mov		r5, r0
mov		r6, lr
mov		r0, #0x00
ldr		r1, =__priority
bl		OSSetThreadPriority
mov		r0, r5
mov		lr, r6

#start main
ldr 	r2, =main
bx		r2