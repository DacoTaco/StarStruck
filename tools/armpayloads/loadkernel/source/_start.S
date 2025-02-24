.section .init
.globl _start
.thumb
_start:
#load in address
ldr r0, =0x10100000 //address
ldr r1, =0x0025161F //version

#switch to arm mode
bx pc
nop

.arm
#syscall 47 - boot kernel
.long 0xE6000870
bx lr
