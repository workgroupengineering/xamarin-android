/* libunwind - a platform-independent unwind library
   Copyright (C) 2008 CodeSourcery
   Copyright (C) 2011-2013 Linaro Limited
   Copyright (C) 2012 Tommi Rantala <tt.rantala@gmail.com>

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

//#include "unwind_i.h"
#include "../../../../external/libunwind/src/aarch64/unwind_i.h"
#ifndef UNW_REMOTE_ONLY

HIDDEN int
aarch64_local_resume (unw_addr_space_t as, unw_cursor_t *cursor, void *arg)
{
	struct cursor *c = (struct cursor *) cursor;
	unw_tdep_context_t *uc = c->uc;

	if (c->sigcontext_format == AARCH64_SCF_NONE)
		{
			/* Since there are no signals involved here we restore EH and non scratch
			   registers only.  */
			unsigned long regs[24];
			regs[0] = uc->uc_mcontext.regs[0];
			regs[1] = uc->uc_mcontext.regs[1];
			regs[2] = uc->uc_mcontext.regs[2];
			regs[3] = uc->uc_mcontext.regs[3];
			regs[4] = uc->uc_mcontext.regs[19];
			regs[5] = uc->uc_mcontext.regs[20];
			regs[6] = uc->uc_mcontext.regs[21];
			regs[7] = uc->uc_mcontext.regs[22];
			regs[8] = uc->uc_mcontext.regs[23];
			regs[9] = uc->uc_mcontext.regs[24];
			regs[10] = uc->uc_mcontext.regs[25];
			regs[11] = uc->uc_mcontext.regs[26];
			regs[12] = uc->uc_mcontext.regs[27];
			regs[13] = uc->uc_mcontext.regs[28];
			regs[14] = uc->uc_mcontext.regs[29]; /* FP */
			regs[15] = uc->uc_mcontext.regs[30]; /* LR */
			regs[16] = GET_FPCTX(uc)->vregs[8];
			regs[17] = GET_FPCTX(uc)->vregs[9];
			regs[18] = GET_FPCTX(uc)->vregs[10];
			regs[19] = GET_FPCTX(uc)->vregs[11];
			regs[20] = GET_FPCTX(uc)->vregs[12];
			regs[21] = GET_FPCTX(uc)->vregs[13];
			regs[22] = GET_FPCTX(uc)->vregs[14];
			regs[23] = GET_FPCTX(uc)->vregs[15];
			unsigned long sp = uc->uc_mcontext.sp;

			struct regs_overlay {
				char x[sizeof(regs)];
			};

			__asm__ __volatile__ (
			"mov x4, %0\n"
			"mov x5, %1\n"
			"ldp x0,  x1,  [x4]\n"
			"ldp x2,  x3,  [x4,16]\n"
			"ldp x19, x20, [x4,32]\n"
			"ldp x21, x22, [x4,48]\n"
			"ldp x23, x24, [x4,64]\n"
			"ldp x25, x26, [x4,80]\n"
			"ldp x27, x28, [x4,96]\n"
			"ldp x29, x30, [x4,112]\n"
			"ldp d8, d9,   [x4,128]\n"
			"ldp d10, d11, [x4,144]\n"
			"ldp d12, d13, [x4,160]\n"
			"ldp d14, d15, [x4,176]\n"
			"mov sp, x5\n"
			"ret \n"
			:
			: "r" (regs),
			  "r" (sp),
			  "m" (*(struct regs_overlay *)regs)
			 );
		}
	else
		{
			struct sigcontext *sc = (struct sigcontext *) c->sigcontext_addr;

			if (c->dwarf.eh_valid_mask & 0x1) sc->regs[0] = c->dwarf.eh_args[0];
			if (c->dwarf.eh_valid_mask & 0x2) sc->regs[1] = c->dwarf.eh_args[1];
			if (c->dwarf.eh_valid_mask & 0x4) sc->regs[2] = c->dwarf.eh_args[2];
			if (c->dwarf.eh_valid_mask & 0x8) sc->regs[3] = c->dwarf.eh_args[3];

			sc->regs[4] = uc->uc_mcontext.regs[4];
			sc->regs[5] = uc->uc_mcontext.regs[5];
			sc->regs[6] = uc->uc_mcontext.regs[6];
			sc->regs[7] = uc->uc_mcontext.regs[7];
			sc->regs[8] = uc->uc_mcontext.regs[8];
			sc->regs[9] = uc->uc_mcontext.regs[9];
			sc->regs[10] = uc->uc_mcontext.regs[10];
			sc->regs[11] = uc->uc_mcontext.regs[11];
			sc->regs[12] = uc->uc_mcontext.regs[12];
			sc->regs[13] = uc->uc_mcontext.regs[13];
			sc->regs[14] = uc->uc_mcontext.regs[14];
			sc->regs[15] = uc->uc_mcontext.regs[15];
			sc->regs[16] = uc->uc_mcontext.regs[16];
			sc->regs[17] = uc->uc_mcontext.regs[17];
			sc->regs[18] = uc->uc_mcontext.regs[18];
			sc->regs[19] = uc->uc_mcontext.regs[19];
			sc->regs[20] = uc->uc_mcontext.regs[20];
			sc->regs[21] = uc->uc_mcontext.regs[21];
			sc->regs[22] = uc->uc_mcontext.regs[22];
			sc->regs[23] = uc->uc_mcontext.regs[23];
			sc->regs[24] = uc->uc_mcontext.regs[24];
			sc->regs[25] = uc->uc_mcontext.regs[25];
			sc->regs[26] = uc->uc_mcontext.regs[26];
			sc->regs[27] = uc->uc_mcontext.regs[27];
			sc->regs[28] = uc->uc_mcontext.regs[28];
			sc->regs[29] = uc->uc_mcontext.regs[29];
			sc->regs[30] = uc->uc_mcontext.regs[30];
			sc->sp = uc->uc_mcontext.sp;
			sc->pc = uc->uc_mcontext.pc;
			sc->pstate = uc->uc_mcontext.pstate;

			__asm__ __volatile__ (
			"mov sp, %0\n"
			"ret %1\n"
			: : "r" (c->sigcontext_sp), "r" (c->sigcontext_pc)
			 );
		}
	unreachable();
	return -UNW_EINVAL;
}

#endif /* !UNW_REMOTE_ONLY */
