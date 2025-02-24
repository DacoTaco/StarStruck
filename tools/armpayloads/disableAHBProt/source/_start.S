.section .init
.globl _start
.thumb
.extern DisableAHBProt
_start:
bl	DisableAHBProt

_loop:
b	_loop
