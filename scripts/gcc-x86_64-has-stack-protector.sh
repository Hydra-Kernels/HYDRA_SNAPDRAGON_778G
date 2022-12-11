#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

<<<<<<< HEAD
echo "int foo(void) { char X[200]; return 3; }" | $* -S -x c -c -m64 -O0 -mcmodel=kernel -fno-PIE -fstack-protector - -o - 2> /dev/null | grep -q "%gs"
=======
echo "int foo(void) { char X[200]; return 3; }" | $* -S -x c -c -O0 -mcmodel=kernel -fno-PIE -fstack-protector - -o - 2> /dev/null | grep -q "%gs"
if [ "$?" -eq "0" ] ; then
	echo y
else
	echo n
fi
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
