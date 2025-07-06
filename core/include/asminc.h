/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Assembly macro's

Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#if !__ASSEMBLER__
    #error This header file is only for use in assembly files!
#endif // !__ASSEMBLER__

/* clang-format off */
#macro comes from devkitPro's libctru
#thanks fincs!
#this macro adds a few macros and directives to functions that are handy or required 
#example : .type [functionname], %function, which allows arm functions to be called from thumb code
.macro BEGIN_ASM_FUNC name, linkage=global, section=text
    .section        .\section\().\name, "ax", %progbits
    .align          2
    .\linkage       \name
    .type           \name, %function
    .func           \name
    \name:
.endm

.macro END_ASM_FUNC
    .endfunc
.endm
/* clang-format on */

#ifdef __thumb__
#define _BL blx
#else
#define _BL bl
#endif


#ifdef __thumb__
#define _THUMBMODE_
#endif