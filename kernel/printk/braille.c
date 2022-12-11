// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "console_cmdline.h"
#include "braille.h"

int _braille_console_setup(char **str, char **brl_options)
{
<<<<<<< HEAD
	size_t len;

	len = str_has_prefix(*str, "brl,");
	if (len) {
		*brl_options = "";
		*str += len;
		return 0;
	}

	len = str_has_prefix(*str, "brl=");
	if (len) {
		*brl_options = *str + len;
		*str = strchr(*brl_options, ',');
		if (!*str) {
			pr_err("need port name after brl=\n");
			return -EINVAL;
		}
		*((*str)++) = 0;
	}

=======
	if (!strncmp(*str, "brl,", 4)) {
		*brl_options = "";
		*str += 4;
	} else if (!strncmp(*str, "brl=", 4)) {
		*brl_options = *str + 4;
		*str = strchr(*brl_options, ',');
		if (!*str) {
			pr_err("need port name after brl=\n");
			return -EINVAL;
		}
		*((*str)++) = 0;
	}

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	return 0;
}

int
_braille_register_console(struct console *console, struct console_cmdline *c)
{
	int rtn = 0;

	if (c->brl_options) {
		console->flags |= CON_BRL;
		rtn = braille_register_console(console, c->index, c->options,
					       c->brl_options);
	}

	return rtn;
}

int
_braille_unregister_console(struct console *console)
{
	if (console->flags & CON_BRL)
		return braille_unregister_console(console);

	return 0;
}
