// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/lshrdi3.c
 */

#include <linux/module.h>
#include <linux/libgcc.h>

<<<<<<< HEAD:lib/lshrdi3.c
=======
#include "libgcc.h"

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc:arch/mips/lib/lshrdi3.c
long long notrace __lshrdi3(long long u, word_type b)
{
	DWunion uu, w;
	word_type bm;

	if (b == 0)
		return u;

	uu.ll = u;
	bm = 32 - b;

	if (bm <= 0) {
		w.s.high = 0;
		w.s.low = (unsigned int) uu.s.high >> -bm;
	} else {
		const unsigned int carries = (unsigned int) uu.s.high << bm;

		w.s.high = (unsigned int) uu.s.high >> b;
		w.s.low = ((unsigned int) uu.s.low >> b) | carries;
	}

	return w.ll;
}
EXPORT_SYMBOL(__lshrdi3);
