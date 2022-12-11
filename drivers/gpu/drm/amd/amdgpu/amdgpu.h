/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */
#ifndef __AMDGPU_H__
#define __AMDGPU_H__

#include "amdgpu_ctx.h"

#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/rbtree.h>
#include <linux/hashtable.h>
#include <linux/dma-fence.h>

#include <drm/ttm/ttm_bo_api.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_module.h>
#include <drm/ttm/ttm_execbuf_util.h>

#include <drm/amdgpu_drm.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/gpu_scheduler.h>

#include <kgd_kfd_interface.h>
#include "dm_pp_interface.h"
#include "kgd_pp_interface.h"

#include "amd_shared.h"
#include "amdgpu_mode.h"
#include "amdgpu_ih.h"
#include "amdgpu_irq.h"
#include "amdgpu_ucode.h"
#include "amdgpu_ttm.h"
#include "amdgpu_psp.h"
#include "amdgpu_gds.h"
#include "amdgpu_sync.h"
#include "amdgpu_ring.h"
#include "amdgpu_vm.h"
#include "amdgpu_dpm.h"
#include "amdgpu_acp.h"
#include "amdgpu_uvd.h"
#include "amdgpu_vce.h"
#include "amdgpu_vcn.h"
#include "amdgpu_mn.h"
#include "amdgpu_gmc.h"
#include "amdgpu_gfx.h"
#include "amdgpu_sdma.h"
#include "amdgpu_dm.h"
#include "amdgpu_virt.h"
#include "amdgpu_csa.h"
#include "amdgpu_gart.h"
#include "amdgpu_debugfs.h"
#include "amdgpu_job.h"
#include "amdgpu_bo_list.h"
#include "amdgpu_gem.h"
#include "amdgpu_doorbell.h"
#include "amdgpu_amdkfd.h"
#include "amdgpu_smu.h"
#include "amdgpu_discovery.h"
#include "amdgpu_mes.h"
#include "amdgpu_umc.h"
#include "amdgpu_mmhub.h"

#define MAX_GPU_INSTANCE		16

struct amdgpu_gpu_instance
{
	struct amdgpu_device		*adev;
	int				mgpu_fan_enabled;
};

struct amdgpu_mgpu_info
{
	struct amdgpu_gpu_instance	gpu_ins[MAX_GPU_INSTANCE];
	struct mutex			mutex;
	uint32_t			num_gpu;
	uint32_t			num_dgpu;
	uint32_t			num_apu;
};

/*
 * Modules parameters.
 */
extern int amdgpu_modeset;
extern int amdgpu_vram_limit;
extern int amdgpu_vis_vram_limit;
extern int amdgpu_gart_size;
extern int amdgpu_gtt_size;
extern int amdgpu_moverate;
extern int amdgpu_benchmarking;
extern int amdgpu_testing;
extern int amdgpu_audio;
extern int amdgpu_disp_priority;
extern int amdgpu_hw_i2c;
extern int amdgpu_pcie_gen2;
extern int amdgpu_msi;
extern int amdgpu_dpm;
extern int amdgpu_fw_load_type;
extern int amdgpu_aspm;
extern int amdgpu_runtime_pm;
extern uint amdgpu_ip_block_mask;
extern int amdgpu_bapm;
extern int amdgpu_deep_color;
extern int amdgpu_vm_size;
extern int amdgpu_vm_block_size;
extern int amdgpu_vm_fragment_size;
extern int amdgpu_vm_fault_stop;
extern int amdgpu_vm_debug;
extern int amdgpu_vm_update_mode;
extern int amdgpu_dc;
extern int amdgpu_sched_jobs;
extern int amdgpu_sched_hw_submission;
extern uint amdgpu_pcie_gen_cap;
extern uint amdgpu_pcie_lane_cap;
extern uint amdgpu_cg_mask;
extern uint amdgpu_pg_mask;
extern uint amdgpu_sdma_phase_quantum;
extern char *amdgpu_disable_cu;
extern char *amdgpu_virtual_display;
extern uint amdgpu_pp_feature_mask;
extern int amdgpu_ngg;
extern int amdgpu_prim_buf_per_se;
extern int amdgpu_pos_buf_per_se;
extern int amdgpu_cntl_sb_buf_per_se;
extern int amdgpu_param_buf_per_se;
extern int amdgpu_job_hang_limit;
extern int amdgpu_lbpw;
extern int amdgpu_compute_multipipe;
extern int amdgpu_gpu_recovery;
extern int amdgpu_emu_mode;
extern uint amdgpu_smu_memory_pool_size;
extern uint amdgpu_dc_feature_mask;
extern uint amdgpu_dm_abm_level;
extern struct amdgpu_mgpu_info mgpu_info;
extern int amdgpu_ras_enable;
extern uint amdgpu_ras_mask;
extern int amdgpu_async_gfx_ring;
extern int amdgpu_mcbp;
extern int amdgpu_discovery;
extern int amdgpu_mes;
extern int amdgpu_noretry;

#ifdef CONFIG_DRM_AMDGPU_SI
extern int amdgpu_si_support;
#endif
#ifdef CONFIG_DRM_AMDGPU_CIK
extern int amdgpu_cik_support;
#endif

#define AMDGPU_VM_MAX_NUM_CTX			4096
#define AMDGPU_SG_THRESHOLD			(256*1024*1024)
#define AMDGPU_DEFAULT_GTT_SIZE_MB		3072ULL /* 3GB by default */
#define AMDGPU_WAIT_IDLE_TIMEOUT_IN_MS	        3000
#define AMDGPU_MAX_USEC_TIMEOUT			100000	/* 100 ms */
#define AMDGPU_FENCE_JIFFIES_TIMEOUT		(HZ / 2)
/* AMDGPU_IB_POOL_SIZE must be a power of 2 */
#define AMDGPU_IB_POOL_SIZE			16
#define AMDGPU_DEBUGFS_MAX_COMPONENTS		32
#define AMDGPUFB_CONN_LIMIT			4
#define AMDGPU_BIOS_NUM_SCRATCH			16

/* hard reset data */
#define AMDGPU_ASIC_RESET_DATA                  0x39d5e86b

/* reset flags */
#define AMDGPU_RESET_GFX			(1 << 0)
#define AMDGPU_RESET_COMPUTE			(1 << 1)
#define AMDGPU_RESET_DMA			(1 << 2)
#define AMDGPU_RESET_CP				(1 << 3)
#define AMDGPU_RESET_GRBM			(1 << 4)
#define AMDGPU_RESET_DMA1			(1 << 5)
#define AMDGPU_RESET_RLC			(1 << 6)
#define AMDGPU_RESET_SEM			(1 << 7)
#define AMDGPU_RESET_IH				(1 << 8)
#define AMDGPU_RESET_VMC			(1 << 9)
#define AMDGPU_RESET_MC				(1 << 10)
#define AMDGPU_RESET_DISPLAY			(1 << 11)
#define AMDGPU_RESET_UVD			(1 << 12)
#define AMDGPU_RESET_VCE			(1 << 13)
#define AMDGPU_RESET_VCE1			(1 << 14)

/* max cursor sizes (in pixels) */
#define CIK_CURSOR_WIDTH 128
#define CIK_CURSOR_HEIGHT 128

struct amdgpu_device;
struct amdgpu_ib;
struct amdgpu_cs_parser;
struct amdgpu_job;
struct amdgpu_irq_src;
struct amdgpu_fpriv;
struct amdgpu_bo_va_mapping;
struct amdgpu_atif;
struct kfd_vm_fault_info;

enum amdgpu_cp_irq {
	AMDGPU_CP_IRQ_GFX_ME0_PIPE0_EOP = 0,
	AMDGPU_CP_IRQ_GFX_ME0_PIPE1_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE0_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE1_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE2_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE3_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC2_PIPE0_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC2_PIPE1_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC2_PIPE2_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC2_PIPE3_EOP,

	AMDGPU_CP_IRQ_LAST
};

enum amdgpu_thermal_irq {
	AMDGPU_THERMAL_IRQ_LOW_TO_HIGH = 0,
	AMDGPU_THERMAL_IRQ_HIGH_TO_LOW,

	AMDGPU_THERMAL_IRQ_LAST
};

enum amdgpu_kiq_irq {
	AMDGPU_CP_KIQ_IRQ_DRIVER0 = 0,
	AMDGPU_CP_KIQ_IRQ_LAST
};

#define MAX_KIQ_REG_WAIT       5000 /* in usecs, 5ms */
#define MAX_KIQ_REG_BAILOUT_INTERVAL   5 /* in msecs, 5ms */
#define MAX_KIQ_REG_TRY 80 /* 20 -> 80 */

int amdgpu_device_ip_set_clockgating_state(void *dev,
					   enum amd_ip_block_type block_type,
					   enum amd_clockgating_state state);
int amdgpu_device_ip_set_powergating_state(void *dev,
					   enum amd_ip_block_type block_type,
					   enum amd_powergating_state state);
void amdgpu_device_ip_get_clockgating_state(struct amdgpu_device *adev,
					    u32 *flags);
int amdgpu_device_ip_wait_for_idle(struct amdgpu_device *adev,
				   enum amd_ip_block_type block_type);
bool amdgpu_device_ip_is_idle(struct amdgpu_device *adev,
			      enum amd_ip_block_type block_type);

#define AMDGPU_MAX_IP_NUM 16

struct amdgpu_ip_block_status {
	bool valid;
	bool sw;
	bool hw;
	bool late_initialized;
	bool hang;
};

struct amdgpu_ip_block_version {
	const enum amd_ip_block_type type;
	const u32 major;
	const u32 minor;
	const u32 rev;
	const struct amd_ip_funcs *funcs;
};

struct amdgpu_ip_block {
	struct amdgpu_ip_block_status status;
	const struct amdgpu_ip_block_version *version;
};

int amdgpu_device_ip_block_version_cmp(struct amdgpu_device *adev,
				       enum amd_ip_block_type type,
				       u32 major, u32 minor);

struct amdgpu_ip_block *
amdgpu_device_ip_get_ip_block(struct amdgpu_device *adev,
			      enum amd_ip_block_type type);

int amdgpu_device_ip_block_add(struct amdgpu_device *adev,
			       const struct amdgpu_ip_block_version *ip_block_version);

/*
 * BIOS.
 */
bool amdgpu_get_bios(struct amdgpu_device *adev);
bool amdgpu_read_bios(struct amdgpu_device *adev);

/*
 * Clocks
 */

#define AMDGPU_MAX_PPLL 3

struct amdgpu_clock {
	struct amdgpu_pll ppll[AMDGPU_MAX_PPLL];
	struct amdgpu_pll spll;
	struct amdgpu_pll mpll;
	/* 10 Khz units */
	uint32_t default_mclk;
	uint32_t default_sclk;
	uint32_t default_dispclk;
	uint32_t current_dispclk;
	uint32_t dp_extclk;
	uint32_t max_pixel_clock;
};

<<<<<<< HEAD
=======
/*
 * Fences.
 */
struct amdgpu_fence_driver {
	uint64_t			gpu_addr;
	volatile uint32_t		*cpu_addr;
	/* sync_seq is protected by ring emission lock */
	uint64_t			sync_seq[AMDGPU_MAX_RINGS];
	atomic64_t			last_seq;
	bool				initialized;
	struct amdgpu_irq_src		*irq_src;
	unsigned			irq_type;
	struct timer_list		fallback_timer;
	wait_queue_head_t		fence_queue;
};

/* some special values for the owner field */
#define AMDGPU_FENCE_OWNER_UNDEFINED	((void*)0ul)
#define AMDGPU_FENCE_OWNER_VM		((void*)1ul)

#define AMDGPU_FENCE_FLAG_64BIT         (1 << 0)
#define AMDGPU_FENCE_FLAG_INT           (1 << 1)

struct amdgpu_fence {
	struct fence base;

	/* RB, DMA, etc. */
	struct amdgpu_ring		*ring;
	uint64_t			seq;

	/* filp or special value for fence creator */
	void				*owner;

	wait_queue_t			fence_wake;
};

struct amdgpu_user_fence {
	/* write-back bo */
	struct amdgpu_bo 	*bo;
	/* write-back address offset to bo start */
	uint32_t                offset;
};

int amdgpu_fence_driver_init(struct amdgpu_device *adev);
void amdgpu_fence_driver_fini(struct amdgpu_device *adev);
void amdgpu_fence_driver_force_completion(struct amdgpu_device *adev);

int amdgpu_fence_driver_init_ring(struct amdgpu_ring *ring);
int amdgpu_fence_driver_start_ring(struct amdgpu_ring *ring,
				   struct amdgpu_irq_src *irq_src,
				   unsigned irq_type);
void amdgpu_fence_driver_suspend(struct amdgpu_device *adev);
void amdgpu_fence_driver_resume(struct amdgpu_device *adev);
int amdgpu_fence_emit(struct amdgpu_ring *ring, void *owner,
		      struct amdgpu_fence **fence);
void amdgpu_fence_process(struct amdgpu_ring *ring);
int amdgpu_fence_wait_next(struct amdgpu_ring *ring);
int amdgpu_fence_wait_empty(struct amdgpu_ring *ring);
unsigned amdgpu_fence_count_emitted(struct amdgpu_ring *ring);

bool amdgpu_fence_need_sync(struct amdgpu_fence *fence,
			    struct amdgpu_ring *ring);
void amdgpu_fence_note_sync(struct amdgpu_fence *fence,
			    struct amdgpu_ring *ring);

/*
 * TTM.
 */
struct amdgpu_mman {
	struct ttm_bo_global_ref        bo_global_ref;
	struct drm_global_reference	mem_global_ref;
	struct ttm_bo_device		bdev;
	bool				mem_global_referenced;
	bool				initialized;

#if defined(CONFIG_DEBUG_FS)
	struct dentry			*vram;
	struct dentry			*gtt;
#endif

	/* buffer handling */
	const struct amdgpu_buffer_funcs	*buffer_funcs;
	struct amdgpu_ring			*buffer_funcs_ring;
};

int amdgpu_copy_buffer(struct amdgpu_ring *ring,
		       uint64_t src_offset,
		       uint64_t dst_offset,
		       uint32_t byte_count,
		       struct reservation_object *resv,
		       struct fence **fence);
int amdgpu_mmap(struct file *filp, struct vm_area_struct *vma);

struct amdgpu_bo_list_entry {
	struct amdgpu_bo		*robj;
	struct ttm_validate_buffer	tv;
	struct amdgpu_bo_va		*bo_va;
	unsigned			prefered_domains;
	unsigned			allowed_domains;
	uint32_t			priority;
};

struct amdgpu_bo_va_mapping {
	struct list_head		list;
	struct interval_tree_node	it;
	uint64_t			offset;
	uint32_t			flags;
};

/* bo virtual addresses in a specific vm */
struct amdgpu_bo_va {
	struct mutex		        mutex;
	/* protected by bo being reserved */
	struct list_head		bo_list;
	struct fence		        *last_pt_update;
	unsigned			ref_count;

	/* protected by vm mutex and spinlock */
	struct list_head		vm_status;

	/* mappings for this bo_va */
	struct list_head		invalids;
	struct list_head		valids;

	/* constant after initialization */
	struct amdgpu_vm		*vm;
	struct amdgpu_bo		*bo;
};

#define AMDGPU_GEM_DOMAIN_MAX		0x3

struct amdgpu_bo {
	/* Protected by gem.mutex */
	struct list_head		list;
	/* Protected by tbo.reserved */
	u32				initial_domain;
	struct ttm_place		placements[AMDGPU_GEM_DOMAIN_MAX + 1];
	struct ttm_placement		placement;
	struct ttm_buffer_object	tbo;
	struct ttm_bo_kmap_obj		kmap;
	u64				flags;
	unsigned			pin_count;
	void				*kptr;
	u64				tiling_flags;
	u64				metadata_flags;
	void				*metadata;
	u32				metadata_size;
	unsigned			prime_shared_count;
	/* list of all virtual address to which this bo
	 * is associated to
	 */
	struct list_head		va;
	/* Constant after initialization */
	struct amdgpu_device		*adev;
	struct drm_gem_object		gem_base;
	struct amdgpu_bo		*parent;

	struct ttm_bo_kmap_obj		dma_buf_vmap;
	pid_t				pid;
	struct amdgpu_mn		*mn;
	struct list_head		mn_list;
};
#define gem_to_amdgpu_bo(gobj) container_of((gobj), struct amdgpu_bo, gem_base)

void amdgpu_gem_object_free(struct drm_gem_object *obj);
int amdgpu_gem_object_open(struct drm_gem_object *obj,
				struct drm_file *file_priv);
void amdgpu_gem_object_close(struct drm_gem_object *obj,
				struct drm_file *file_priv);
unsigned long amdgpu_gem_timeout(uint64_t timeout_ns);
struct sg_table *amdgpu_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *amdgpu_gem_prime_import_sg_table(struct drm_device *dev,
							struct dma_buf_attachment *attach,
							struct sg_table *sg);
struct dma_buf *amdgpu_gem_prime_export(struct drm_device *dev,
					struct drm_gem_object *gobj,
					int flags);
int amdgpu_gem_prime_pin(struct drm_gem_object *obj);
void amdgpu_gem_prime_unpin(struct drm_gem_object *obj);
struct reservation_object *amdgpu_gem_prime_res_obj(struct drm_gem_object *);
void *amdgpu_gem_prime_vmap(struct drm_gem_object *obj);
void amdgpu_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
int amdgpu_gem_debugfs_init(struct amdgpu_device *adev);

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
/* sub-allocation manager, it has to be protected by another lock.
 * By conception this is an helper for other part of the driver
 * like the indirect buffer or semaphore, which both have their
 * locking.
 *
 * Principe is simple, we keep a list of sub allocation in offset
 * order (first entry has offset == 0, last entry has the highest
 * offset).
 *
 * When allocating new object we first check if there is room at
 * the end total_size - (last_object_offset + last_object_size) >=
 * alloc_size. If so we allocate new object there.
 *
 * When there is not enough room at the end, we start waiting for
 * each sub object until we reach object_offset+object_size >=
 * alloc_size, this object then become the sub object we return.
 *
 * Alignment can't be bigger than page size.
 *
 * Hole are not considered for allocation to keep things simple.
 * Assumption is that there won't be hole (all object on same
 * alignment).
 */

#define AMDGPU_SA_NUM_FENCE_LISTS	32

struct amdgpu_sa_manager {
	wait_queue_head_t	wq;
	struct amdgpu_bo	*bo;
	struct list_head	*hole;
	struct list_head	flist[AMDGPU_SA_NUM_FENCE_LISTS];
	struct list_head	olist;
	unsigned		size;
	uint64_t		gpu_addr;
	void			*cpu_ptr;
	uint32_t		domain;
	uint32_t		align;
};

/* sub-allocation buffer */
struct amdgpu_sa_bo {
	struct list_head		olist;
	struct list_head		flist;
	struct amdgpu_sa_manager	*manager;
	unsigned			soffset;
	unsigned			eoffset;
	struct dma_fence	        *fence;
};

<<<<<<< HEAD
int amdgpu_fence_slab_init(void);
void amdgpu_fence_slab_fini(void);
=======
/*
 * GEM objects.
 */
struct amdgpu_gem {
	struct mutex		mutex;
	struct list_head	objects;
};

int amdgpu_gem_init(struct amdgpu_device *adev);
void amdgpu_gem_fini(struct amdgpu_device *adev);
int amdgpu_gem_object_create(struct amdgpu_device *adev, unsigned long size,
				int alignment, u32 initial_domain,
				u64 flags, bool kernel,
				struct drm_gem_object **obj);

int amdgpu_mode_dumb_create(struct drm_file *file_priv,
			    struct drm_device *dev,
			    struct drm_mode_create_dumb *args);
int amdgpu_mode_dumb_mmap(struct drm_file *filp,
			  struct drm_device *dev,
			  uint32_t handle, uint64_t *offset_p);

/*
 * Semaphores.
 */
struct amdgpu_semaphore {
	struct amdgpu_sa_bo	*sa_bo;
	signed			waiters;
	uint64_t		gpu_addr;
};

int amdgpu_semaphore_create(struct amdgpu_device *adev,
			    struct amdgpu_semaphore **semaphore);
bool amdgpu_semaphore_emit_signal(struct amdgpu_ring *ring,
				  struct amdgpu_semaphore *semaphore);
bool amdgpu_semaphore_emit_wait(struct amdgpu_ring *ring,
				struct amdgpu_semaphore *semaphore);
void amdgpu_semaphore_free(struct amdgpu_device *adev,
			   struct amdgpu_semaphore **semaphore,
			   struct fence *fence);

/*
 * Synchronization
 */
struct amdgpu_sync {
	struct amdgpu_semaphore *semaphores[AMDGPU_NUM_SYNCS];
	struct fence		*sync_to[AMDGPU_MAX_RINGS];
	DECLARE_HASHTABLE(fences, 4);
	struct fence	        *last_vm_update;
};

void amdgpu_sync_create(struct amdgpu_sync *sync);
int amdgpu_sync_fence(struct amdgpu_device *adev, struct amdgpu_sync *sync,
		      struct fence *f);
int amdgpu_sync_resv(struct amdgpu_device *adev,
		     struct amdgpu_sync *sync,
		     struct reservation_object *resv,
		     void *owner);
int amdgpu_sync_rings(struct amdgpu_sync *sync,
		      struct amdgpu_ring *ring);
struct fence *amdgpu_sync_get_fence(struct amdgpu_sync *sync);
int amdgpu_sync_wait(struct amdgpu_sync *sync);
void amdgpu_sync_free(struct amdgpu_device *adev, struct amdgpu_sync *sync,
		      struct fence *fence);

/*
 * GART structures, functions & helpers
 */
struct amdgpu_mc;

#define AMDGPU_GPU_PAGE_SIZE 4096
#define AMDGPU_GPU_PAGE_MASK (AMDGPU_GPU_PAGE_SIZE - 1)
#define AMDGPU_GPU_PAGE_SHIFT 12
#define AMDGPU_GPU_PAGE_ALIGN(a) (((a) + AMDGPU_GPU_PAGE_MASK) & ~AMDGPU_GPU_PAGE_MASK)

struct amdgpu_gart {
	dma_addr_t			table_addr;
	struct amdgpu_bo		*robj;
	void				*ptr;
	unsigned			num_gpu_pages;
	unsigned			num_cpu_pages;
	unsigned			table_size;
	struct page			**pages;
	dma_addr_t			*pages_addr;
	bool				ready;
	const struct amdgpu_gart_funcs *gart_funcs;
};

int amdgpu_gart_table_ram_alloc(struct amdgpu_device *adev);
void amdgpu_gart_table_ram_free(struct amdgpu_device *adev);
int amdgpu_gart_table_vram_alloc(struct amdgpu_device *adev);
void amdgpu_gart_table_vram_free(struct amdgpu_device *adev);
int amdgpu_gart_table_vram_pin(struct amdgpu_device *adev);
void amdgpu_gart_table_vram_unpin(struct amdgpu_device *adev);
int amdgpu_gart_init(struct amdgpu_device *adev);
void amdgpu_gart_fini(struct amdgpu_device *adev);
void amdgpu_gart_unbind(struct amdgpu_device *adev, uint64_t offset,
			int pages);
int amdgpu_gart_bind(struct amdgpu_device *adev, uint64_t offset,
		     int pages, struct page **pagelist,
		     dma_addr_t *dma_addr, uint32_t flags);

/*
 * GPU MC structures, functions & helpers
 */
struct amdgpu_mc {
	resource_size_t		aper_size;
	resource_size_t		aper_base;
	resource_size_t		agp_base;
	/* for some chips with <= 32MB we need to lie
	 * about vram size near mc fb location */
	u64			mc_vram_size;
	u64			visible_vram_size;
	u64			gtt_size;
	u64			gtt_start;
	u64			gtt_end;
	u64			vram_start;
	u64			vram_end;
	unsigned		vram_width;
	u64			real_vram_size;
	int			vram_mtrr;
	u64                     gtt_base_align;
	u64                     mc_mask;
	const struct firmware   *fw;	/* MC firmware */
	uint32_t                fw_version;
	struct amdgpu_irq_src	vm_fault;
	uint32_t		vram_type;
};

/*
 * GPU doorbell structures, functions & helpers
 */
typedef enum _AMDGPU_DOORBELL_ASSIGNMENT
{
	AMDGPU_DOORBELL_KIQ                     = 0x000,
	AMDGPU_DOORBELL_HIQ                     = 0x001,
	AMDGPU_DOORBELL_DIQ                     = 0x002,
	AMDGPU_DOORBELL_MEC_RING0               = 0x010,
	AMDGPU_DOORBELL_MEC_RING1               = 0x011,
	AMDGPU_DOORBELL_MEC_RING2               = 0x012,
	AMDGPU_DOORBELL_MEC_RING3               = 0x013,
	AMDGPU_DOORBELL_MEC_RING4               = 0x014,
	AMDGPU_DOORBELL_MEC_RING5               = 0x015,
	AMDGPU_DOORBELL_MEC_RING6               = 0x016,
	AMDGPU_DOORBELL_MEC_RING7               = 0x017,
	AMDGPU_DOORBELL_GFX_RING0               = 0x020,
	AMDGPU_DOORBELL_sDMA_ENGINE0            = 0x1E0,
	AMDGPU_DOORBELL_sDMA_ENGINE1            = 0x1E1,
	AMDGPU_DOORBELL_IH                      = 0x1E8,
	AMDGPU_DOORBELL_MAX_ASSIGNMENT          = 0x3FF,
	AMDGPU_DOORBELL_INVALID                 = 0xFFFF
} AMDGPU_DOORBELL_ASSIGNMENT;

struct amdgpu_doorbell {
	/* doorbell mmio */
	resource_size_t		base;
	resource_size_t		size;
	u32 __iomem		*ptr;
	u32			num_doorbells;	/* Number of doorbells actually reserved for amdgpu. */
};

void amdgpu_doorbell_get_kfd_info(struct amdgpu_device *adev,
				phys_addr_t *aperture_base,
				size_t *aperture_size,
				size_t *start_offset);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

/*
 * IRQS.
 */

struct amdgpu_flip_work {
	struct delayed_work		flip_work;
	struct work_struct		unpin_work;
	struct amdgpu_device		*adev;
	int				crtc_id;
	u32				target_vblank;
	uint64_t			base;
	struct drm_pending_vblank_event *event;
	struct amdgpu_bo		*old_abo;
	struct dma_fence		*excl;
	unsigned			shared_count;
	struct dma_fence		**shared;
	struct dma_fence_cb		cb;
	bool				async;
};


/*
 * CP & rings.
 */

struct amdgpu_ib {
	struct amdgpu_sa_bo		*sa_bo;
	uint32_t			length_dw;
	uint64_t			gpu_addr;
	uint32_t			*ptr;
	uint32_t			flags;
};

extern const struct drm_sched_backend_ops amdgpu_sched_ops;

/*
 * file private structure
 */

struct amdgpu_fpriv {
	struct amdgpu_vm	vm;
	struct amdgpu_bo_va	*prt_va;
	struct amdgpu_bo_va	*csa_va;
	struct mutex		bo_list_lock;
	struct idr		bo_list_handles;
	struct amdgpu_ctx_mgr	ctx_mgr;
};

int amdgpu_file_to_fpriv(struct file *filp, struct amdgpu_fpriv **fpriv);
int amdgpu_device_get_job_timeout_settings(struct amdgpu_device *adev);

int amdgpu_ib_get(struct amdgpu_device *adev, struct amdgpu_vm *vm,
		  unsigned size, struct amdgpu_ib *ib);
void amdgpu_ib_free(struct amdgpu_device *adev, struct amdgpu_ib *ib,
		    struct dma_fence *f);
int amdgpu_ib_schedule(struct amdgpu_ring *ring, unsigned num_ibs,
		       struct amdgpu_ib *ibs, struct amdgpu_job *job,
		       struct dma_fence **f);
int amdgpu_ib_pool_init(struct amdgpu_device *adev);
void amdgpu_ib_pool_fini(struct amdgpu_device *adev);
int amdgpu_ib_ring_tests(struct amdgpu_device *adev);

/*
 * CS.
 */
struct amdgpu_cs_chunk {
	uint32_t		chunk_id;
	uint32_t		length_dw;
	void			*kdata;
};

struct amdgpu_cs_post_dep {
	struct drm_syncobj *syncobj;
	struct dma_fence_chain *chain;
	u64 point;
};

struct amdgpu_cs_parser {
	struct amdgpu_device	*adev;
	struct drm_file		*filp;
	struct amdgpu_ctx	*ctx;

	/* chunks */
	unsigned		nchunks;
	struct amdgpu_cs_chunk	*chunks;

	/* scheduler job object */
	struct amdgpu_job	*job;
	struct drm_sched_entity	*entity;

	/* buffer objects */
	struct ww_acquire_ctx		ticket;
	struct amdgpu_bo_list		*bo_list;
	struct amdgpu_mn		*mn;
	struct amdgpu_bo_list_entry	vm_pd;
	struct list_head		validated;
	struct dma_fence		*fence;
	uint64_t			bytes_moved_threshold;
	uint64_t			bytes_moved_vis_threshold;
	uint64_t			bytes_moved;
	uint64_t			bytes_moved_vis;
	struct amdgpu_bo_list_entry	*evictable;

	/* user fence */
	struct amdgpu_bo_list_entry	uf_entry;

	unsigned			num_post_deps;
	struct amdgpu_cs_post_dep	*post_deps;
};

static inline u32 amdgpu_get_ib_value(struct amdgpu_cs_parser *p,
				      uint32_t ib_idx, int idx)
{
	return p->job->ibs[ib_idx].ptr[idx];
}

static inline void amdgpu_set_ib_value(struct amdgpu_cs_parser *p,
				       uint32_t ib_idx, int idx,
				       uint32_t value)
{
	p->job->ibs[ib_idx].ptr[idx] = value;
}

/*
 * Writeback
 */
#define AMDGPU_MAX_WB 128	/* Reserve at most 128 WB slots for amdgpu-owned rings. */

struct amdgpu_wb {
	struct amdgpu_bo	*wb_obj;
	volatile uint32_t	*wb;
	uint64_t		gpu_addr;
	u32			num_wb;	/* Number of wb slots actually reserved for amdgpu. */
	unsigned long		used[DIV_ROUND_UP(AMDGPU_MAX_WB, BITS_PER_LONG)];
};

<<<<<<< HEAD
int amdgpu_device_wb_get(struct amdgpu_device *adev, u32 *wb);
void amdgpu_device_wb_free(struct amdgpu_device *adev, u32 wb);
=======
int amdgpu_wb_get(struct amdgpu_device *adev, u32 *wb);
void amdgpu_wb_free(struct amdgpu_device *adev, u32 wb);

/**
 * struct amdgpu_pm - power management datas
 * It keeps track of various data needed to take powermanagement decision.
 */

enum amdgpu_pm_state_type {
	/* not used for dpm */
	POWER_STATE_TYPE_DEFAULT,
	POWER_STATE_TYPE_POWERSAVE,
	/* user selectable states */
	POWER_STATE_TYPE_BATTERY,
	POWER_STATE_TYPE_BALANCED,
	POWER_STATE_TYPE_PERFORMANCE,
	/* internal states */
	POWER_STATE_TYPE_INTERNAL_UVD,
	POWER_STATE_TYPE_INTERNAL_UVD_SD,
	POWER_STATE_TYPE_INTERNAL_UVD_HD,
	POWER_STATE_TYPE_INTERNAL_UVD_HD2,
	POWER_STATE_TYPE_INTERNAL_UVD_MVC,
	POWER_STATE_TYPE_INTERNAL_BOOT,
	POWER_STATE_TYPE_INTERNAL_THERMAL,
	POWER_STATE_TYPE_INTERNAL_ACPI,
	POWER_STATE_TYPE_INTERNAL_ULV,
	POWER_STATE_TYPE_INTERNAL_3DPERF,
};

enum amdgpu_int_thermal_type {
	THERMAL_TYPE_NONE,
	THERMAL_TYPE_EXTERNAL,
	THERMAL_TYPE_EXTERNAL_GPIO,
	THERMAL_TYPE_RV6XX,
	THERMAL_TYPE_RV770,
	THERMAL_TYPE_ADT7473_WITH_INTERNAL,
	THERMAL_TYPE_EVERGREEN,
	THERMAL_TYPE_SUMO,
	THERMAL_TYPE_NI,
	THERMAL_TYPE_SI,
	THERMAL_TYPE_EMC2103_WITH_INTERNAL,
	THERMAL_TYPE_CI,
	THERMAL_TYPE_KV,
};

enum amdgpu_dpm_auto_throttle_src {
	AMDGPU_DPM_AUTO_THROTTLE_SRC_THERMAL,
	AMDGPU_DPM_AUTO_THROTTLE_SRC_EXTERNAL
};

enum amdgpu_dpm_event_src {
	AMDGPU_DPM_EVENT_SRC_ANALOG = 0,
	AMDGPU_DPM_EVENT_SRC_EXTERNAL = 1,
	AMDGPU_DPM_EVENT_SRC_DIGITAL = 2,
	AMDGPU_DPM_EVENT_SRC_ANALOG_OR_EXTERNAL = 3,
	AMDGPU_DPM_EVENT_SRC_DIGIAL_OR_EXTERNAL = 4
};

#define AMDGPU_MAX_VCE_LEVELS 6

enum amdgpu_vce_level {
	AMDGPU_VCE_LEVEL_AC_ALL = 0,     /* AC, All cases */
	AMDGPU_VCE_LEVEL_DC_EE = 1,      /* DC, entropy encoding */
	AMDGPU_VCE_LEVEL_DC_LL_LOW = 2,  /* DC, low latency queue, res <= 720 */
	AMDGPU_VCE_LEVEL_DC_LL_HIGH = 3, /* DC, low latency queue, 1080 >= res > 720 */
	AMDGPU_VCE_LEVEL_DC_GP_LOW = 4,  /* DC, general purpose queue, res <= 720 */
	AMDGPU_VCE_LEVEL_DC_GP_HIGH = 5, /* DC, general purpose queue, 1080 >= res > 720 */
};

struct amdgpu_ps {
	u32 caps; /* vbios flags */
	u32 class; /* vbios flags */
	u32 class2; /* vbios flags */
	/* UVD clocks */
	u32 vclk;
	u32 dclk;
	/* VCE clocks */
	u32 evclk;
	u32 ecclk;
	bool vce_active;
	enum amdgpu_vce_level vce_level;
	/* asic priv */
	void *ps_priv;
};

struct amdgpu_dpm_thermal {
	/* thermal interrupt work */
	struct work_struct work;
	/* low temperature threshold */
	int                min_temp;
	/* high temperature threshold */
	int                max_temp;
	/* was last interrupt low to high or high to low */
	bool               high_to_low;
	/* interrupt source */
	struct amdgpu_irq_src	irq;
};

enum amdgpu_clk_action
{
	AMDGPU_SCLK_UP = 1,
	AMDGPU_SCLK_DOWN
};

struct amdgpu_blacklist_clocks
{
	u32 sclk;
	u32 mclk;
	enum amdgpu_clk_action action;
};

struct amdgpu_clock_and_voltage_limits {
	u32 sclk;
	u32 mclk;
	u16 vddc;
	u16 vddci;
};

struct amdgpu_clock_array {
	u32 count;
	u32 *values;
};

struct amdgpu_clock_voltage_dependency_entry {
	u32 clk;
	u16 v;
};

struct amdgpu_clock_voltage_dependency_table {
	u32 count;
	struct amdgpu_clock_voltage_dependency_entry *entries;
};

union amdgpu_cac_leakage_entry {
	struct {
		u16 vddc;
		u32 leakage;
	};
	struct {
		u16 vddc1;
		u16 vddc2;
		u16 vddc3;
	};
};

struct amdgpu_cac_leakage_table {
	u32 count;
	union amdgpu_cac_leakage_entry *entries;
};

struct amdgpu_phase_shedding_limits_entry {
	u16 voltage;
	u32 sclk;
	u32 mclk;
};

struct amdgpu_phase_shedding_limits_table {
	u32 count;
	struct amdgpu_phase_shedding_limits_entry *entries;
};

struct amdgpu_uvd_clock_voltage_dependency_entry {
	u32 vclk;
	u32 dclk;
	u16 v;
};

struct amdgpu_uvd_clock_voltage_dependency_table {
	u8 count;
	struct amdgpu_uvd_clock_voltage_dependency_entry *entries;
};

struct amdgpu_vce_clock_voltage_dependency_entry {
	u32 ecclk;
	u32 evclk;
	u16 v;
};

struct amdgpu_vce_clock_voltage_dependency_table {
	u8 count;
	struct amdgpu_vce_clock_voltage_dependency_entry *entries;
};

struct amdgpu_ppm_table {
	u8 ppm_design;
	u16 cpu_core_number;
	u32 platform_tdp;
	u32 small_ac_platform_tdp;
	u32 platform_tdc;
	u32 small_ac_platform_tdc;
	u32 apu_tdp;
	u32 dgpu_tdp;
	u32 dgpu_ulv_power;
	u32 tj_max;
};

struct amdgpu_cac_tdp_table {
	u16 tdp;
	u16 configurable_tdp;
	u16 tdc;
	u16 battery_power_limit;
	u16 small_power_limit;
	u16 low_cac_leakage;
	u16 high_cac_leakage;
	u16 maximum_power_delivery_limit;
};

struct amdgpu_dpm_dynamic_state {
	struct amdgpu_clock_voltage_dependency_table vddc_dependency_on_sclk;
	struct amdgpu_clock_voltage_dependency_table vddci_dependency_on_mclk;
	struct amdgpu_clock_voltage_dependency_table vddc_dependency_on_mclk;
	struct amdgpu_clock_voltage_dependency_table mvdd_dependency_on_mclk;
	struct amdgpu_clock_voltage_dependency_table vddc_dependency_on_dispclk;
	struct amdgpu_uvd_clock_voltage_dependency_table uvd_clock_voltage_dependency_table;
	struct amdgpu_vce_clock_voltage_dependency_table vce_clock_voltage_dependency_table;
	struct amdgpu_clock_voltage_dependency_table samu_clock_voltage_dependency_table;
	struct amdgpu_clock_voltage_dependency_table acp_clock_voltage_dependency_table;
	struct amdgpu_clock_voltage_dependency_table vddgfx_dependency_on_sclk;
	struct amdgpu_clock_array valid_sclk_values;
	struct amdgpu_clock_array valid_mclk_values;
	struct amdgpu_clock_and_voltage_limits max_clock_voltage_on_dc;
	struct amdgpu_clock_and_voltage_limits max_clock_voltage_on_ac;
	u32 mclk_sclk_ratio;
	u32 sclk_mclk_delta;
	u16 vddc_vddci_delta;
	u16 min_vddc_for_pcie_gen2;
	struct amdgpu_cac_leakage_table cac_leakage_table;
	struct amdgpu_phase_shedding_limits_table phase_shedding_limits_table;
	struct amdgpu_ppm_table *ppm_table;
	struct amdgpu_cac_tdp_table *cac_tdp_table;
};

struct amdgpu_dpm_fan {
	u16 t_min;
	u16 t_med;
	u16 t_high;
	u16 pwm_min;
	u16 pwm_med;
	u16 pwm_high;
	u8 t_hyst;
	u32 cycle_delay;
	u16 t_max;
	u8 control_mode;
	u16 default_max_fan_pwm;
	u16 default_fan_output_sensitivity;
	u16 fan_output_sensitivity;
	bool ucode_fan_control;
};

enum amdgpu_pcie_gen {
	AMDGPU_PCIE_GEN1 = 0,
	AMDGPU_PCIE_GEN2 = 1,
	AMDGPU_PCIE_GEN3 = 2,
	AMDGPU_PCIE_GEN_INVALID = 0xffff
};

enum amdgpu_dpm_forced_level {
	AMDGPU_DPM_FORCED_LEVEL_AUTO = 0,
	AMDGPU_DPM_FORCED_LEVEL_LOW = 1,
	AMDGPU_DPM_FORCED_LEVEL_HIGH = 2,
};

struct amdgpu_vce_state {
	/* vce clocks */
	u32 evclk;
	u32 ecclk;
	/* gpu clocks */
	u32 sclk;
	u32 mclk;
	u8 clk_idx;
	u8 pstate;
};

struct amdgpu_dpm_funcs {
	int (*get_temperature)(struct amdgpu_device *adev);
	int (*pre_set_power_state)(struct amdgpu_device *adev);
	int (*set_power_state)(struct amdgpu_device *adev);
	void (*post_set_power_state)(struct amdgpu_device *adev);
	void (*display_configuration_changed)(struct amdgpu_device *adev);
	u32 (*get_sclk)(struct amdgpu_device *adev, bool low);
	u32 (*get_mclk)(struct amdgpu_device *adev, bool low);
	void (*print_power_state)(struct amdgpu_device *adev, struct amdgpu_ps *ps);
	void (*debugfs_print_current_performance_level)(struct amdgpu_device *adev, struct seq_file *m);
	int (*force_performance_level)(struct amdgpu_device *adev, enum amdgpu_dpm_forced_level level);
	bool (*vblank_too_short)(struct amdgpu_device *adev);
	void (*powergate_uvd)(struct amdgpu_device *adev, bool gate);
	void (*powergate_vce)(struct amdgpu_device *adev, bool gate);
	void (*enable_bapm)(struct amdgpu_device *adev, bool enable);
	void (*set_fan_control_mode)(struct amdgpu_device *adev, u32 mode);
	u32 (*get_fan_control_mode)(struct amdgpu_device *adev);
	int (*set_fan_speed_percent)(struct amdgpu_device *adev, u32 speed);
	int (*get_fan_speed_percent)(struct amdgpu_device *adev, u32 *speed);
};

struct amdgpu_dpm {
	struct amdgpu_ps        *ps;
	/* number of valid power states */
	int                     num_ps;
	/* current power state that is active */
	struct amdgpu_ps        *current_ps;
	/* requested power state */
	struct amdgpu_ps        *requested_ps;
	/* boot up power state */
	struct amdgpu_ps        *boot_ps;
	/* default uvd power state */
	struct amdgpu_ps        *uvd_ps;
	/* vce requirements */
	struct amdgpu_vce_state vce_states[AMDGPU_MAX_VCE_LEVELS];
	enum amdgpu_vce_level vce_level;
	enum amdgpu_pm_state_type state;
	enum amdgpu_pm_state_type user_state;
	u32                     platform_caps;
	u32                     voltage_response_time;
	u32                     backbias_response_time;
	void                    *priv;
	u32			new_active_crtcs;
	int			new_active_crtc_count;
	u32			current_active_crtcs;
	int			current_active_crtc_count;
	struct amdgpu_dpm_dynamic_state dyn_state;
	struct amdgpu_dpm_fan fan;
	u32 tdp_limit;
	u32 near_tdp_limit;
	u32 near_tdp_limit_adjusted;
	u32 sq_ramping_threshold;
	u32 cac_leakage;
	u16 tdp_od_limit;
	u32 tdp_adjustment;
	u16 load_line_slope;
	bool power_control;
	bool ac_power;
	/* special states active */
	bool                    thermal_active;
	bool                    uvd_active;
	bool                    vce_active;
	/* thermal handling */
	struct amdgpu_dpm_thermal thermal;
	/* forced levels */
	enum amdgpu_dpm_forced_level forced_level;
};

struct amdgpu_pm {
	struct mutex		mutex;
	u32                     current_sclk;
	u32                     current_mclk;
	u32                     default_sclk;
	u32                     default_mclk;
	struct amdgpu_i2c_chan *i2c_bus;
	/* internal thermal controller on rv6xx+ */
	enum amdgpu_int_thermal_type int_thermal_type;
	struct device	        *int_hwmon_dev;
	/* fan control parameters */
	bool                    no_fan;
	u8                      fan_pulses_per_revolution;
	u8                      fan_min_rpm;
	u8                      fan_max_rpm;
	/* dpm */
	bool                    dpm_enabled;
	bool                    sysfs_initialized;
	struct amdgpu_dpm       dpm;
	const struct firmware	*fw;	/* SMC firmware */
	uint32_t                fw_version;
	const struct amdgpu_dpm_funcs *funcs;
};

/*
 * UVD
 */
#define AMDGPU_MAX_UVD_HANDLES	10
#define AMDGPU_UVD_STACK_SIZE	(1024*1024)
#define AMDGPU_UVD_HEAP_SIZE	(1024*1024)
#define AMDGPU_UVD_FIRMWARE_OFFSET 256

struct amdgpu_uvd {
	struct amdgpu_bo	*vcpu_bo;
	void			*cpu_addr;
	uint64_t		gpu_addr;
	unsigned		fw_version;
	atomic_t		handles[AMDGPU_MAX_UVD_HANDLES];
	struct drm_file		*filp[AMDGPU_MAX_UVD_HANDLES];
	struct delayed_work	idle_work;
	const struct firmware	*fw;	/* UVD firmware */
	struct amdgpu_ring	ring;
	struct amdgpu_irq_src	irq;
	bool			address_64_bit;
};

/*
 * VCE
 */
#define AMDGPU_MAX_VCE_HANDLES	16
#define AMDGPU_VCE_FIRMWARE_OFFSET 256

#define AMDGPU_VCE_HARVEST_VCE0 (1 << 0)
#define AMDGPU_VCE_HARVEST_VCE1 (1 << 1)

struct amdgpu_vce {
	struct amdgpu_bo	*vcpu_bo;
	uint64_t		gpu_addr;
	unsigned		fw_version;
	unsigned		fb_version;
	atomic_t		handles[AMDGPU_MAX_VCE_HANDLES];
	struct drm_file		*filp[AMDGPU_MAX_VCE_HANDLES];
	uint32_t		img_size[AMDGPU_MAX_VCE_HANDLES];
	struct delayed_work	idle_work;
	const struct firmware	*fw;	/* VCE firmware */
	struct amdgpu_ring	ring[AMDGPU_MAX_VCE_RINGS];
	struct amdgpu_irq_src	irq;
	unsigned		harvest_config;
};

/*
 * SDMA
 */
struct amdgpu_sdma_instance {
	/* SDMA firmware */
	const struct firmware	*fw;
	uint32_t		fw_version;
	uint32_t		feature_version;

	struct amdgpu_ring	ring;
	bool			burst_nop;
};

struct amdgpu_sdma {
	struct amdgpu_sdma_instance instance[AMDGPU_MAX_SDMA_INSTANCES];
	struct amdgpu_irq_src	trap_irq;
	struct amdgpu_irq_src	illegal_inst_irq;
	int 			num_instances;
};

/*
 * Firmware
 */
struct amdgpu_firmware {
	struct amdgpu_firmware_info ucode[AMDGPU_UCODE_ID_MAXIMUM];
	bool smu_load;
	struct amdgpu_bo *fw_buf;
	unsigned int fw_size;
};
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

/*
 * Benchmarking
 */
void amdgpu_benchmark(struct amdgpu_device *adev, int test_number);


/*
 * Testing
 */
void amdgpu_test_moves(struct amdgpu_device *adev);

/*
 * ASIC specific register table accessible by UMD
 */
struct amdgpu_allowed_register_entry {
	uint32_t reg_offset;
	bool grbm_indexed;
};

enum amd_reset_method {
	AMD_RESET_METHOD_LEGACY = 0,
	AMD_RESET_METHOD_MODE0,
	AMD_RESET_METHOD_MODE1,
	AMD_RESET_METHOD_MODE2,
	AMD_RESET_METHOD_BACO
};

/*
 * ASIC specific functions.
 */
struct amdgpu_asic_funcs {
	bool (*read_disabled_bios)(struct amdgpu_device *adev);
	bool (*read_bios_from_rom)(struct amdgpu_device *adev,
				   u8 *bios, u32 length_bytes);
	int (*read_register)(struct amdgpu_device *adev, u32 se_num,
			     u32 sh_num, u32 reg_offset, u32 *value);
	void (*set_vga_state)(struct amdgpu_device *adev, bool state);
	int (*reset)(struct amdgpu_device *adev);
	enum amd_reset_method (*reset_method)(struct amdgpu_device *adev);
	/* get the reference clock */
	u32 (*get_xclk)(struct amdgpu_device *adev);
	/* MM block clocks */
	int (*set_uvd_clocks)(struct amdgpu_device *adev, u32 vclk, u32 dclk);
	int (*set_vce_clocks)(struct amdgpu_device *adev, u32 evclk, u32 ecclk);
	/* static power management */
	int (*get_pcie_lanes)(struct amdgpu_device *adev);
	void (*set_pcie_lanes)(struct amdgpu_device *adev, int lanes);
	/* get config memsize register */
	u32 (*get_config_memsize)(struct amdgpu_device *adev);
	/* flush hdp write queue */
	void (*flush_hdp)(struct amdgpu_device *adev, struct amdgpu_ring *ring);
	/* invalidate hdp read cache */
	void (*invalidate_hdp)(struct amdgpu_device *adev,
			       struct amdgpu_ring *ring);
	/* check if the asic needs a full reset of if soft reset will work */
	bool (*need_full_reset)(struct amdgpu_device *adev);
	/* initialize doorbell layout for specific asic*/
	void (*init_doorbell_index)(struct amdgpu_device *adev);
	/* PCIe bandwidth usage */
	void (*get_pcie_usage)(struct amdgpu_device *adev, uint64_t *count0,
			       uint64_t *count1);
	/* do we need to reset the asic at init time (e.g., kexec) */
	bool (*need_reset_on_init)(struct amdgpu_device *adev);
	/* PCIe replay counter */
	uint64_t (*get_pcie_replay_count)(struct amdgpu_device *adev);
};

/*
 * IOCTL.
 */
int amdgpu_bo_list_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp);

int amdgpu_cs_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int amdgpu_cs_fence_to_handle_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *filp);
int amdgpu_cs_wait_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int amdgpu_cs_wait_fences_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp);

/* VRAM scratch page for HDP bug, default vram page */
struct amdgpu_vram_scratch {
	struct amdgpu_bo		*robj;
	volatile uint32_t		*ptr;
	u64				gpu_addr;
};

/*
 * ACPI
 */
struct amdgpu_atcs_functions {
	bool get_ext_state;
	bool pcie_perf_req;
	bool pcie_dev_rdy;
	bool pcie_bus_width;
};

struct amdgpu_atcs {
	struct amdgpu_atcs_functions functions;
};

/*
 * Firmware VRAM reservation
 */
struct amdgpu_fw_vram_usage {
	u64 start_offset;
	u64 size;
	struct amdgpu_bo *reserved_bo;
	void *va;
};

/*
 * CGS
 */
struct cgs_device *amdgpu_cgs_create_device(struct amdgpu_device *adev);
void amdgpu_cgs_destroy_device(struct cgs_device *cgs_device);

/*
 * Core structure, functions and helpers.
 */
typedef uint32_t (*amdgpu_rreg_t)(struct amdgpu_device*, uint32_t);
typedef void (*amdgpu_wreg_t)(struct amdgpu_device*, uint32_t, uint32_t);

typedef uint64_t (*amdgpu_rreg64_t)(struct amdgpu_device*, uint32_t);
typedef void (*amdgpu_wreg64_t)(struct amdgpu_device*, uint32_t, uint64_t);

typedef uint32_t (*amdgpu_block_rreg_t)(struct amdgpu_device*, uint32_t, uint32_t);
typedef void (*amdgpu_block_wreg_t)(struct amdgpu_device*, uint32_t, uint32_t, uint32_t);


/*
 * amdgpu nbio functions
 *
 */
struct nbio_hdp_flush_reg {
	u32 ref_and_mask_cp0;
	u32 ref_and_mask_cp1;
	u32 ref_and_mask_cp2;
	u32 ref_and_mask_cp3;
	u32 ref_and_mask_cp4;
	u32 ref_and_mask_cp5;
	u32 ref_and_mask_cp6;
	u32 ref_and_mask_cp7;
	u32 ref_and_mask_cp8;
	u32 ref_and_mask_cp9;
	u32 ref_and_mask_sdma0;
	u32 ref_and_mask_sdma1;
	u32 ref_and_mask_sdma2;
	u32 ref_and_mask_sdma3;
	u32 ref_and_mask_sdma4;
	u32 ref_and_mask_sdma5;
	u32 ref_and_mask_sdma6;
	u32 ref_and_mask_sdma7;
};

struct amdgpu_mmio_remap {
	u32 reg_offset;
	resource_size_t bus_addr;
};

struct amdgpu_nbio_funcs {
	const struct nbio_hdp_flush_reg *hdp_flush_reg;
	u32 (*get_hdp_flush_req_offset)(struct amdgpu_device *adev);
	u32 (*get_hdp_flush_done_offset)(struct amdgpu_device *adev);
	u32 (*get_pcie_index_offset)(struct amdgpu_device *adev);
	u32 (*get_pcie_data_offset)(struct amdgpu_device *adev);
	u32 (*get_rev_id)(struct amdgpu_device *adev);
	void (*mc_access_enable)(struct amdgpu_device *adev, bool enable);
	void (*hdp_flush)(struct amdgpu_device *adev, struct amdgpu_ring *ring);
	u32 (*get_memsize)(struct amdgpu_device *adev);
	void (*sdma_doorbell_range)(struct amdgpu_device *adev, int instance,
			bool use_doorbell, int doorbell_index, int doorbell_size);
	void (*vcn_doorbell_range)(struct amdgpu_device *adev, bool use_doorbell,
				   int doorbell_index, int instance);
	void (*enable_doorbell_aperture)(struct amdgpu_device *adev,
					 bool enable);
	void (*enable_doorbell_selfring_aperture)(struct amdgpu_device *adev,
						  bool enable);
	void (*ih_doorbell_range)(struct amdgpu_device *adev,
				  bool use_doorbell, int doorbell_index);
	void (*update_medium_grain_clock_gating)(struct amdgpu_device *adev,
						 bool enable);
	void (*update_medium_grain_light_sleep)(struct amdgpu_device *adev,
						bool enable);
	void (*get_clockgating_state)(struct amdgpu_device *adev,
				      u32 *flags);
	void (*ih_control)(struct amdgpu_device *adev);
	void (*init_registers)(struct amdgpu_device *adev);
	void (*detect_hw_virt)(struct amdgpu_device *adev);
	void (*remap_hdp_registers)(struct amdgpu_device *adev);
};

struct amdgpu_df_funcs {
	void (*sw_init)(struct amdgpu_device *adev);
	void (*enable_broadcast_mode)(struct amdgpu_device *adev,
				      bool enable);
	u32 (*get_fb_channel_number)(struct amdgpu_device *adev);
	u32 (*get_hbm_channel_number)(struct amdgpu_device *adev);
	void (*update_medium_grain_clock_gating)(struct amdgpu_device *adev,
						 bool enable);
	void (*get_clockgating_state)(struct amdgpu_device *adev,
				      u32 *flags);
	void (*enable_ecc_force_par_wr_rmw)(struct amdgpu_device *adev,
					    bool enable);
	int (*pmc_start)(struct amdgpu_device *adev, uint64_t config,
					 int is_enable);
	int (*pmc_stop)(struct amdgpu_device *adev, uint64_t config,
					 int is_disable);
	void (*pmc_get_count)(struct amdgpu_device *adev, uint64_t config,
					 uint64_t *count);
	uint64_t (*get_fica)(struct amdgpu_device *adev, uint32_t ficaa_val);
	void (*set_fica)(struct amdgpu_device *adev, uint32_t ficaa_val,
			 uint32_t ficadl_val, uint32_t ficadh_val);
};
/* Define the HW IP blocks will be used in driver , add more if necessary */
enum amd_hw_ip_block_type {
	GC_HWIP = 1,
	HDP_HWIP,
	SDMA0_HWIP,
	SDMA1_HWIP,
	SDMA2_HWIP,
	SDMA3_HWIP,
	SDMA4_HWIP,
	SDMA5_HWIP,
	SDMA6_HWIP,
	SDMA7_HWIP,
	MMHUB_HWIP,
	ATHUB_HWIP,
	NBIO_HWIP,
	MP0_HWIP,
	MP1_HWIP,
	UVD_HWIP,
	VCN_HWIP = UVD_HWIP,
	VCE_HWIP,
	DF_HWIP,
	DCE_HWIP,
	OSSSYS_HWIP,
	SMUIO_HWIP,
	PWR_HWIP,
	NBIF_HWIP,
	THM_HWIP,
	CLK_HWIP,
	UMC_HWIP,
	RSMU_HWIP,
	MAX_HWIP
};

#define HWIP_MAX_INSTANCE	8

struct amd_powerplay {
	void *pp_handle;
	const struct amd_pm_funcs *pp_funcs;
};

#define AMDGPU_RESET_MAGIC_NUM 64
#define AMDGPU_MAX_DF_PERFMONS 4
struct amdgpu_device {
	struct device			*dev;
	struct drm_device		*ddev;
	struct pci_dev			*pdev;

#ifdef CONFIG_DRM_AMD_ACP
	struct amdgpu_acp		acp;
#endif

	/* ASIC */
	enum amd_asic_type		asic_type;
	uint32_t			family;
	uint32_t			rev_id;
	uint32_t			external_rev_id;
	unsigned long			flags;
	int				usec_timeout;
	const struct amdgpu_asic_funcs	*asic_funcs;
	bool				shutdown;
	bool				need_swiotlb;
	bool				accel_working;
	struct notifier_block		acpi_nb;
	struct amdgpu_i2c_chan		*i2c_bus[AMDGPU_MAX_I2C_BUS];
	struct amdgpu_debugfs		debugfs[AMDGPU_DEBUGFS_MAX_COMPONENTS];
	unsigned			debugfs_count;
#if defined(CONFIG_DEBUG_FS)
	struct dentry                   *debugfs_preempt;
	struct dentry			*debugfs_regs[AMDGPU_DEBUGFS_MAX_COMPONENTS];
#endif
	struct amdgpu_atif		*atif;
	struct amdgpu_atcs		atcs;
	struct mutex			srbm_mutex;
	/* GRBM index mutex. Protects concurrent access to GRBM index */
	struct mutex                    grbm_idx_mutex;
	struct dev_pm_domain		vga_pm_domain;
	bool				have_disp_power_ref;
	bool                            have_atomics_support;

	/* BIOS */
	bool				is_atom_fw;
	uint8_t				*bios;
	uint32_t			bios_size;
	struct amdgpu_bo		*stolen_vga_memory;
	struct amdgpu_bo		*discovery_memory;
	uint32_t			bios_scratch_reg_offset;
	uint32_t			bios_scratch[AMDGPU_BIOS_NUM_SCRATCH];

	/* Register/doorbell mmio */
	resource_size_t			rmmio_base;
	resource_size_t			rmmio_size;
	void __iomem			*rmmio;
	/* protects concurrent MM_INDEX/DATA based register access */
	spinlock_t mmio_idx_lock;
	struct amdgpu_mmio_remap        rmmio_remap;
	/* protects concurrent SMC based register access */
	spinlock_t smc_idx_lock;
	amdgpu_rreg_t			smc_rreg;
	amdgpu_wreg_t			smc_wreg;
	/* protects concurrent PCIE register access */
	spinlock_t pcie_idx_lock;
	amdgpu_rreg_t			pcie_rreg;
	amdgpu_wreg_t			pcie_wreg;
	amdgpu_rreg_t			pciep_rreg;
	amdgpu_wreg_t			pciep_wreg;
	amdgpu_rreg64_t			pcie_rreg64;
	amdgpu_wreg64_t			pcie_wreg64;
	/* protects concurrent UVD register access */
	spinlock_t uvd_ctx_idx_lock;
	amdgpu_rreg_t			uvd_ctx_rreg;
	amdgpu_wreg_t			uvd_ctx_wreg;
	/* protects concurrent DIDT register access */
	spinlock_t didt_idx_lock;
	amdgpu_rreg_t			didt_rreg;
	amdgpu_wreg_t			didt_wreg;
	/* protects concurrent gc_cac register access */
	spinlock_t gc_cac_idx_lock;
	amdgpu_rreg_t			gc_cac_rreg;
	amdgpu_wreg_t			gc_cac_wreg;
	/* protects concurrent se_cac register access */
	spinlock_t se_cac_idx_lock;
	amdgpu_rreg_t			se_cac_rreg;
	amdgpu_wreg_t			se_cac_wreg;
	/* protects concurrent ENDPOINT (audio) register access */
	spinlock_t audio_endpt_idx_lock;
	amdgpu_block_rreg_t		audio_endpt_rreg;
	amdgpu_block_wreg_t		audio_endpt_wreg;
	void __iomem                    *rio_mem;
	resource_size_t			rio_mem_size;
	struct amdgpu_doorbell		doorbell;

	/* clock/pll info */
	struct amdgpu_clock            clock;

	/* MC */
	struct amdgpu_gmc		gmc;
	struct amdgpu_gart		gart;
	dma_addr_t			dummy_page_addr;
	struct amdgpu_vm_manager	vm_manager;
	struct amdgpu_vmhub             vmhub[AMDGPU_MAX_VMHUBS];
	unsigned			num_vmhubs;

	/* memory management */
	struct amdgpu_mman		mman;
	struct amdgpu_vram_scratch	vram_scratch;
	struct amdgpu_wb		wb;
	atomic64_t			num_bytes_moved;
	atomic64_t			num_evictions;
	atomic64_t			num_vram_cpu_page_faults;
	atomic_t			gpu_reset_counter;
	atomic_t			vram_lost_counter;

	/* data for buffer migration throttling */
	struct {
		spinlock_t		lock;
		s64			last_update_us;
		s64			accum_us; /* accumulated microseconds */
		s64			accum_us_vis; /* for visible VRAM */
		u32			log2_max_MBps;
	} mm_stats;

	/* display */
	bool				enable_virtual_display;
	struct amdgpu_mode_info		mode_info;
	/* For pre-DCE11. DCE11 and later are in "struct amdgpu_device->dm" */
	struct work_struct		hotplug_work;
	struct amdgpu_irq_src		crtc_irq;
	struct amdgpu_irq_src		vupdate_irq;
	struct amdgpu_irq_src		pageflip_irq;
	struct amdgpu_irq_src		hpd_irq;

	/* rings */
	u64				fence_context;
	unsigned			num_rings;
	struct amdgpu_ring		*rings[AMDGPU_MAX_RINGS];
	bool				ib_pool_ready;
	struct amdgpu_sa_manager	ring_tmp_bo;

	/* interrupts */
	struct amdgpu_irq		irq;

	/* powerplay */
	struct amd_powerplay		powerplay;
	bool				pp_force_state_enabled;

	/* smu */
	struct smu_context		smu;

	/* dpm */
	struct amdgpu_pm		pm;
	u32				cg_flags;
	u32				pg_flags;

	/* gfx */
	struct amdgpu_gfx		gfx;

	/* sdma */
	struct amdgpu_sdma		sdma;

	/* uvd */
	struct amdgpu_uvd		uvd;

	/* vce */
	struct amdgpu_vce		vce;

	/* vcn */
	struct amdgpu_vcn		vcn;

	/* firmwares */
	struct amdgpu_firmware		firmware;

	/* PSP */
	struct psp_context		psp;

	/* GDS */
	struct amdgpu_gds		gds;

	/* KFD */
	struct amdgpu_kfd_dev		kfd;

	/* UMC */
	struct amdgpu_umc		umc;

	/* display related functionality */
	struct amdgpu_display_manager dm;

	/* discovery */
	uint8_t				*discovery;

	/* mes */
	bool                            enable_mes;
	struct amdgpu_mes               mes;

	struct amdgpu_ip_block          ip_blocks[AMDGPU_MAX_IP_NUM];
	int				num_ip_blocks;
	struct mutex	mn_lock;
	DECLARE_HASHTABLE(mn_hash, 7);

	/* tracking pinned memory */
	atomic64_t vram_pin_size;
	atomic64_t visible_pin_size;
	atomic64_t gart_pin_size;

	/* soc15 register offset based on ip, instance and  segment */
	uint32_t 		*reg_offset[MAX_HWIP][HWIP_MAX_INSTANCE];

	const struct amdgpu_nbio_funcs	*nbio_funcs;
	const struct amdgpu_df_funcs	*df_funcs;
	const struct amdgpu_mmhub_funcs	*mmhub_funcs;

	/* delayed work_func for deferring clockgating during resume */
	struct delayed_work     delayed_init_work;

	struct amdgpu_virt	virt;
	/* firmware VRAM reservation */
	struct amdgpu_fw_vram_usage fw_vram_usage;

	/* link all shadow bo */
	struct list_head                shadow_list;
	struct mutex                    shadow_list_lock;
	/* keep an lru list of rings by HW IP */
	struct list_head		ring_lru_list;
	spinlock_t			ring_lru_list_lock;

	/* record hw reset is performed */
	bool has_hw_reset;
	u8				reset_magic[AMDGPU_RESET_MAGIC_NUM];

	/* s3/s4 mask */
	bool                            in_suspend;

	/* record last mm index being written through WREG32*/
	unsigned long last_mm_index;
	bool                            in_gpu_reset;
	enum pp_mp1_state               mp1_state;
	struct mutex  lock_reset;
	struct amdgpu_doorbell_index doorbell_index;

	int asic_reset_res;
	struct work_struct		xgmi_reset_work;

	bool                            in_baco_reset;

	long				gfx_timeout;
	long				sdma_timeout;
	long				video_timeout;
	long				compute_timeout;

	uint64_t			unique_id;
	uint64_t	df_perfmon_config_assign_mask[AMDGPU_MAX_DF_PERFMONS];
};

static inline struct amdgpu_device *amdgpu_ttm_adev(struct ttm_bo_device *bdev)
{
	return container_of(bdev, struct amdgpu_device, mman.bdev);
}

int amdgpu_device_init(struct amdgpu_device *adev,
		       struct drm_device *ddev,
		       struct pci_dev *pdev,
		       uint32_t flags);
void amdgpu_device_fini(struct amdgpu_device *adev);
int amdgpu_gpu_wait_for_idle(struct amdgpu_device *adev);

uint32_t amdgpu_mm_rreg(struct amdgpu_device *adev, uint32_t reg,
			uint32_t acc_flags);
void amdgpu_mm_wreg(struct amdgpu_device *adev, uint32_t reg, uint32_t v,
		    uint32_t acc_flags);
void amdgpu_mm_wreg8(struct amdgpu_device *adev, uint32_t offset, uint8_t value);
uint8_t amdgpu_mm_rreg8(struct amdgpu_device *adev, uint32_t offset);

u32 amdgpu_io_rreg(struct amdgpu_device *adev, u32 reg);
void amdgpu_io_wreg(struct amdgpu_device *adev, u32 reg, u32 v);

bool amdgpu_device_asic_has_dc_support(enum amd_asic_type asic_type);
bool amdgpu_device_has_dc_support(struct amdgpu_device *adev);

int emu_soc_asic_init(struct amdgpu_device *adev);

/*
 * Registers read & write functions.
 */

#define AMDGPU_REGS_IDX       (1<<0)
#define AMDGPU_REGS_NO_KIQ    (1<<1)

#define RREG32_NO_KIQ(reg) amdgpu_mm_rreg(adev, (reg), AMDGPU_REGS_NO_KIQ)
#define WREG32_NO_KIQ(reg, v) amdgpu_mm_wreg(adev, (reg), (v), AMDGPU_REGS_NO_KIQ)

#define RREG8(reg) amdgpu_mm_rreg8(adev, (reg))
#define WREG8(reg, v) amdgpu_mm_wreg8(adev, (reg), (v))

#define RREG32(reg) amdgpu_mm_rreg(adev, (reg), 0)
#define RREG32_IDX(reg) amdgpu_mm_rreg(adev, (reg), AMDGPU_REGS_IDX)
#define DREG32(reg) printk(KERN_INFO "REGISTER: " #reg " : 0x%08X\n", amdgpu_mm_rreg(adev, (reg), 0))
#define WREG32(reg, v) amdgpu_mm_wreg(adev, (reg), (v), 0)
#define WREG32_IDX(reg, v) amdgpu_mm_wreg(adev, (reg), (v), AMDGPU_REGS_IDX)
#define REG_SET(FIELD, v) (((v) << FIELD##_SHIFT) & FIELD##_MASK)
#define REG_GET(FIELD, v) (((v) << FIELD##_SHIFT) & FIELD##_MASK)
#define RREG32_PCIE(reg) adev->pcie_rreg(adev, (reg))
#define WREG32_PCIE(reg, v) adev->pcie_wreg(adev, (reg), (v))
#define RREG32_PCIE_PORT(reg) adev->pciep_rreg(adev, (reg))
#define WREG32_PCIE_PORT(reg, v) adev->pciep_wreg(adev, (reg), (v))
#define RREG64_PCIE(reg) adev->pcie_rreg64(adev, (reg))
#define WREG64_PCIE(reg, v) adev->pcie_wreg64(adev, (reg), (v))
#define RREG32_SMC(reg) adev->smc_rreg(adev, (reg))
#define WREG32_SMC(reg, v) adev->smc_wreg(adev, (reg), (v))
#define RREG32_UVD_CTX(reg) adev->uvd_ctx_rreg(adev, (reg))
#define WREG32_UVD_CTX(reg, v) adev->uvd_ctx_wreg(adev, (reg), (v))
#define RREG32_DIDT(reg) adev->didt_rreg(adev, (reg))
#define WREG32_DIDT(reg, v) adev->didt_wreg(adev, (reg), (v))
#define RREG32_GC_CAC(reg) adev->gc_cac_rreg(adev, (reg))
#define WREG32_GC_CAC(reg, v) adev->gc_cac_wreg(adev, (reg), (v))
#define RREG32_SE_CAC(reg) adev->se_cac_rreg(adev, (reg))
#define WREG32_SE_CAC(reg, v) adev->se_cac_wreg(adev, (reg), (v))
#define RREG32_AUDIO_ENDPT(block, reg) adev->audio_endpt_rreg(adev, (block), (reg))
#define WREG32_AUDIO_ENDPT(block, reg, v) adev->audio_endpt_wreg(adev, (block), (reg), (v))
#define WREG32_P(reg, val, mask)				\
	do {							\
		uint32_t tmp_ = RREG32(reg);			\
		tmp_ &= (mask);					\
		tmp_ |= ((val) & ~(mask));			\
		WREG32(reg, tmp_);				\
	} while (0)
#define WREG32_AND(reg, and) WREG32_P(reg, 0, and)
#define WREG32_OR(reg, or) WREG32_P(reg, or, ~(or))
#define WREG32_PLL_P(reg, val, mask)				\
	do {							\
		uint32_t tmp_ = RREG32_PLL(reg);		\
		tmp_ &= (mask);					\
		tmp_ |= ((val) & ~(mask));			\
		WREG32_PLL(reg, tmp_);				\
	} while (0)
#define DREG32_SYS(sqf, adev, reg) seq_printf((sqf), #reg " : 0x%08X\n", amdgpu_mm_rreg((adev), (reg), false))
#define RREG32_IO(reg) amdgpu_io_rreg(adev, (reg))
#define WREG32_IO(reg, v) amdgpu_io_wreg(adev, (reg), (v))

#define REG_FIELD_SHIFT(reg, field) reg##__##field##__SHIFT
#define REG_FIELD_MASK(reg, field) reg##__##field##_MASK

#define REG_SET_FIELD(orig_val, reg, field, field_val)			\
	(((orig_val) & ~REG_FIELD_MASK(reg, field)) |			\
	 (REG_FIELD_MASK(reg, field) & ((field_val) << REG_FIELD_SHIFT(reg, field))))

#define REG_GET_FIELD(value, reg, field)				\
	(((value) & REG_FIELD_MASK(reg, field)) >> REG_FIELD_SHIFT(reg, field))

#define WREG32_FIELD(reg, field, val)	\
	WREG32(mm##reg, (RREG32(mm##reg) & ~REG_FIELD_MASK(reg, field)) | (val) << REG_FIELD_SHIFT(reg, field))

#define WREG32_FIELD_OFFSET(reg, offset, field, val)	\
	WREG32(mm##reg + offset, (RREG32(mm##reg + offset) & ~REG_FIELD_MASK(reg, field)) | (val) << REG_FIELD_SHIFT(reg, field))

/*
 * BIOS helpers.
 */
#define RBIOS8(i) (adev->bios[i])
#define RBIOS16(i) (RBIOS8(i) | (RBIOS8((i)+1) << 8))
#define RBIOS32(i) ((RBIOS16(i)) | (RBIOS16((i)+2) << 16))

/*
 * ASICs macro.
 */
#define amdgpu_asic_set_vga_state(adev, state) (adev)->asic_funcs->set_vga_state((adev), (state))
#define amdgpu_asic_reset(adev) (adev)->asic_funcs->reset((adev))
#define amdgpu_asic_reset_method(adev) (adev)->asic_funcs->reset_method((adev))
#define amdgpu_asic_get_xclk(adev) (adev)->asic_funcs->get_xclk((adev))
#define amdgpu_asic_set_uvd_clocks(adev, v, d) (adev)->asic_funcs->set_uvd_clocks((adev), (v), (d))
#define amdgpu_asic_set_vce_clocks(adev, ev, ec) (adev)->asic_funcs->set_vce_clocks((adev), (ev), (ec))
#define amdgpu_get_pcie_lanes(adev) (adev)->asic_funcs->get_pcie_lanes((adev))
#define amdgpu_set_pcie_lanes(adev, l) (adev)->asic_funcs->set_pcie_lanes((adev), (l))
#define amdgpu_asic_get_gpu_clock_counter(adev) (adev)->asic_funcs->get_gpu_clock_counter((adev))
#define amdgpu_asic_read_disabled_bios(adev) (adev)->asic_funcs->read_disabled_bios((adev))
#define amdgpu_asic_read_bios_from_rom(adev, b, l) (adev)->asic_funcs->read_bios_from_rom((adev), (b), (l))
#define amdgpu_asic_read_register(adev, se, sh, offset, v)((adev)->asic_funcs->read_register((adev), (se), (sh), (offset), (v)))
#define amdgpu_asic_get_config_memsize(adev) (adev)->asic_funcs->get_config_memsize((adev))
#define amdgpu_asic_flush_hdp(adev, r) (adev)->asic_funcs->flush_hdp((adev), (r))
#define amdgpu_asic_invalidate_hdp(adev, r) (adev)->asic_funcs->invalidate_hdp((adev), (r))
#define amdgpu_asic_need_full_reset(adev) (adev)->asic_funcs->need_full_reset((adev))
#define amdgpu_asic_init_doorbell_index(adev) (adev)->asic_funcs->init_doorbell_index((adev))
#define amdgpu_asic_get_pcie_usage(adev, cnt0, cnt1) ((adev)->asic_funcs->get_pcie_usage((adev), (cnt0), (cnt1)))
#define amdgpu_asic_need_reset_on_init(adev) (adev)->asic_funcs->need_reset_on_init((adev))
#define amdgpu_asic_get_pcie_replay_count(adev) ((adev)->asic_funcs->get_pcie_replay_count((adev)))
#define amdgpu_inc_vram_lost(adev) atomic_inc(&((adev)->vram_lost_counter));

/* Common functions */
bool amdgpu_device_should_recover_gpu(struct amdgpu_device *adev);
int amdgpu_device_gpu_recover(struct amdgpu_device *adev,
			      struct amdgpu_job* job);
void amdgpu_device_pci_config_reset(struct amdgpu_device *adev);
bool amdgpu_device_need_post(struct amdgpu_device *adev);

<<<<<<< HEAD
void amdgpu_cs_report_moved_bytes(struct amdgpu_device *adev, u64 num_bytes,
				  u64 num_vis_bytes);
int amdgpu_device_resize_fb_bar(struct amdgpu_device *adev);
void amdgpu_device_program_register_sequence(struct amdgpu_device *adev,
=======
int amdgpu_cs_parser_init(struct amdgpu_cs_parser *p, void *data);
int amdgpu_cs_get_ring(struct amdgpu_device *adev, u32 ip_type,
		       u32 ip_instance, u32 ring,
		       struct amdgpu_ring **out_ring);
void amdgpu_ttm_placement_from_domain(struct amdgpu_bo *rbo, u32 domain);
bool amdgpu_ttm_bo_is_amdgpu_bo(struct ttm_buffer_object *bo);
int amdgpu_ttm_tt_set_userptr(struct ttm_tt *ttm, uint64_t addr,
				     uint32_t flags);
bool amdgpu_ttm_tt_has_userptr(struct ttm_tt *ttm);
bool amdgpu_ttm_tt_affect_userptr(struct ttm_tt *ttm, unsigned long start,
				  unsigned long end);
bool amdgpu_ttm_tt_is_readonly(struct ttm_tt *ttm);
uint32_t amdgpu_ttm_tt_pte_flags(struct amdgpu_device *adev, struct ttm_tt *ttm,
				 struct ttm_mem_reg *mem);
void amdgpu_vram_location(struct amdgpu_device *adev, struct amdgpu_mc *mc, u64 base);
void amdgpu_gtt_location(struct amdgpu_device *adev, struct amdgpu_mc *mc);
void amdgpu_ttm_set_active_vram_size(struct amdgpu_device *adev, u64 size);
void amdgpu_program_register_sequence(struct amdgpu_device *adev,
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
					     const u32 *registers,
					     const u32 array_size);

bool amdgpu_device_is_px(struct drm_device *dev);
bool amdgpu_device_is_peer_accessible(struct amdgpu_device *adev,
				      struct amdgpu_device *peer_adev);

/* atpx handler */
#if defined(CONFIG_VGA_SWITCHEROO)
void amdgpu_register_atpx_handler(void);
void amdgpu_unregister_atpx_handler(void);
bool amdgpu_has_atpx_dgpu_power_cntl(void);
bool amdgpu_is_atpx_hybrid(void);
bool amdgpu_atpx_dgpu_req_power_for_displays(void);
bool amdgpu_has_atpx(void);
#else
static inline void amdgpu_register_atpx_handler(void) {}
static inline void amdgpu_unregister_atpx_handler(void) {}
static inline bool amdgpu_has_atpx_dgpu_power_cntl(void) { return false; }
static inline bool amdgpu_is_atpx_hybrid(void) { return false; }
static inline bool amdgpu_atpx_dgpu_req_power_for_displays(void) { return false; }
static inline bool amdgpu_has_atpx(void) { return false; }
#endif

#if defined(CONFIG_VGA_SWITCHEROO) && defined(CONFIG_ACPI)
void *amdgpu_atpx_get_dhandle(void);
#else
static inline void *amdgpu_atpx_get_dhandle(void) { return NULL; }
#endif

/*
 * KMS
 */
extern const struct drm_ioctl_desc amdgpu_ioctls_kms[];
extern const int amdgpu_max_kms_ioctl;

int amdgpu_driver_load_kms(struct drm_device *dev, unsigned long flags);
void amdgpu_driver_unload_kms(struct drm_device *dev);
void amdgpu_driver_lastclose_kms(struct drm_device *dev);
int amdgpu_driver_open_kms(struct drm_device *dev, struct drm_file *file_priv);
void amdgpu_driver_postclose_kms(struct drm_device *dev,
				 struct drm_file *file_priv);
int amdgpu_device_ip_suspend(struct amdgpu_device *adev);
int amdgpu_device_suspend(struct drm_device *dev, bool suspend, bool fbcon);
int amdgpu_device_resume(struct drm_device *dev, bool resume, bool fbcon);
u32 amdgpu_get_vblank_counter_kms(struct drm_device *dev, unsigned int pipe);
int amdgpu_enable_vblank_kms(struct drm_device *dev, unsigned int pipe);
void amdgpu_disable_vblank_kms(struct drm_device *dev, unsigned int pipe);
long amdgpu_kms_compat_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg);

/*
 * functions used by amdgpu_encoder.c
 */
struct amdgpu_afmt_acr {
	u32 clock;

	int n_32khz;
	int cts_32khz;

	int n_44_1khz;
	int cts_44_1khz;

	int n_48khz;
	int cts_48khz;

};

struct amdgpu_afmt_acr amdgpu_afmt_acr(uint32_t clock);

/* amdgpu_acpi.c */
#if defined(CONFIG_ACPI)
int amdgpu_acpi_init(struct amdgpu_device *adev);
void amdgpu_acpi_fini(struct amdgpu_device *adev);
bool amdgpu_acpi_is_pcie_performance_request_supported(struct amdgpu_device *adev);
int amdgpu_acpi_pcie_performance_request(struct amdgpu_device *adev,
						u8 perf_req, bool advertise);
int amdgpu_acpi_pcie_notify_device_ready(struct amdgpu_device *adev);

void amdgpu_acpi_get_backlight_caps(struct amdgpu_device *adev,
		struct amdgpu_dm_backlight_caps *caps);
#else
static inline int amdgpu_acpi_init(struct amdgpu_device *adev) { return 0; }
static inline void amdgpu_acpi_fini(struct amdgpu_device *adev) { }
#endif

int amdgpu_cs_find_mapping(struct amdgpu_cs_parser *parser,
			   uint64_t addr, struct amdgpu_bo **bo,
			   struct amdgpu_bo_va_mapping **mapping);

#if defined(CONFIG_DRM_AMD_DC)
int amdgpu_dm_display_resume(struct amdgpu_device *adev );
#else
static inline int amdgpu_dm_display_resume(struct amdgpu_device *adev) { return 0; }
#endif


void amdgpu_register_gpu_instance(struct amdgpu_device *adev);
void amdgpu_unregister_gpu_instance(struct amdgpu_device *adev);

#include "amdgpu_object.h"

/* used by df_v3_6.c and amdgpu_pmu.c */
#define AMDGPU_PMU_ATTR(_name, _object)					\
static ssize_t								\
_name##_show(struct device *dev,					\
			       struct device_attribute *attr,		\
			       char *page)				\
{									\
	BUILD_BUG_ON(sizeof(_object) >= PAGE_SIZE - 1);			\
	return sprintf(page, _object "\n");				\
}									\
									\
static struct device_attribute pmu_attr_##_name = __ATTR_RO(_name)

#endif

