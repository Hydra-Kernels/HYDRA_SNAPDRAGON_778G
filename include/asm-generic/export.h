#ifndef __ASM_GENERIC_EXPORT_H
#define __ASM_GENERIC_EXPORT_H

#ifndef KSYM_FUNC
#define KSYM_FUNC(x) x
#endif
<<<<<<< HEAD
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
#define KSYM_ALIGN 4
#elif defined(CONFIG_64BIT)
#define KSYM_ALIGN 8
#else
=======
#ifdef CONFIG_64BIT
#define __put .quad
#ifndef KSYM_ALIGN
#define KSYM_ALIGN 8
#endif
#ifndef KCRC_ALIGN
#define KCRC_ALIGN 8
#endif
#else
#define __put .long
#ifndef KSYM_ALIGN
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#define KSYM_ALIGN 4
#endif
#ifndef KCRC_ALIGN
#define KCRC_ALIGN 4
#endif
<<<<<<< HEAD

.macro __put, val, name
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
	.long	\val - ., \name - ., 0
#elif defined(CONFIG_64BIT)
	.quad	\val, \name, 0
#else
	.long	\val, \name, 0
#endif
.endm
=======
#endif

#ifdef CONFIG_HAVE_UNDERSCORE_SYMBOL_PREFIX
#define KSYM(name) _##name
#else
#define KSYM(name) name
#endif
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

/*
 * note on .section use: @progbits vs %progbits nastiness doesn't matter,
 * since we immediately emit into those sections anyway.
 */
.macro ___EXPORT_SYMBOL name,val,sec
#ifdef CONFIG_MODULES
<<<<<<< HEAD
	.globl __ksymtab_\name
	.section ___ksymtab\sec+\name,"a"
	.balign KSYM_ALIGN
__ksymtab_\name:
	__put \val, __kstrtab_\name
	.previous
	.section __ksymtab_strings,"a"
__kstrtab_\name:
	.asciz "\name"
=======
	.globl KSYM(__ksymtab_\name)
	.section ___ksymtab\sec+\name,"a"
	.balign KSYM_ALIGN
KSYM(__ksymtab_\name):
	__put \val, KSYM(__kstrtab_\name)
	.previous
	.section __ksymtab_strings,"a"
KSYM(__kstrtab_\name):
#ifdef CONFIG_HAVE_UNDERSCORE_SYMBOL_PREFIX
	.asciz "_\name"
#else
	.asciz "\name"
#endif
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	.previous
#ifdef CONFIG_MODVERSIONS
	.section ___kcrctab\sec+\name,"a"
	.balign KCRC_ALIGN
<<<<<<< HEAD
__kcrctab_\name:
#if defined(CONFIG_MODULE_REL_CRCS)
	.long __crc_\name - .
#else
	.long __crc_\name
#endif
	.weak __crc_\name
=======
KSYM(__kcrctab_\name):
	__put KSYM(__crc_\name)
	.weak KSYM(__crc_\name)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	.previous
#endif
#endif
.endm
<<<<<<< HEAD

#if defined(CONFIG_TRIM_UNUSED_KSYMS)
=======
#undef __put

#if defined(__KSYM_DEPS__)

#define __EXPORT_SYMBOL(sym, val, sec)	=== __KSYM_##sym ===

#elif defined(CONFIG_TRIM_UNUSED_KSYMS)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

#include <linux/kconfig.h>
#include <generated/autoksyms.h>

<<<<<<< HEAD
.macro __ksym_marker sym
	.section ".discard.ksym","a"
__ksym_marker_\sym:
	 .previous
.endm

#define __EXPORT_SYMBOL(sym, val, sec)				\
	__ksym_marker sym;					\
	__cond_export_sym(sym, val, sec, __is_defined(__KSYM_##sym))
=======
#define __EXPORT_SYMBOL(sym, val, sec)				\
	__cond_export_sym(sym, val, sec, config_enabled(__KSYM_##sym))
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#define __cond_export_sym(sym, val, sec, conf)			\
	___cond_export_sym(sym, val, sec, conf)
#define ___cond_export_sym(sym, val, sec, enabled)		\
	__cond_export_sym_##enabled(sym, val, sec)
#define __cond_export_sym_1(sym, val, sec) ___EXPORT_SYMBOL sym, val, sec
#define __cond_export_sym_0(sym, val, sec) /* nothing */

#else
#define __EXPORT_SYMBOL(sym, val, sec) ___EXPORT_SYMBOL sym, val, sec
#endif

#define EXPORT_SYMBOL(name)					\
<<<<<<< HEAD
	__EXPORT_SYMBOL(name, KSYM_FUNC(name),)
#define EXPORT_SYMBOL_GPL(name) 				\
	__EXPORT_SYMBOL(name, KSYM_FUNC(name), _gpl)
#define EXPORT_DATA_SYMBOL(name)				\
	__EXPORT_SYMBOL(name, name,)
#define EXPORT_DATA_SYMBOL_GPL(name)				\
	__EXPORT_SYMBOL(name, name,_gpl)
=======
	__EXPORT_SYMBOL(name, KSYM_FUNC(KSYM(name)),)
#define EXPORT_SYMBOL_GPL(name) 				\
	__EXPORT_SYMBOL(name, KSYM_FUNC(KSYM(name)), _gpl)
#define EXPORT_DATA_SYMBOL(name)				\
	__EXPORT_SYMBOL(name, KSYM(name),)
#define EXPORT_DATA_SYMBOL_GPL(name)				\
	__EXPORT_SYMBOL(name, KSYM(name),_gpl)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

#endif
