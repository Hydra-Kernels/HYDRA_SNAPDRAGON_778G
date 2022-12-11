// SPDX-License-Identifier: GPL-2.0-or-later
/*
 */

#include <linux/export.h>

<<<<<<< HEAD:lib/cmpdi2.c
#include <linux/libgcc.h>

=======
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc:arch/mips/lib/cmpdi2.c
word_type notrace __cmpdi2(long long a, long long b)
{
	const DWunion au = {
		.ll = a
	};
	const DWunion bu = {
		.ll = b
	};

	if (au.s.high < bu.s.high)
		return 0;
	else if (au.s.high > bu.s.high)
		return 2;

	if ((unsigned int) au.s.low < (unsigned int) bu.s.low)
		return 0;
	else if ((unsigned int) au.s.low > (unsigned int) bu.s.low)
		return 2;

	return 1;
}
EXPORT_SYMBOL(__cmpdi2);
