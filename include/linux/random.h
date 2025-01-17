/* SPDX-License-Identifier: GPL-2.0 */
/*
 * include/linux/random.h
 *
 * Include file for the random number generator.
 */
#ifndef _LINUX_RANDOM_H
#define _LINUX_RANDOM_H

#include <linux/list.h>
#include <linux/once.h>

#include <uapi/linux/random.h>

struct random_ready_callback {
	struct list_head list;
	void (*func)(struct random_ready_callback *rdy);
	struct module *owner;
};

extern void add_device_randomness(const void *, unsigned int);
extern void add_bootloader_randomness(const void *, unsigned int);

#if defined(LATENT_ENTROPY_PLUGIN) && !defined(__CHECKER__)
static inline void add_latent_entropy(void)
{
	add_device_randomness((const void *)&latent_entropy,
			      sizeof(latent_entropy));
}
#else
static inline void add_latent_entropy(void) {}
#endif

extern void add_input_randomness(unsigned int type, unsigned int code,
				 unsigned int value) __latent_entropy;
extern void add_interrupt_randomness(int irq, int irq_flags) __latent_entropy;

extern void get_random_bytes(void *buf, int nbytes);
extern int wait_for_random_bytes(void);
extern int __init rand_initialize(void);
extern bool rng_is_initialized(void);
extern int add_random_ready_callback(struct random_ready_callback *rdy);
extern void del_random_ready_callback(struct random_ready_callback *rdy);
extern int __must_check get_random_bytes_arch(void *buf, int nbytes);

#ifndef MODULE
extern const struct file_operations random_fops, urandom_fops;
#endif

<<<<<<< HEAD
u32 get_random_u32(void);
u64 get_random_u64(void);
static inline unsigned int get_random_int(void)
=======
unsigned int get_random_int(void);
unsigned long get_random_long(void);
unsigned long randomize_range(unsigned long start, unsigned long end, unsigned long len);

u32 prandom_u32(void);
void prandom_bytes(void *buf, size_t nbytes);
void prandom_seed(u32 seed);
void prandom_reseed_late(void);

struct rnd_state {
	__u32 s1, s2, s3, s4;
};

u32 prandom_u32_state(struct rnd_state *state);
void prandom_bytes_state(struct rnd_state *state, void *buf, size_t nbytes);
void prandom_seed_full_state(struct rnd_state __percpu *pcpu_state);

#define prandom_init_once(pcpu_state)			\
	DO_ONCE(prandom_seed_full_state, (pcpu_state))

/**
 * prandom_u32_max - returns a pseudo-random number in interval [0, ep_ro)
 * @ep_ro: right open interval endpoint
 *
 * Returns a pseudo-random number that is in interval [0, ep_ro). Note
 * that the result depends on PRNG being well distributed in [0, ~0U]
 * u32 space. Here we use maximally equidistributed combined Tausworthe
 * generator, that is, prandom_u32(). This is useful when requesting a
 * random index of an array containing ep_ro elements, for example.
 *
 * Returns: pseudo-random number in interval [0, ep_ro)
 */
static inline u32 prandom_u32_max(u32 ep_ro)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
{
	return get_random_u32();
}
static inline unsigned long get_random_long(void)
{
#if BITS_PER_LONG == 64
	return get_random_u64();
#else
	return get_random_u32();
#endif
}

/*
 * On 64-bit architectures, protect against non-terminated C string overflows
 * by zeroing out the first byte of the canary; this leaves 56 bits of entropy.
 */
#ifdef CONFIG_64BIT
# ifdef __LITTLE_ENDIAN
#  define CANARY_MASK 0xffffffffffffff00UL
# else /* big endian, 64 bits: */
#  define CANARY_MASK 0x00ffffffffffffffUL
# endif
#else /* 32 bits: */
# define CANARY_MASK 0xffffffffUL
#endif

static inline unsigned long get_random_canary(void)
{
	unsigned long val = get_random_long();

	return val & CANARY_MASK;
}

/* Calls wait_for_random_bytes() and then calls get_random_bytes(buf, nbytes).
 * Returns the result of the call to wait_for_random_bytes. */
static inline int get_random_bytes_wait(void *buf, int nbytes)
{
	int ret = wait_for_random_bytes();
	get_random_bytes(buf, nbytes);
	return ret;
}

#define declare_get_random_var_wait(var) \
	static inline int get_random_ ## var ## _wait(var *out) { \
		int ret = wait_for_random_bytes(); \
		if (unlikely(ret)) \
			return ret; \
		*out = get_random_ ## var(); \
		return 0; \
	}
declare_get_random_var_wait(u32)
declare_get_random_var_wait(u64)
declare_get_random_var_wait(int)
declare_get_random_var_wait(long)
#undef declare_get_random_var

unsigned long randomize_page(unsigned long start, unsigned long range);

/*
 * This is designed to be standalone for just prandom
 * users, but for now we include it from <linux/random.h>
 * for legacy reasons.
 */
#include <linux/prandom.h>

#ifdef CONFIG_ARCH_RANDOM
# include <asm/archrandom.h>
#else
static inline bool __must_check arch_get_random_long(unsigned long *v)
{
	return false;
}
static inline bool __must_check arch_get_random_int(unsigned int *v)
{
	return false;
}
static inline bool __must_check arch_get_random_seed_long(unsigned long *v)
{
	return false;
}
static inline bool __must_check arch_get_random_seed_int(unsigned int *v)
{
	return false;
}
#endif

#endif /* _LINUX_RANDOM_H */
