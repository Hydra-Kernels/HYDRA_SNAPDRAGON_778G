/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_COMPILER_TYPES_H
#error "Please don't include <linux/compiler-gcc.h> directly, include <linux/compiler.h> instead."
#endif

/*
 * Common definitions for all gcc versions go here.
 */
#define GCC_VERSION (__GNUC__ * 10000		\
		     + __GNUC_MINOR__ * 100	\
		     + __GNUC_PATCHLEVEL__)

#if GCC_VERSION < 40600
# error Sorry, your compiler is too old - please upgrade it.
#elif defined(CONFIG_ARM64) && GCC_VERSION < 50100
/*
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63293
 * https://lore.kernel.org/r/20210107111841.GN1551@shell.armlinux.org.uk
 */
# error Sorry, your version of GCC is too old - please use 5.1 or newer.
#endif

/*
 * This macro obfuscates arithmetic on a variable address so that gcc
 * shouldn't recognize the original var, and make assumptions about it.
 *
 * This is needed because the C standard makes it undefined to do
 * pointer arithmetic on "objects" outside their boundaries and the
 * gcc optimizers assume this is the case. In particular they
 * assume such arithmetic does not wrap.
 *
 * A miscompilation has been observed because of this on PPC.
 * To work around it we hide the relationship of the pointer and the object
 * using this macro.
 *
 * Versions of the ppc64 compiler before 4.1 had a bug where use of
 * RELOC_HIDE could trash r30. The bug can be worked around by changing
 * the inline assembly constraint from =g to =r, in this particular
 * case either is valid.
 */
#define RELOC_HIDE(ptr, off)						\
({									\
	unsigned long __ptr;						\
	__asm__ ("" : "=r"(__ptr) : "0"(ptr));				\
	(typeof(ptr)) (__ptr + (off));					\
})

/*
<<<<<<< HEAD
 * A trick to suppress uninitialized variable warning without generating any
 * code
 */
#define uninitialized_var(x) x = x

#ifdef CONFIG_RETPOLINE
#define __noretpoline __attribute__((__indirect_branch__("keep")))
#endif

=======
 * Feature detection for gnu_inline (gnu89 extern inline semantics). Either
 * __GNUC_STDC_INLINE__ is defined (not using gnu89 extern inline semantics,
 * and we opt in to the gnu89 semantics), or __GNUC_STDC_INLINE__ is not
 * defined so the gnu89 semantics are the default.
 */
#ifdef __GNUC_STDC_INLINE__
# define __gnu_inline	__attribute__((gnu_inline))
#else
# define __gnu_inline
#endif

/*
 * Force always-inline if the user requests it so via the .config,
 * or if gcc is too old.
 * GCC does not warn about unused static inline functions for
 * -Wunused-function.  This turns out to avoid the need for complex #ifdef
 * directives.  Suppress the warning in clang as well by using "unused"
 * function attribute, which is redundant but not harmful for gcc.
 * Prefer gnu_inline, so that extern inline functions do not emit an
 * externally visible function. This makes extern inline behave as per gnu89
 * semantics rather than c99. This prevents multiple symbol definition errors
 * of extern inline functions at link time.
 * A lot of inline functions can cause havoc with function tracing.
 */
#if !defined(CONFIG_ARCH_SUPPORTS_OPTIMIZED_INLINING) ||		\
    !defined(CONFIG_OPTIMIZE_INLINING) || (__GNUC__ < 4)
#define inline \
	inline __attribute__((always_inline, unused)) notrace __gnu_inline
#else
#define inline inline		__attribute__((unused)) notrace __gnu_inline
#endif

#define __inline__ inline
#define __inline inline
#define __always_inline	inline __attribute__((always_inline))
#define  noinline	__attribute__((noinline))

#define __deprecated	__attribute__((deprecated))
#define __packed	__attribute__((packed))
#define __weak		__attribute__((weak))
#define __alias(symbol)	__attribute__((alias(#symbol)))

/*
 * it doesn't make sense on ARM (currently the only user of __naked)
 * to trace naked functions because then mcount is called without
 * stack and frame pointer being set up and there is no chance to
 * restore the lr register to the value before mcount was called.
 *
 * The asm() bodies of naked functions often depend on standard calling
 * conventions, therefore they must be noinline and noclone.
 *
 * GCC 4.[56] currently fail to enforce this, so we must do so ourselves.
 * See GCC PR44290.
 */
#define __naked		__attribute__((naked)) noinline __noclone notrace

#define __noreturn	__attribute__((noreturn))

/*
 * From the GCC manual:
 *
 * Many functions have no effects except the return value and their
 * return value depends only on the parameters and/or global
 * variables.  Such a function can be subject to common subexpression
 * elimination and loop optimization just as an arithmetic operator
 * would be.
 * [...]
 */
#define __pure			__attribute__((pure))
#define __aligned(x)		__attribute__((aligned(x)))
#define __printf(a, b)		__attribute__((format(printf, a, b)))
#define __scanf(a, b)		__attribute__((format(scanf, a, b)))
#define __attribute_const__	__attribute__((__const__))
#define __maybe_unused		__attribute__((unused))
#define __always_unused		__attribute__((unused))

/* gcc version specific checks */

#if GCC_VERSION < 30200
# error Sorry, your compiler is too old - please upgrade it.
#endif

#if GCC_VERSION < 30300
# define __used			__attribute__((__unused__))
#else
# define __used			__attribute__((__used__))
#endif

#ifdef CONFIG_GCOV_KERNEL
# if GCC_VERSION < 30400
#   error "GCOV profiling support for gcc versions below 3.4 not included"
# endif /* __GNUC_MINOR__ */
#endif /* CONFIG_GCOV_KERNEL */

#if GCC_VERSION >= 30400
#define __must_check		__attribute__((warn_unused_result))
#endif

#if GCC_VERSION >= 40000

/* GCC 4.1.[01] miscompiles __weak */
#ifdef __KERNEL__
# if GCC_VERSION >= 40100 &&  GCC_VERSION <= 40101
#  error Your version of gcc miscompiles the __weak directive
# endif
#endif

#define __used			__attribute__((__used__))
#define __compiler_offsetof(a, b)					\
	__builtin_offsetof(a, b)

#if GCC_VERSION >= 40100 && GCC_VERSION < 40600
# define __compiletime_object_size(obj) __builtin_object_size(obj, 0)
#endif

#if GCC_VERSION >= 40300
/* Mark functions as cold. gcc will assume any path leading to a call
 * to them will be unlikely.  This means a lot of manual unlikely()s
 * are unnecessary now for any paths leading to the usual suspects
 * like BUG(), printk(), panic() etc. [but let's keep them for now for
 * older compilers]
 *
 * Early snapshots of gcc 4.3 don't support this and we can't detect this
 * in the preprocessor, but we can live with this because they're unreleased.
 * Maketime probing would be overkill here.
 *
 * gcc also has a __attribute__((__hot__)) to move hot functions into
 * a special section, but I don't see any sense in this right now in
 * the kernel context
 */
#define __cold			__attribute__((__cold__))

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

#define __compiletime_object_size(obj) __builtin_object_size(obj, 0)

#define __compiletime_warning(message) __attribute__((__warning__(message)))
#define __compiletime_error(message) __attribute__((__error__(message)))

#if defined(LATENT_ENTROPY_PLUGIN) && !defined(__CHECKER__)
#define __latent_entropy __attribute__((latent_entropy))
#endif

/*
 * calling noreturn functions, __builtin_unreachable() and __builtin_trap()
 * confuse the stack allocation in gcc, leading to overly large stack
 * frames, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82365
 *
 * Adding an empty inline assembly before it works around the problem
 */
#define barrier_before_unreachable() asm volatile("")

/*
 * Mark a position in code as unreachable.  This can be used to
 * suppress control flow warnings after asm blocks that transfer
 * control elsewhere.
 */
#define unreachable() \
	do {					\
		annotate_unreachable();		\
		barrier_before_unreachable();	\
		__builtin_unreachable();	\
	} while (0)

<<<<<<< HEAD
#if defined(RANDSTRUCT_PLUGIN) && !defined(__CHECKER__)
#define __randomize_layout __attribute__((randomize_layout))
#define __no_randomize_layout __attribute__((no_randomize_layout))
/* This anon struct can add padding, so only enable it under randstruct. */
#define randomized_struct_fields_start	struct {
#define randomized_struct_fields_end	} __randomize_layout;
=======
/* Mark a function definition as prohibited from being cloned. */
#define __noclone	__attribute__((__noclone__, __optimize__("no-tracer")))

#endif /* GCC_VERSION >= 40500 */

#if GCC_VERSION >= 40600
/*
 * When used with Link Time Optimization, gcc can optimize away C functions or
 * variables which are referenced only from assembly code.  __visible tells the
 * optimizer that something else uses this function or variable, thus preventing
 * this.
 */
#define __visible	__attribute__((externally_visible))
#endif


#if GCC_VERSION >= 40900 && !defined(__CHECKER__)
/*
 * __assume_aligned(n, k): Tell the optimizer that the returned
 * pointer can be assumed to be k modulo n. The second argument is
 * optional (default 0), so we use a variadic macro to make the
 * shorthand.
 *
 * Beware: Do not apply this to functions which may return
 * ERR_PTRs. Also, it is probably unwise to apply it to functions
 * returning extra information in the low bits (but in that case the
 * compiler should see some alignment anyway, when the return value is
 * massaged by 'flags = ptr & 3; ptr &= ~3;').
 */
#define __assume_aligned(a, ...) __attribute__((__assume_aligned__(a, ## __VA_ARGS__)))
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#endif

/*
 * GCC 'asm goto' miscompiles certain code sequences:
 *
 *   http://gcc.gnu.org/bugzilla/show_bug.cgi?id=58670
 *
 * Work it around via a compiler barrier quirk suggested by Jakub Jelinek.
 *
 * (asm goto is automatically volatile - the naming reflects this.)
 */
#define asm_volatile_goto(x...)	do { asm goto(x); asm (""); } while (0)

/*
 * sparse (__CHECKER__) pretends to be gcc, but can't do constant
 * folding in __builtin_bswap*() (yet), so don't set these for it.
 */
#if defined(CONFIG_ARCH_USE_BUILTIN_BSWAP) && !defined(__CHECKER__)
#define __HAVE_BUILTIN_BSWAP32__
#define __HAVE_BUILTIN_BSWAP64__
#if GCC_VERSION >= 40800
#define __HAVE_BUILTIN_BSWAP16__
#endif
#endif /* CONFIG_ARCH_USE_BUILTIN_BSWAP && !__CHECKER__ */

#if GCC_VERSION >= 70000
#define KASAN_ABI_VERSION 5
#elif GCC_VERSION >= 50000
#define KASAN_ABI_VERSION 4
#elif GCC_VERSION >= 40902
#define KASAN_ABI_VERSION 3
#endif

#if __has_attribute(__no_sanitize_address__)
#define __no_sanitize_address __attribute__((no_sanitize_address))
#else
#define __no_sanitize_address
#endif

#if GCC_VERSION >= 50100
#define COMPILER_HAS_GENERIC_BUILTIN_OVERFLOW 1
#endif

/*
 * Turn individual warnings and errors on and off locally, depending
 * on version.
 */
#define __diag_GCC(version, severity, s) \
	__diag_GCC_ ## version(__diag_GCC_ ## severity s)

/* Severity used in pragma directives */
#define __diag_GCC_ignore	ignored
#define __diag_GCC_warn		warning
#define __diag_GCC_error	error

#define __diag_str1(s)		#s
#define __diag_str(s)		__diag_str1(s)
#define __diag(s)		_Pragma(__diag_str(GCC diagnostic s))

#if GCC_VERSION >= 80000
#define __diag_GCC_8(s)		__diag(s)
#else
#define __diag_GCC_8(s)
#endif
