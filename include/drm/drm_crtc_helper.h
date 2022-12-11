/*
 * Copyright © 2006 Keith Packard
 * Copyright © 2007-2008 Dave Airlie
 * Copyright © 2007-2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
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
 */

/*
 * The DRM mode setting helper functions are common code for drivers to use if
 * they wish.  Drivers are not forced to use this code in their
 * implementations but it would be useful if they code they do use at least
 * provides a consistent interface and operation to userspace
 */

#ifndef __DRM_CRTC_HELPER_H__
#define __DRM_CRTC_HELPER_H__

#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/idr.h>

#include <linux/fb.h>

#include <drm/drm_crtc.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_modeset_helper.h>

void drm_helper_disable_unused_functions(struct drm_device *dev);
int drm_crtc_helper_set_config(struct drm_mode_set *set,
			       struct drm_modeset_acquire_ctx *ctx);
bool drm_crtc_helper_set_mode(struct drm_crtc *crtc,
			      struct drm_display_mode *mode,
			      int x, int y,
			      struct drm_framebuffer *old_fb);
bool drm_helper_crtc_in_use(struct drm_crtc *crtc);
bool drm_helper_encoder_in_use(struct drm_encoder *encoder);

int drm_helper_connector_dpms(struct drm_connector *connector, int mode);

<<<<<<< HEAD
void drm_helper_resume_force_mode(struct drm_device *dev);
int drm_helper_force_disable_all(struct drm_device *dev);
=======
	/* Provider can fixup or change mode timings before modeset occurs */
	bool (*mode_fixup)(struct drm_crtc *crtc,
			   const struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	/* Actually set the mode */
	int (*mode_set)(struct drm_crtc *crtc, struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode, int x, int y,
			struct drm_framebuffer *old_fb);
	/* Actually set the mode for atomic helpers, optional */
	void (*mode_set_nofb)(struct drm_crtc *crtc);

	/* Move the crtc on the current fb to the given position *optional* */
	int (*mode_set_base)(struct drm_crtc *crtc, int x, int y,
			     struct drm_framebuffer *old_fb);
	int (*mode_set_base_atomic)(struct drm_crtc *crtc,
				    struct drm_framebuffer *fb, int x, int y,
				    enum mode_set_atomic);

	/* reload the current crtc LUT */
	void (*load_lut)(struct drm_crtc *crtc);

	void (*disable)(struct drm_crtc *crtc);
	void (*enable)(struct drm_crtc *crtc);

	/* atomic helpers */
	int (*atomic_check)(struct drm_crtc *crtc,
			    struct drm_crtc_state *state);
	void (*atomic_begin)(struct drm_crtc *crtc,
			     struct drm_crtc_state *old_crtc_state);
	void (*atomic_flush)(struct drm_crtc *crtc,
			     struct drm_crtc_state *old_crtc_state);
};

/**
 * struct drm_encoder_helper_funcs - helper operations for encoders
 * @dpms: set power state
 * @save: save connector state
 * @restore: restore connector state
 * @mode_fixup: try to fixup proposed mode for this connector
 * @prepare: part of the disable sequence, called before the CRTC modeset
 * @commit: called after the CRTC modeset
 * @mode_set: set this mode, optional for atomic helpers
 * @get_crtc: return CRTC that the encoder is currently attached to
 * @detect: connection status detection
 * @disable: disable encoder when not in use (overrides DPMS off)
 * @enable: enable encoder
 * @atomic_check: check for validity of an atomic update
 *
 * The helper operations are called by the mid-layer CRTC helper.
 *
 * Note that with atomic helpers @dpms, @prepare and @commit hooks are
 * deprecated. Used @enable and @disable instead exclusively.
 *
 * With legacy crtc helpers there's a big semantic difference between @disable
 * and the other hooks: @disable also needs to release any resources acquired in
 * @mode_set (like shared PLLs).
 */
struct drm_encoder_helper_funcs {
	void (*dpms)(struct drm_encoder *encoder, int mode);
	void (*save)(struct drm_encoder *encoder);
	void (*restore)(struct drm_encoder *encoder);

	bool (*mode_fixup)(struct drm_encoder *encoder,
			   const struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	void (*prepare)(struct drm_encoder *encoder);
	void (*commit)(struct drm_encoder *encoder);
	void (*mode_set)(struct drm_encoder *encoder,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode);
	struct drm_crtc *(*get_crtc)(struct drm_encoder *encoder);
	/* detect for DAC style encoders */
	enum drm_connector_status (*detect)(struct drm_encoder *encoder,
					    struct drm_connector *connector);
	void (*disable)(struct drm_encoder *encoder);

	void (*enable)(struct drm_encoder *encoder);

	/* atomic helpers */
	int (*atomic_check)(struct drm_encoder *encoder,
			    struct drm_crtc_state *crtc_state,
			    struct drm_connector_state *conn_state);
};

/**
 * struct drm_connector_helper_funcs - helper operations for connectors
 * @get_modes: get mode list for this connector
 * @mode_valid: is this mode valid on the given connector? (optional)
 * @best_encoder: return the preferred encoder for this connector
 * @atomic_best_encoder: atomic version of @best_encoder
 *
 * The helper operations are called by the mid-layer CRTC helper.
 */
struct drm_connector_helper_funcs {
	int (*get_modes)(struct drm_connector *connector);
	enum drm_mode_status (*mode_valid)(struct drm_connector *connector,
					   struct drm_display_mode *mode);
	struct drm_encoder *(*best_encoder)(struct drm_connector *connector);
	struct drm_encoder *(*atomic_best_encoder)(struct drm_connector *connector,
						   struct drm_connector_state *connector_state);
};

extern void drm_helper_disable_unused_functions(struct drm_device *dev);
extern int drm_crtc_helper_set_config(struct drm_mode_set *set);
extern bool drm_crtc_helper_set_mode(struct drm_crtc *crtc,
				     struct drm_display_mode *mode,
				     int x, int y,
				     struct drm_framebuffer *old_fb);
extern bool drm_helper_crtc_in_use(struct drm_crtc *crtc);
extern bool drm_helper_encoder_in_use(struct drm_encoder *encoder);

extern int drm_helper_connector_dpms(struct drm_connector *connector, int mode);

extern void drm_helper_move_panel_connectors_to_head(struct drm_device *);

extern void drm_helper_mode_fill_fb_struct(struct drm_framebuffer *fb,
					   struct drm_mode_fb_cmd2 *mode_cmd);

static inline void drm_crtc_helper_add(struct drm_crtc *crtc,
				       const struct drm_crtc_helper_funcs *funcs)
{
	crtc->helper_private = funcs;
}

static inline void drm_encoder_helper_add(struct drm_encoder *encoder,
					  const struct drm_encoder_helper_funcs *funcs)
{
	encoder->helper_private = funcs;
}

static inline void drm_connector_helper_add(struct drm_connector *connector,
					    const struct drm_connector_helper_funcs *funcs)
{
	connector->helper_private = funcs;
}

extern void drm_helper_resume_force_mode(struct drm_device *dev);

int drm_helper_crtc_mode_set(struct drm_crtc *crtc, struct drm_display_mode *mode,
			     struct drm_display_mode *adjusted_mode, int x, int y,
			     struct drm_framebuffer *old_fb);
int drm_helper_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
				  struct drm_framebuffer *old_fb);

/* drm_probe_helper.c */
extern int drm_helper_probe_single_connector_modes(struct drm_connector
						   *connector, uint32_t maxX,
						   uint32_t maxY);
extern int drm_helper_probe_single_connector_modes_nomerge(struct drm_connector
							   *connector,
							   uint32_t maxX,
							   uint32_t maxY);
extern void drm_kms_helper_poll_init(struct drm_device *dev);
extern void drm_kms_helper_poll_fini(struct drm_device *dev);
extern bool drm_helper_hpd_irq_event(struct drm_device *dev);
extern void drm_kms_helper_hotplug_event(struct drm_device *dev);

extern void drm_kms_helper_poll_disable(struct drm_device *dev);
extern void drm_kms_helper_poll_enable(struct drm_device *dev);
extern void drm_kms_helper_poll_enable_locked(struct drm_device *dev);
extern bool drm_kms_helper_is_poll_worker(void);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

#endif
