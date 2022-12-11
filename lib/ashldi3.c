// SPDX-License-Identifier: GPL-2.0-or-later
/*
 */

#include <linux/export.h>

<<<<<<< HEAD:lib/ashldi3.c
#include <linux/libgcc.h>

=======
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc:arch/mips/lib/ashldi3.c
long long notrace __ashldi3(long long u, word_type b)
{
	DWunion uu, w;
	word_type bm;

	if (b == 0)
		return u;

	uu.ll = u;
	bm = 32 - b;

	if (bm <= 0) {
		w.s.low = 0;
		w.s.high = (unsigned int) uu.s.low << -bm;
	} else {
		const unsigned int carries = (unsigned int) uu.s.low >> bm;

		w.s.low = (unsigned int) uu.s.low << b;
		w.s.high = ((unsigned int) uu.s.high << b) | carries;
	}

	return w.ll;
}
EXPORT_SYMBOL(__ashldi3);
