/*
 * Copyright (c) 2006-2008 Intel Corporation
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 * Copyright (c) 2008 Red Hat Inc.
 *
 * DRM core CRTC related functions
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Authors:
 *      Keith Packard
 *	Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/dma-fence.h>
#include <linux/uaccess.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_atomic.h>
#include <drm/drm_auth.h>
#include <drm/drm_debugfs_crc.h>
#include <drm/drm_drv.h>
#include <drm/drm_print.h>
#include <drm/drm_file.h>

#include "drm_crtc_internal.h"
#include "drm_internal.h"

/**
 * DOC: overview
 *
 * A CRTC represents the overall display pipeline. It receives pixel data from
 * &drm_plane and blends them together. The &drm_display_mode is also attached
 * to the CRTC, specifying display timings. On the output side the data is fed
 * to one or more &drm_encoder, which are then each connected to one
 * &drm_connector.
 *
 * To create a CRTC, a KMS drivers allocates and zeroes an instances of
 * &struct drm_crtc (possibly as part of a larger structure) and registers it
 * with a call to drm_crtc_init_with_planes().
 *
 * The CRTC is also the entry point for legacy modeset operations, see
 * &drm_crtc_funcs.set_config, legacy plane operations, see
 * &drm_crtc_funcs.page_flip and &drm_crtc_funcs.cursor_set2, and other legacy
 * operations like &drm_crtc_funcs.gamma_set. For atomic drivers all these
 * features are controlled through &drm_property and
 * &drm_mode_config_funcs.atomic_check and &drm_mode_config_funcs.atomic_check.
 */

/**
 * drm_crtc_from_index - find the registered CRTC at an index
 * @dev: DRM device
 * @idx: index of registered CRTC to find for
 *
 * Given a CRTC index, return the registered CRTC from DRM device's
 * list of CRTCs with matching index. This is the inverse of drm_crtc_index().
 * It's useful in the vblank callbacks (like &drm_driver.enable_vblank or
 * &drm_driver.disable_vblank), since that still deals with indices instead
 * of pointers to &struct drm_crtc."
 */
struct drm_crtc *drm_crtc_from_index(struct drm_device *dev, int idx)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, dev)
		if (idx == crtc->index)
			return crtc;

	return NULL;
}
EXPORT_SYMBOL(drm_crtc_from_index);

int drm_crtc_force_disable(struct drm_crtc *crtc)
{
	struct drm_mode_set set = {
		.crtc = crtc,
	};

	WARN_ON(drm_drv_uses_atomic_modeset(crtc->dev));

	return drm_mode_set_config_internal(&set);
}

static unsigned int drm_num_crtcs(struct drm_device *dev)
{
	unsigned int num = 0;
	struct drm_crtc *tmp;

	drm_for_each_crtc(tmp, dev) {
		num++;
	}

	return num;
}

int drm_crtc_register_all(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	int ret = 0;

	drm_for_each_crtc(crtc, dev) {
		drm_debugfs_crtc_add(crtc);

		if (crtc->funcs->late_register)
			ret = crtc->funcs->late_register(crtc);
		if (ret)
			return ret;
	}

	return 0;
}

void drm_crtc_unregister_all(struct drm_device *dev)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, dev) {
		if (crtc->funcs->early_unregister)
			crtc->funcs->early_unregister(crtc);
		drm_debugfs_crtc_remove(crtc);
	}
}

static int drm_crtc_crc_init(struct drm_crtc *crtc)
{
#ifdef CONFIG_DEBUG_FS
	spin_lock_init(&crtc->crc.lock);
	init_waitqueue_head(&crtc->crc.wq);
	crtc->crc.source = kstrdup("auto", GFP_KERNEL);
	if (!crtc->crc.source)
		return -ENOMEM;
#endif
	return 0;
}

static void drm_crtc_crc_fini(struct drm_crtc *crtc)
{
#ifdef CONFIG_DEBUG_FS
	kfree(crtc->crc.source);
#endif
}

static const struct dma_fence_ops drm_crtc_fence_ops;

static struct drm_crtc *fence_to_crtc(struct dma_fence *fence)
{
	BUG_ON(fence->ops != &drm_crtc_fence_ops);
	return container_of(fence->lock, struct drm_crtc, fence_lock);
}

static const char *drm_crtc_fence_get_driver_name(struct dma_fence *fence)
{
	struct drm_crtc *crtc = fence_to_crtc(fence);

	return crtc->dev->driver->name;
}

static const char *drm_crtc_fence_get_timeline_name(struct dma_fence *fence)
{
	struct drm_crtc *crtc = fence_to_crtc(fence);

	return crtc->timeline_name;
}

static const struct dma_fence_ops drm_crtc_fence_ops = {
	.get_driver_name = drm_crtc_fence_get_driver_name,
	.get_timeline_name = drm_crtc_fence_get_timeline_name,
};

struct dma_fence *drm_crtc_create_fence(struct drm_crtc *crtc)
{
	struct dma_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return NULL;

	dma_fence_init(fence, &drm_crtc_fence_ops, &crtc->fence_lock,
		       crtc->fence_context, ++crtc->fence_seqno);

	return fence;
}

/**
 * drm_crtc_init_with_planes - Initialise a new CRTC object with
 *    specified primary and cursor planes.
 * @dev: DRM device
 * @crtc: CRTC object to init
 * @primary: Primary plane for CRTC
 * @cursor: Cursor plane for CRTC
 * @funcs: callbacks for the new CRTC
 * @name: printf style format string for the CRTC name, or NULL for default name
 *
 * Inits a new object created as base part of a driver crtc object. Drivers
 * should use this function instead of drm_crtc_init(), which is only provided
 * for backwards compatibility with drivers which do not yet support universal
 * planes). For really simple hardware which has only 1 plane look at
 * drm_simple_display_pipe_init() instead.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_crtc_init_with_planes(struct drm_device *dev, struct drm_crtc *crtc,
			      struct drm_plane *primary,
			      struct drm_plane *cursor,
			      const struct drm_crtc_funcs *funcs,
			      const char *name, ...)
{
	struct drm_mode_config *config = &dev->mode_config;
	int ret;

	WARN_ON(primary && primary->type != DRM_PLANE_TYPE_PRIMARY);
	WARN_ON(cursor && cursor->type != DRM_PLANE_TYPE_CURSOR);

	/* crtc index is used with 32bit bitmasks */
	if (WARN_ON(config->num_crtc >= 32))
		return -EINVAL;

	WARN_ON(drm_drv_uses_atomic_modeset(dev) &&
		(!funcs->atomic_destroy_state ||
		 !funcs->atomic_duplicate_state));

	crtc->dev = dev;
	crtc->funcs = funcs;

	INIT_LIST_HEAD(&crtc->commit_list);
	spin_lock_init(&crtc->commit_lock);

	drm_modeset_lock_init(&crtc->mutex);
	ret = drm_mode_object_add(dev, &crtc->base, DRM_MODE_OBJECT_CRTC);
	if (ret)
		return ret;

	if (name) {
		va_list ap;

		va_start(ap, name);
		crtc->name = kvasprintf(GFP_KERNEL, name, ap);
		va_end(ap);
	} else {
		crtc->name = kasprintf(GFP_KERNEL, "crtc-%d",
				       drm_num_crtcs(dev));
	}
	if (!crtc->name) {
		drm_mode_object_unregister(dev, &crtc->base);
		return -ENOMEM;
	}

	crtc->fence_context = dma_fence_context_alloc(1);
	spin_lock_init(&crtc->fence_lock);
	snprintf(crtc->timeline_name, sizeof(crtc->timeline_name),
		 "CRTC:%d-%s", crtc->base.id, crtc->name);

	crtc->base.properties = &crtc->properties;

	list_add_tail(&crtc->head, &config->crtc_list);
	crtc->index = config->num_crtc++;

	crtc->primary = primary;
	crtc->cursor = cursor;
	if (primary && !primary->possible_crtcs)
		primary->possible_crtcs = drm_crtc_mask(crtc);
	if (cursor && !cursor->possible_crtcs)
		cursor->possible_crtcs = drm_crtc_mask(crtc);

	ret = drm_crtc_crc_init(crtc);
	if (ret) {
		drm_mode_object_unregister(dev, &crtc->base);
		return ret;
	}

	if (drm_core_check_feature(dev, DRIVER_ATOMIC)) {
		drm_object_attach_property(&crtc->base, config->prop_active, 0);
		drm_object_attach_property(&crtc->base, config->prop_mode_id, 0);
		drm_object_attach_property(&crtc->base,
					   config->prop_out_fence_ptr, 0);
		drm_object_attach_property(&crtc->base,
					   config->prop_vrr_enabled, 0);
	}

	return 0;
}
EXPORT_SYMBOL(drm_crtc_init_with_planes);

/**
 * drm_crtc_cleanup - Clean up the core crtc usage
 * @crtc: CRTC to cleanup
 *
 * This function cleans up @crtc and removes it from the DRM mode setting
 * core. Note that the function does *not* free the crtc structure itself,
 * this is the responsibility of the caller.
 */
void drm_crtc_cleanup(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;

	/* Note that the crtc_list is considered to be static; should we
	 * remove the drm_crtc at runtime we would have to decrement all
	 * the indices on the drm_crtc after us in the crtc_list.
	 */

	drm_crtc_crc_fini(crtc);

	kfree(crtc->gamma_store);
	crtc->gamma_store = NULL;

	drm_modeset_lock_fini(&crtc->mutex);

	drm_mode_object_unregister(dev, &crtc->base);
	list_del(&crtc->head);
	dev->mode_config.num_crtc--;

	WARN_ON(crtc->state && !crtc->funcs->atomic_destroy_state);
	if (crtc->state && crtc->funcs->atomic_destroy_state)
		crtc->funcs->atomic_destroy_state(crtc, crtc->state);

	kfree(crtc->name);

	memset(crtc, 0, sizeof(*crtc));
}
EXPORT_SYMBOL(drm_crtc_cleanup);

/**
 * drm_mode_getcrtc - get CRTC configuration
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Construct a CRTC configuration structure to return to the user.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_getcrtc(struct drm_device *dev,
		     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc *crtc_resp = data;
	struct drm_crtc *crtc;
	struct drm_plane *plane;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	crtc = drm_crtc_find(dev, file_priv, crtc_resp->crtc_id);
	if (!crtc)
		return -ENOENT;

	plane = crtc->primary;

	crtc_resp->gamma_size = crtc->gamma_size;

	drm_modeset_lock(&plane->mutex, NULL);
	if (plane->state && plane->state->fb)
		crtc_resp->fb_id = plane->state->fb->base.id;
	else if (!plane->state && plane->fb)
		crtc_resp->fb_id = plane->fb->base.id;
	else
		crtc_resp->fb_id = 0;

	if (plane->state) {
		crtc_resp->x = plane->state->src_x >> 16;
		crtc_resp->y = plane->state->src_y >> 16;
	}
	drm_modeset_unlock(&plane->mutex);

	drm_modeset_lock(&crtc->mutex, NULL);
	if (crtc->state) {
		if (crtc->state->enable) {
			drm_mode_convert_to_umode(&crtc_resp->mode, &crtc->state->mode);
			crtc_resp->mode_valid = 1;
		} else {
			crtc_resp->mode_valid = 0;
		}
	} else {
		crtc_resp->x = crtc->x;
		crtc_resp->y = crtc->y;

		if (crtc->enabled) {
			drm_mode_convert_to_umode(&crtc_resp->mode, &crtc->mode);
			crtc_resp->mode_valid = 1;

		} else {
			crtc_resp->mode_valid = 0;
		}
	}
	if (!file_priv->aspect_ratio_allowed)
		crtc_resp->mode.flags &= ~DRM_MODE_FLAG_PIC_AR_MASK;
	drm_modeset_unlock(&crtc->mutex);

	return 0;
}

static int __drm_mode_set_config_internal(struct drm_mode_set *set,
					  struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_crtc *crtc = set->crtc;
	struct drm_framebuffer *fb;
	struct drm_crtc *tmp;
	int ret;

	WARN_ON(drm_drv_uses_atomic_modeset(crtc->dev));

	/*
	 * NOTE: ->set_config can also disable other crtcs (if we steal all
	 * connectors from it), hence we need to refcount the fbs across all
	 * crtcs. Atomic modeset will have saner semantics ...
	 */
	drm_for_each_crtc(tmp, crtc->dev) {
		struct drm_plane *plane = tmp->primary;

		plane->old_fb = plane->fb;
	}

	fb = set->fb;

	ret = crtc->funcs->set_config(set, ctx);
	if (ret == 0) {
		struct drm_plane *plane = crtc->primary;

		plane->crtc = fb ? crtc : NULL;
		plane->fb = fb;
	}

	drm_for_each_crtc(tmp, crtc->dev) {
		struct drm_plane *plane = tmp->primary;

		if (plane->fb)
			drm_framebuffer_get(plane->fb);
		if (plane->old_fb)
			drm_framebuffer_put(plane->old_fb);
		plane->old_fb = NULL;
	}

	return ret;
}

/**
 * drm_mode_set_config_internal - helper to call &drm_mode_config_funcs.set_config
 * @set: modeset config to set
 *
 * This is a little helper to wrap internal calls to the
 * &drm_mode_config_funcs.set_config driver interface. The only thing it adds is
 * correct refcounting dance.
 *
 * This should only be used by non-atomic legacy drivers.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_set_config_internal(struct drm_mode_set *set)
{
	WARN_ON(drm_drv_uses_atomic_modeset(set->crtc->dev));

	return __drm_mode_set_config_internal(set, NULL);
}
EXPORT_SYMBOL(drm_mode_set_config_internal);

/**
 * drm_crtc_check_viewport - Checks that a framebuffer is big enough for the
 *     CRTC viewport
 * @crtc: CRTC that framebuffer will be displayed on
 * @x: x panning
 * @y: y panning
 * @mode: mode that framebuffer will be displayed under
 * @fb: framebuffer to check size of
 */
int drm_crtc_check_viewport(const struct drm_crtc *crtc,
			    int x, int y,
			    const struct drm_display_mode *mode,
			    const struct drm_framebuffer *fb)

{
	int hdisplay, vdisplay;

	drm_mode_get_hv_timing(mode, &hdisplay, &vdisplay);

	if (crtc->state &&
	    drm_rotation_90_or_270(crtc->primary->state->rotation))
		swap(hdisplay, vdisplay);

	return drm_framebuffer_check_src_coords(x << 16, y << 16,
						hdisplay << 16, vdisplay << 16,
						fb);
}
EXPORT_SYMBOL(drm_crtc_check_viewport);

/**
 * drm_mode_setcrtc - set CRTC configuration
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Build a new CRTC configuration based on user request.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_setcrtc(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_mode_crtc *crtc_req = data;
	struct drm_crtc *crtc;
	struct drm_plane *plane;
	struct drm_connector **connector_set = NULL, *connector;
	struct drm_framebuffer *fb = NULL;
	struct drm_display_mode *mode = NULL;
	struct drm_mode_set set;
	uint32_t __user *set_connectors_ptr;
	struct drm_modeset_acquire_ctx ctx;
	int ret;
	int i;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	/*
	 * Universal plane src offsets are only 16.16, prevent havoc for
	 * drivers using universal plane code internally.
	 */
	if (crtc_req->x & 0xffff0000 || crtc_req->y & 0xffff0000)
		return -ERANGE;

	crtc = drm_crtc_find(dev, file_priv, crtc_req->crtc_id);
	if (!crtc) {
		DRM_DEBUG_KMS("Unknown CRTC ID %d\n", crtc_req->crtc_id);
		return -ENOENT;
	}
	DRM_DEBUG_KMS("[CRTC:%d:%s]\n", crtc->base.id, crtc->name);

	plane = crtc->primary;

	/* allow disabling with the primary plane leased */
	if (crtc_req->mode_valid && !drm_lease_held(file_priv, plane->base.id))
		return -EACCES;

	mutex_lock(&crtc->dev->mode_config.mutex);
	DRM_MODESET_LOCK_ALL_BEGIN(dev, ctx,
				   DRM_MODESET_ACQUIRE_INTERRUPTIBLE, ret);

	if (crtc_req->mode_valid) {
		/* If we have a mode we need a framebuffer. */
		/* If we pass -1, set the mode with the currently bound fb */
		if (crtc_req->fb_id == -1) {
			struct drm_framebuffer *old_fb;

			if (plane->state)
				old_fb = plane->state->fb;
			else
				old_fb = plane->fb;

			if (!old_fb) {
				DRM_DEBUG_KMS("CRTC doesn't have current FB\n");
				ret = -EINVAL;
				goto out;
			}

			fb = old_fb;
			/* Make refcounting symmetric with the lookup path. */
			drm_framebuffer_get(fb);
		} else {
			fb = drm_framebuffer_lookup(dev, file_priv, crtc_req->fb_id);
			if (!fb) {
				DRM_DEBUG_KMS("Unknown FB ID%d\n",
						crtc_req->fb_id);
				ret = -ENOENT;
				goto out;
			}
		}

		mode = drm_mode_create(dev);
		if (!mode) {
			ret = -ENOMEM;
			goto out;
		}
		if (!file_priv->aspect_ratio_allowed &&
		    (crtc_req->mode.flags & DRM_MODE_FLAG_PIC_AR_MASK) != DRM_MODE_FLAG_PIC_AR_NONE) {
			DRM_DEBUG_KMS("Unexpected aspect-ratio flag bits\n");
			ret = -EINVAL;
			goto out;
		}

<<<<<<< HEAD

		ret = drm_mode_convert_umode(dev, mode, &crtc_req->mode);
		if (ret) {
			DRM_DEBUG_KMS("Invalid mode (ret=%d, status=%s)\n",
				      ret, drm_get_mode_status_name(mode->status));
			drm_mode_debug_printmodeline(mode);
			goto out;
		}

=======
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
		/*
		 * Check whether the primary plane supports the fb pixel format.
		 * Drivers not implementing the universal planes API use a
		 * default formats list provided by the DRM core which doesn't
		 * match real hardware capabilities. Skip the check in that
		 * case.
		 */
		if (!plane->format_default) {
			ret = drm_plane_check_pixel_format(plane,
							   fb->format->format,
							   fb->modifier);
			if (ret) {
				struct drm_format_name_buf format_name;
				DRM_DEBUG_KMS("Invalid pixel format %s, modifier 0x%llx\n",
					      drm_get_format_name(fb->format->format,
								  &format_name),
					      fb->modifier);
				goto out;
			}
		}

		ret = drm_crtc_check_viewport(crtc, crtc_req->x, crtc_req->y,
					      mode, fb);
		if (ret)
			goto out;

	}

	if (crtc_req->count_connectors == 0 && mode) {
		DRM_DEBUG_KMS("Count connectors is 0 but mode set\n");
		ret = -EINVAL;
		goto out;
	}

	if (crtc_req->count_connectors > 0 && (!mode || !fb)) {
		DRM_DEBUG_KMS("Count connectors is %d but no mode or fb set\n",
			  crtc_req->count_connectors);
		ret = -EINVAL;
		goto out;
	}

	if (crtc_req->count_connectors > 0) {
		u32 out_id;

		/* Avoid unbounded kernel memory allocation */
		if (crtc_req->count_connectors > config->num_connector) {
			ret = -EINVAL;
			goto out;
		}

		connector_set = kmalloc_array(crtc_req->count_connectors,
					      sizeof(struct drm_connector *),
					      GFP_KERNEL);
		if (!connector_set) {
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < crtc_req->count_connectors; i++) {
			connector_set[i] = NULL;
			set_connectors_ptr = (uint32_t __user *)(unsigned long)crtc_req->set_connectors_ptr;
			if (get_user(out_id, &set_connectors_ptr[i])) {
				ret = -EFAULT;
				goto out;
			}

			connector = drm_connector_lookup(dev, file_priv, out_id);
			if (!connector) {
				DRM_DEBUG_KMS("Connector id %d unknown\n",
						out_id);
				ret = -ENOENT;
				goto out;
			}
			DRM_DEBUG_KMS("[CONNECTOR:%d:%s]\n",
					connector->base.id,
					connector->name);

			connector_set[i] = connector;
		}
	}

	set.crtc = crtc;
	set.x = crtc_req->x;
	set.y = crtc_req->y;
	set.mode = mode;
	set.connectors = connector_set;
	set.num_connectors = crtc_req->count_connectors;
	set.fb = fb;

	if (drm_drv_uses_atomic_modeset(dev))
		ret = crtc->funcs->set_config(&set, &ctx);
	else
		ret = __drm_mode_set_config_internal(&set, &ctx);

out:
	if (fb)
		drm_framebuffer_put(fb);

	if (connector_set) {
		for (i = 0; i < crtc_req->count_connectors; i++) {
			if (connector_set[i])
				drm_connector_put(connector_set[i]);
		}
	}
	kfree(connector_set);
	drm_mode_destroy(dev, mode);

	/* In case we need to retry... */
	connector_set = NULL;
	fb = NULL;
	mode = NULL;

	DRM_MODESET_LOCK_ALL_END(ctx, ret);
	mutex_unlock(&crtc->dev->mode_config.mutex);

	return ret;
}

<<<<<<< HEAD
int drm_mode_crtc_set_obj_prop(struct drm_mode_object *obj,
			       struct drm_property *property,
			       uint64_t value)
=======
static int drm_mode_cursor_common(struct drm_device *dev,
				  struct drm_mode_cursor2 *req,
				  struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	if (!req->flags || (~DRM_MODE_CURSOR_FLAGS & req->flags))
		return -EINVAL;

	crtc = drm_crtc_find(dev, req->crtc_id);
	if (!crtc) {
		DRM_DEBUG_KMS("Unknown CRTC ID %d\n", req->crtc_id);
		return -ENOENT;
	}

	/*
	 * If this crtc has a universal cursor plane, call that plane's update
	 * handler rather than using legacy cursor handlers.
	 */
	drm_modeset_lock_crtc(crtc, crtc->cursor);
	if (crtc->cursor) {
		ret = drm_mode_cursor_universal(crtc, req, file_priv);
		goto out;
	}

	if (req->flags & DRM_MODE_CURSOR_BO) {
		if (!crtc->funcs->cursor_set && !crtc->funcs->cursor_set2) {
			ret = -ENXIO;
			goto out;
		}
		/* Turns off the cursor if handle is 0 */
		if (crtc->funcs->cursor_set2)
			ret = crtc->funcs->cursor_set2(crtc, file_priv, req->handle,
						      req->width, req->height, req->hot_x, req->hot_y);
		else
			ret = crtc->funcs->cursor_set(crtc, file_priv, req->handle,
						      req->width, req->height);
	}

	if (req->flags & DRM_MODE_CURSOR_MOVE) {
		if (crtc->funcs->cursor_move) {
			ret = crtc->funcs->cursor_move(crtc, req->x, req->y);
		} else {
			ret = -EFAULT;
			goto out;
		}
	}
out:
	drm_modeset_unlock_crtc(crtc);

	return ret;

}


/**
 * drm_mode_cursor_ioctl - set CRTC's cursor configuration
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Set the cursor configuration based on user request.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_cursor_ioctl(struct drm_device *dev,
			  void *data, struct drm_file *file_priv)
{
	struct drm_mode_cursor *req = data;
	struct drm_mode_cursor2 new_req;

	memcpy(&new_req, req, sizeof(struct drm_mode_cursor));
	new_req.hot_x = new_req.hot_y = 0;

	return drm_mode_cursor_common(dev, &new_req, file_priv);
}

/**
 * drm_mode_cursor2_ioctl - set CRTC's cursor configuration
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Set the cursor configuration based on user request. This implements the 2nd
 * version of the cursor ioctl, which allows userspace to additionally specify
 * the hotspot of the pointer.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_cursor2_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv)
{
	struct drm_mode_cursor2 *req = data;

	return drm_mode_cursor_common(dev, req, file_priv);
}

/**
 * drm_mode_legacy_fb_format - compute drm fourcc code from legacy description
 * @bpp: bits per pixels
 * @depth: bit depth per pixel
 *
 * Computes a drm fourcc pixel format code for the given @bpp/@depth values.
 * Useful in fbdev emulation code, since that deals in those values.
 */
uint32_t drm_mode_legacy_fb_format(uint32_t bpp, uint32_t depth)
{
	uint32_t fmt;

	switch (bpp) {
	case 8:
		fmt = DRM_FORMAT_C8;
		break;
	case 16:
		if (depth == 15)
			fmt = DRM_FORMAT_XRGB1555;
		else
			fmt = DRM_FORMAT_RGB565;
		break;
	case 24:
		fmt = DRM_FORMAT_RGB888;
		break;
	case 32:
		if (depth == 24)
			fmt = DRM_FORMAT_XRGB8888;
		else if (depth == 30)
			fmt = DRM_FORMAT_XRGB2101010;
		else
			fmt = DRM_FORMAT_ARGB8888;
		break;
	default:
		DRM_ERROR("bad bpp, assuming x8r8g8b8 pixel format\n");
		fmt = DRM_FORMAT_XRGB8888;
		break;
	}

	return fmt;
}
EXPORT_SYMBOL(drm_mode_legacy_fb_format);

/**
 * drm_mode_addfb - add an FB to the graphics configuration
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Add a new FB to the specified CRTC, given a user request. This is the
 * original addfb ioctl which only supported RGB formats.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_addfb(struct drm_device *dev,
		   void *data, struct drm_file *file_priv)
{
	struct drm_mode_fb_cmd *or = data;
	struct drm_mode_fb_cmd2 r = {};
	int ret;

	/* convert to new format and call new ioctl */
	r.fb_id = or->fb_id;
	r.width = or->width;
	r.height = or->height;
	r.pitches[0] = or->pitch;
	r.pixel_format = drm_mode_legacy_fb_format(or->bpp, or->depth);
	r.handles[0] = or->handle;

	ret = drm_mode_addfb2(dev, &r, file_priv);
	if (ret)
		return ret;

	or->fb_id = r.fb_id;

	return 0;
}

static int format_check(const struct drm_mode_fb_cmd2 *r)
{
	uint32_t format = r->pixel_format & ~DRM_FORMAT_BIG_ENDIAN;

	switch (format) {
	case DRM_FORMAT_C8:
	case DRM_FORMAT_RGB332:
	case DRM_FORMAT_BGR233:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_AYUV:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV24:
	case DRM_FORMAT_NV42:
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YVU410:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YVU411:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		return 0;
	default:
		DRM_DEBUG_KMS("invalid pixel format %s\n",
			      drm_get_format_name(r->pixel_format));
		return -EINVAL;
	}
}

static int framebuffer_check(const struct drm_mode_fb_cmd2 *r)
{
	int ret, hsub, vsub, num_planes, i;

	ret = format_check(r);
	if (ret) {
		DRM_DEBUG_KMS("bad framebuffer format %s\n",
			      drm_get_format_name(r->pixel_format));
		return ret;
	}

	hsub = drm_format_horz_chroma_subsampling(r->pixel_format);
	vsub = drm_format_vert_chroma_subsampling(r->pixel_format);
	num_planes = drm_format_num_planes(r->pixel_format);

	if (r->width == 0 || r->width % hsub) {
		DRM_DEBUG_KMS("bad framebuffer width %u\n", r->width);
		return -EINVAL;
	}

	if (r->height == 0 || r->height % vsub) {
		DRM_DEBUG_KMS("bad framebuffer height %u\n", r->height);
		return -EINVAL;
	}

	for (i = 0; i < num_planes; i++) {
		unsigned int width = r->width / (i != 0 ? hsub : 1);
		unsigned int height = r->height / (i != 0 ? vsub : 1);
		unsigned int cpp = drm_format_plane_cpp(r->pixel_format, i);

		if (!r->handles[i]) {
			DRM_DEBUG_KMS("no buffer object handle for plane %d\n", i);
			return -EINVAL;
		}

		if ((uint64_t) width * cpp > UINT_MAX)
			return -ERANGE;

		if ((uint64_t) height * r->pitches[i] + r->offsets[i] > UINT_MAX)
			return -ERANGE;

		if (r->pitches[i] < width * cpp) {
			DRM_DEBUG_KMS("bad pitch %u for plane %d\n", r->pitches[i], i);
			return -EINVAL;
		}

		if (r->modifier[i] && !(r->flags & DRM_MODE_FB_MODIFIERS)) {
			DRM_DEBUG_KMS("bad fb modifier %llu for plane %d\n",
				      r->modifier[i], i);
			return -EINVAL;
		}

		/* modifier specific checks: */
		switch (r->modifier[i]) {
		case DRM_FORMAT_MOD_SAMSUNG_64_32_TILE:
			/* NOTE: the pitch restriction may be lifted later if it turns
			 * out that no hw has this restriction:
			 */
			if (r->pixel_format != DRM_FORMAT_NV12 ||
					width % 128 || height % 32 ||
					r->pitches[i] % 128) {
				DRM_DEBUG_KMS("bad modifier data for plane %d\n", i);
				return -EINVAL;
			}
			break;

		default:
			break;
		}
	}

	for (i = num_planes; i < 4; i++) {
		if (r->modifier[i]) {
			DRM_DEBUG_KMS("non-zero modifier for unused plane %d\n", i);
			return -EINVAL;
		}

		/* Pre-FB_MODIFIERS userspace didn't clear the structs properly. */
		if (!(r->flags & DRM_MODE_FB_MODIFIERS))
			continue;

		if (r->handles[i]) {
			DRM_DEBUG_KMS("buffer object handle for unused plane %d\n", i);
			return -EINVAL;
		}

		if (r->pitches[i]) {
			DRM_DEBUG_KMS("non-zero pitch for unused plane %d\n", i);
			return -EINVAL;
		}

		if (r->offsets[i]) {
			DRM_DEBUG_KMS("non-zero offset for unused plane %d\n", i);
			return -EINVAL;
		}
	}

	return 0;
}

static struct drm_framebuffer *
internal_framebuffer_create(struct drm_device *dev,
			    struct drm_mode_fb_cmd2 *r,
			    struct drm_file *file_priv)
{
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_framebuffer *fb;
	int ret;

	if (r->flags & ~(DRM_MODE_FB_INTERLACED | DRM_MODE_FB_MODIFIERS)) {
		DRM_DEBUG_KMS("bad framebuffer flags 0x%08x\n", r->flags);
		return ERR_PTR(-EINVAL);
	}

	if ((config->min_width > r->width) || (r->width > config->max_width)) {
		DRM_DEBUG_KMS("bad framebuffer width %d, should be >= %d && <= %d\n",
			  r->width, config->min_width, config->max_width);
		return ERR_PTR(-EINVAL);
	}
	if ((config->min_height > r->height) || (r->height > config->max_height)) {
		DRM_DEBUG_KMS("bad framebuffer height %d, should be >= %d && <= %d\n",
			  r->height, config->min_height, config->max_height);
		return ERR_PTR(-EINVAL);
	}

	if (r->flags & DRM_MODE_FB_MODIFIERS &&
	    !dev->mode_config.allow_fb_modifiers) {
		DRM_DEBUG_KMS("driver does not support fb modifiers\n");
		return ERR_PTR(-EINVAL);
	}

	ret = framebuffer_check(r);
	if (ret)
		return ERR_PTR(ret);

	fb = dev->mode_config.funcs->fb_create(dev, file_priv, r);
	if (IS_ERR(fb)) {
		DRM_DEBUG_KMS("could not create framebuffer\n");
		return fb;
	}

	return fb;
}

/**
 * drm_mode_addfb2 - add an FB to the graphics configuration
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Add a new FB to the specified CRTC, given a user request with format. This is
 * the 2nd version of the addfb ioctl, which supports multi-planar framebuffers
 * and uses fourcc codes as pixel format specifiers.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_addfb2(struct drm_device *dev,
		    void *data, struct drm_file *file_priv)
{
	struct drm_mode_fb_cmd2 *r = data;
	struct drm_framebuffer *fb;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	fb = internal_framebuffer_create(dev, r, file_priv);
	if (IS_ERR(fb))
		return PTR_ERR(fb);

	/* Transfer ownership to the filp for reaping on close */

	DRM_DEBUG_KMS("[FB:%d]\n", fb->base.id);
	mutex_lock(&file_priv->fbs_lock);
	r->fb_id = fb->base.id;
	list_add(&fb->filp_head, &file_priv->fbs);
	mutex_unlock(&file_priv->fbs_lock);

	return 0;
}

struct drm_mode_rmfb_work {
	struct work_struct work;
	struct list_head fbs;
};

static void drm_mode_rmfb_work_fn(struct work_struct *w)
{
	struct drm_mode_rmfb_work *arg = container_of(w, typeof(*arg), work);

	while (!list_empty(&arg->fbs)) {
		struct drm_framebuffer *fb =
			list_first_entry(&arg->fbs, typeof(*fb), filp_head);

		list_del_init(&fb->filp_head);
		drm_framebuffer_remove(fb);
	}
}

/**
 * drm_mode_rmfb - remove an FB from the configuration
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Remove the FB specified by the user.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_rmfb(struct drm_device *dev,
		   void *data, struct drm_file *file_priv)
{
	struct drm_framebuffer *fb = NULL;
	struct drm_framebuffer *fbl = NULL;
	uint32_t *id = data;
	int found = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	mutex_lock(&file_priv->fbs_lock);
	mutex_lock(&dev->mode_config.fb_lock);
	fb = __drm_framebuffer_lookup(dev, *id);
	if (!fb)
		goto fail_lookup;

	list_for_each_entry(fbl, &file_priv->fbs, filp_head)
		if (fb == fbl)
			found = 1;
	if (!found)
		goto fail_lookup;

	list_del_init(&fb->filp_head);
	mutex_unlock(&dev->mode_config.fb_lock);
	mutex_unlock(&file_priv->fbs_lock);

	/*
	 * we now own the reference that was stored in the fbs list
	 *
	 * drm_framebuffer_remove may fail with -EINTR on pending signals,
	 * so run this in a separate stack as there's no way to correctly
	 * handle this after the fb is already removed from the lookup table.
	 */
	if (atomic_read(&fb->refcount.refcount) > 1) {
		struct drm_mode_rmfb_work arg;

		INIT_WORK_ONSTACK(&arg.work, drm_mode_rmfb_work_fn);
		INIT_LIST_HEAD(&arg.fbs);
		list_add_tail(&fb->filp_head, &arg.fbs);

		schedule_work(&arg.work);
		flush_work(&arg.work);
		destroy_work_on_stack(&arg.work);
	} else
		drm_framebuffer_unreference(fb);

	return 0;

fail_lookup:
	mutex_unlock(&dev->mode_config.fb_lock);
	mutex_unlock(&file_priv->fbs_lock);

	return -ENOENT;
}

/**
 * drm_mode_getfb - get FB info
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Lookup the FB given its ID and return info about it.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_getfb(struct drm_device *dev,
		   void *data, struct drm_file *file_priv)
{
	struct drm_mode_fb_cmd *r = data;
	struct drm_framebuffer *fb;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	fb = drm_framebuffer_lookup(dev, r->fb_id);
	if (!fb)
		return -ENOENT;

	r->height = fb->height;
	r->width = fb->width;
	r->depth = fb->depth;
	r->bpp = fb->bits_per_pixel;
	r->pitch = fb->pitches[0];
	if (fb->funcs->create_handle) {
		if (file_priv->is_master || capable(CAP_SYS_ADMIN) ||
		    drm_is_control_client(file_priv)) {
			ret = fb->funcs->create_handle(fb, file_priv,
						       &r->handle);
		} else {
			/* GET_FB() is an unprivileged ioctl so we must not
			 * return a buffer-handle to non-master processes! For
			 * backwards-compatibility reasons, we cannot make
			 * GET_FB() privileged, so just return an invalid handle
			 * for non-masters. */
			r->handle = 0;
			ret = 0;
		}
	} else {
		ret = -ENODEV;
	}

	drm_framebuffer_unreference(fb);

	return ret;
}

/**
 * drm_mode_dirtyfb_ioctl - flush frontbuffer rendering on an FB
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Lookup the FB and flush out the damaged area supplied by userspace as a clip
 * rectangle list. Generic userspace which does frontbuffer rendering must call
 * this ioctl to flush out the changes on manual-update display outputs, e.g.
 * usb display-link, mipi manual update panels or edp panel self refresh modes.
 *
 * Modesetting drivers which always update the frontbuffer do not need to
 * implement the corresponding ->dirty framebuffer callback.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_dirtyfb_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv)
{
	struct drm_clip_rect __user *clips_ptr;
	struct drm_clip_rect *clips = NULL;
	struct drm_mode_fb_dirty_cmd *r = data;
	struct drm_framebuffer *fb;
	unsigned flags;
	int num_clips;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	fb = drm_framebuffer_lookup(dev, r->fb_id);
	if (!fb)
		return -ENOENT;

	num_clips = r->num_clips;
	clips_ptr = (struct drm_clip_rect __user *)(unsigned long)r->clips_ptr;

	if (!num_clips != !clips_ptr) {
		ret = -EINVAL;
		goto out_err1;
	}

	flags = DRM_MODE_FB_DIRTY_FLAGS & r->flags;

	/* If userspace annotates copy, clips must come in pairs */
	if (flags & DRM_MODE_FB_DIRTY_ANNOTATE_COPY && (num_clips % 2)) {
		ret = -EINVAL;
		goto out_err1;
	}

	if (num_clips && clips_ptr) {
		if (num_clips < 0 || num_clips > DRM_MODE_FB_DIRTY_MAX_CLIPS) {
			ret = -EINVAL;
			goto out_err1;
		}
		clips = kcalloc(num_clips, sizeof(*clips), GFP_KERNEL);
		if (!clips) {
			ret = -ENOMEM;
			goto out_err1;
		}

		ret = copy_from_user(clips, clips_ptr,
				     num_clips * sizeof(*clips));
		if (ret) {
			ret = -EFAULT;
			goto out_err2;
		}
	}

	if (fb->funcs->dirty) {
		ret = fb->funcs->dirty(fb, file_priv, flags, r->color,
				       clips, num_clips);
	} else {
		ret = -ENOSYS;
	}

out_err2:
	kfree(clips);
out_err1:
	drm_framebuffer_unreference(fb);

	return ret;
}

/**
 * drm_fb_release - remove and free the FBs on this file
 * @priv: drm file for the ioctl
 *
 * Destroy all the FBs associated with @filp.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
void drm_fb_release(struct drm_file *priv)
{
	struct drm_framebuffer *fb, *tfb;
	struct drm_mode_rmfb_work arg;

	INIT_LIST_HEAD(&arg.fbs);

	/*
	 * When the file gets released that means no one else can access the fb
	 * list any more, so no need to grab fpriv->fbs_lock. And we need to
	 * avoid upsetting lockdep since the universal cursor code adds a
	 * framebuffer while holding mutex locks.
	 *
	 * Note that a real deadlock between fpriv->fbs_lock and the modeset
	 * locks is impossible here since no one else but this function can get
	 * at it any more.
	 */
	list_for_each_entry_safe(fb, tfb, &priv->fbs, filp_head) {
		if (atomic_read(&fb->refcount.refcount) > 1) {
			list_move_tail(&fb->filp_head, &arg.fbs);
		} else {
			list_del_init(&fb->filp_head);

			/* This drops the fpriv->fbs reference. */
			drm_framebuffer_unreference(fb);
		}
	}

	if (!list_empty(&arg.fbs)) {
		INIT_WORK_ONSTACK(&arg.work, drm_mode_rmfb_work_fn);

		schedule_work(&arg.work);
		flush_work(&arg.work);
		destroy_work_on_stack(&arg.work);
	}
}

/**
 * drm_property_create - create a new property type
 * @dev: drm device
 * @flags: flags specifying the property type
 * @name: name of the property
 * @num_values: number of pre-defined values
 *
 * This creates a new generic drm property which can then be attached to a drm
 * object with drm_object_attach_property. The returned property object must be
 * freed with drm_property_destroy.
 *
 * Note that the DRM core keeps a per-device list of properties and that, if
 * drm_mode_config_cleanup() is called, it will destroy all properties created
 * by the driver.
 *
 * Returns:
 * A pointer to the newly created property on success, NULL on failure.
 */
struct drm_property *drm_property_create(struct drm_device *dev, int flags,
					 const char *name, int num_values)
{
	struct drm_property *property = NULL;
	int ret;

	property = kzalloc(sizeof(struct drm_property), GFP_KERNEL);
	if (!property)
		return NULL;

	property->dev = dev;

	if (num_values) {
		property->values = kcalloc(num_values, sizeof(uint64_t),
					   GFP_KERNEL);
		if (!property->values)
			goto fail;
	}

	ret = drm_mode_object_get(dev, &property->base, DRM_MODE_OBJECT_PROPERTY);
	if (ret)
		goto fail;

	property->flags = flags;
	property->num_values = num_values;
	INIT_LIST_HEAD(&property->enum_list);

	if (name) {
		strncpy(property->name, name, DRM_PROP_NAME_LEN);
		property->name[DRM_PROP_NAME_LEN-1] = '\0';
	}

	list_add_tail(&property->head, &dev->mode_config.property_list);

	WARN_ON(!drm_property_type_valid(property));

	return property;
fail:
	kfree(property->values);
	kfree(property);
	return NULL;
}
EXPORT_SYMBOL(drm_property_create);

/**
 * drm_property_create_enum - create a new enumeration property type
 * @dev: drm device
 * @flags: flags specifying the property type
 * @name: name of the property
 * @props: enumeration lists with property values
 * @num_values: number of pre-defined values
 *
 * This creates a new generic drm property which can then be attached to a drm
 * object with drm_object_attach_property. The returned property object must be
 * freed with drm_property_destroy.
 *
 * Userspace is only allowed to set one of the predefined values for enumeration
 * properties.
 *
 * Returns:
 * A pointer to the newly created property on success, NULL on failure.
 */
struct drm_property *drm_property_create_enum(struct drm_device *dev, int flags,
					 const char *name,
					 const struct drm_prop_enum_list *props,
					 int num_values)
{
	struct drm_property *property;
	int i, ret;

	flags |= DRM_MODE_PROP_ENUM;

	property = drm_property_create(dev, flags, name, num_values);
	if (!property)
		return NULL;

	for (i = 0; i < num_values; i++) {
		ret = drm_property_add_enum(property, i,
				      props[i].type,
				      props[i].name);
		if (ret) {
			drm_property_destroy(dev, property);
			return NULL;
		}
	}

	return property;
}
EXPORT_SYMBOL(drm_property_create_enum);

/**
 * drm_property_create_bitmask - create a new bitmask property type
 * @dev: drm device
 * @flags: flags specifying the property type
 * @name: name of the property
 * @props: enumeration lists with property bitflags
 * @num_props: size of the @props array
 * @supported_bits: bitmask of all supported enumeration values
 *
 * This creates a new bitmask drm property which can then be attached to a drm
 * object with drm_object_attach_property. The returned property object must be
 * freed with drm_property_destroy.
 *
 * Compared to plain enumeration properties userspace is allowed to set any
 * or'ed together combination of the predefined property bitflag values
 *
 * Returns:
 * A pointer to the newly created property on success, NULL on failure.
 */
struct drm_property *drm_property_create_bitmask(struct drm_device *dev,
					 int flags, const char *name,
					 const struct drm_prop_enum_list *props,
					 int num_props,
					 uint64_t supported_bits)
{
	struct drm_property *property;
	int i, ret, index = 0;
	int num_values = hweight64(supported_bits);

	flags |= DRM_MODE_PROP_BITMASK;

	property = drm_property_create(dev, flags, name, num_values);
	if (!property)
		return NULL;
	for (i = 0; i < num_props; i++) {
		if (!(supported_bits & (1ULL << props[i].type)))
			continue;

		if (WARN_ON(index >= num_values)) {
			drm_property_destroy(dev, property);
			return NULL;
		}

		ret = drm_property_add_enum(property, index++,
				      props[i].type,
				      props[i].name);
		if (ret) {
			drm_property_destroy(dev, property);
			return NULL;
		}
	}

	return property;
}
EXPORT_SYMBOL(drm_property_create_bitmask);

static struct drm_property *property_create_range(struct drm_device *dev,
					 int flags, const char *name,
					 uint64_t min, uint64_t max)
{
	struct drm_property *property;

	property = drm_property_create(dev, flags, name, 2);
	if (!property)
		return NULL;

	property->values[0] = min;
	property->values[1] = max;

	return property;
}

/**
 * drm_property_create_range - create a new unsigned ranged property type
 * @dev: drm device
 * @flags: flags specifying the property type
 * @name: name of the property
 * @min: minimum value of the property
 * @max: maximum value of the property
 *
 * This creates a new generic drm property which can then be attached to a drm
 * object with drm_object_attach_property. The returned property object must be
 * freed with drm_property_destroy.
 *
 * Userspace is allowed to set any unsigned integer value in the (min, max)
 * range inclusive.
 *
 * Returns:
 * A pointer to the newly created property on success, NULL on failure.
 */
struct drm_property *drm_property_create_range(struct drm_device *dev, int flags,
					 const char *name,
					 uint64_t min, uint64_t max)
{
	return property_create_range(dev, DRM_MODE_PROP_RANGE | flags,
			name, min, max);
}
EXPORT_SYMBOL(drm_property_create_range);

/**
 * drm_property_create_signed_range - create a new signed ranged property type
 * @dev: drm device
 * @flags: flags specifying the property type
 * @name: name of the property
 * @min: minimum value of the property
 * @max: maximum value of the property
 *
 * This creates a new generic drm property which can then be attached to a drm
 * object with drm_object_attach_property. The returned property object must be
 * freed with drm_property_destroy.
 *
 * Userspace is allowed to set any signed integer value in the (min, max)
 * range inclusive.
 *
 * Returns:
 * A pointer to the newly created property on success, NULL on failure.
 */
struct drm_property *drm_property_create_signed_range(struct drm_device *dev,
					 int flags, const char *name,
					 int64_t min, int64_t max)
{
	return property_create_range(dev, DRM_MODE_PROP_SIGNED_RANGE | flags,
			name, I642U64(min), I642U64(max));
}
EXPORT_SYMBOL(drm_property_create_signed_range);

/**
 * drm_property_create_object - create a new object property type
 * @dev: drm device
 * @flags: flags specifying the property type
 * @name: name of the property
 * @type: object type from DRM_MODE_OBJECT_* defines
 *
 * This creates a new generic drm property which can then be attached to a drm
 * object with drm_object_attach_property. The returned property object must be
 * freed with drm_property_destroy.
 *
 * Userspace is only allowed to set this to any property value of the given
 * @type. Only useful for atomic properties, which is enforced.
 *
 * Returns:
 * A pointer to the newly created property on success, NULL on failure.
 */
struct drm_property *drm_property_create_object(struct drm_device *dev,
					 int flags, const char *name, uint32_t type)
{
	struct drm_property *property;

	flags |= DRM_MODE_PROP_OBJECT;

	if (WARN_ON(!(flags & DRM_MODE_PROP_ATOMIC)))
		return NULL;

	property = drm_property_create(dev, flags, name, 1);
	if (!property)
		return NULL;

	property->values[0] = type;

	return property;
}
EXPORT_SYMBOL(drm_property_create_object);

/**
 * drm_property_create_bool - create a new boolean property type
 * @dev: drm device
 * @flags: flags specifying the property type
 * @name: name of the property
 *
 * This creates a new generic drm property which can then be attached to a drm
 * object with drm_object_attach_property. The returned property object must be
 * freed with drm_property_destroy.
 *
 * This is implemented as a ranged property with only {0, 1} as valid values.
 *
 * Returns:
 * A pointer to the newly created property on success, NULL on failure.
 */
struct drm_property *drm_property_create_bool(struct drm_device *dev, int flags,
					 const char *name)
{
	return drm_property_create_range(dev, flags, name, 0, 1);
}
EXPORT_SYMBOL(drm_property_create_bool);

/**
 * drm_property_add_enum - add a possible value to an enumeration property
 * @property: enumeration property to change
 * @index: index of the new enumeration
 * @value: value of the new enumeration
 * @name: symbolic name of the new enumeration
 *
 * This functions adds enumerations to a property.
 *
 * It's use is deprecated, drivers should use one of the more specific helpers
 * to directly create the property with all enumerations already attached.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_property_add_enum(struct drm_property *property, int index,
			  uint64_t value, const char *name)
{
	struct drm_property_enum *prop_enum;

	if (!(drm_property_type_is(property, DRM_MODE_PROP_ENUM) ||
			drm_property_type_is(property, DRM_MODE_PROP_BITMASK)))
		return -EINVAL;

	/*
	 * Bitmask enum properties have the additional constraint of values
	 * from 0 to 63
	 */
	if (drm_property_type_is(property, DRM_MODE_PROP_BITMASK) &&
			(value > 63))
		return -EINVAL;

	if (!list_empty(&property->enum_list)) {
		list_for_each_entry(prop_enum, &property->enum_list, head) {
			if (prop_enum->value == value) {
				strncpy(prop_enum->name, name, DRM_PROP_NAME_LEN);
				prop_enum->name[DRM_PROP_NAME_LEN-1] = '\0';
				return 0;
			}
		}
	}

	prop_enum = kzalloc(sizeof(struct drm_property_enum), GFP_KERNEL);
	if (!prop_enum)
		return -ENOMEM;

	strncpy(prop_enum->name, name, DRM_PROP_NAME_LEN);
	prop_enum->name[DRM_PROP_NAME_LEN-1] = '\0';
	prop_enum->value = value;

	property->values[index] = value;
	list_add_tail(&prop_enum->head, &property->enum_list);
	return 0;
}
EXPORT_SYMBOL(drm_property_add_enum);

/**
 * drm_property_destroy - destroy a drm property
 * @dev: drm device
 * @property: property to destry
 *
 * This function frees a property including any attached resources like
 * enumeration values.
 */
void drm_property_destroy(struct drm_device *dev, struct drm_property *property)
{
	struct drm_property_enum *prop_enum, *pt;

	list_for_each_entry_safe(prop_enum, pt, &property->enum_list, head) {
		list_del(&prop_enum->head);
		kfree(prop_enum);
	}

	if (property->num_values)
		kfree(property->values);
	drm_mode_object_put(dev, &property->base);
	list_del(&property->head);
	kfree(property);
}
EXPORT_SYMBOL(drm_property_destroy);

/**
 * drm_object_attach_property - attach a property to a modeset object
 * @obj: drm modeset object
 * @property: property to attach
 * @init_val: initial value of the property
 *
 * This attaches the given property to the modeset object with the given initial
 * value. Currently this function cannot fail since the properties are stored in
 * a statically sized array.
 */
void drm_object_attach_property(struct drm_mode_object *obj,
				struct drm_property *property,
				uint64_t init_val)
{
	int count = obj->properties->count;

	if (count == DRM_OBJECT_MAX_PROPERTY) {
		WARN(1, "Failed to attach object property (type: 0x%x). Please "
			"increase DRM_OBJECT_MAX_PROPERTY by 1 for each time "
			"you see this message on the same object type.\n",
			obj->type);
		return;
	}

	obj->properties->properties[count] = property;
	obj->properties->values[count] = init_val;
	obj->properties->count++;
	if (property->flags & DRM_MODE_PROP_ATOMIC)
		obj->properties->atomic_count++;
}
EXPORT_SYMBOL(drm_object_attach_property);

/**
 * drm_object_property_set_value - set the value of a property
 * @obj: drm mode object to set property value for
 * @property: property to set
 * @val: value the property should be set to
 *
 * This functions sets a given property on a given object. This function only
 * changes the software state of the property, it does not call into the
 * driver's ->set_property callback.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_object_property_set_value(struct drm_mode_object *obj,
				  struct drm_property *property, uint64_t val)
{
	int i;

	for (i = 0; i < obj->properties->count; i++) {
		if (obj->properties->properties[i] == property) {
			obj->properties->values[i] = val;
			return 0;
		}
	}

	return -EINVAL;
}
EXPORT_SYMBOL(drm_object_property_set_value);

/**
 * drm_object_property_get_value - retrieve the value of a property
 * @obj: drm mode object to get property value from
 * @property: property to retrieve
 * @val: storage for the property value
 *
 * This function retrieves the softare state of the given property for the given
 * property. Since there is no driver callback to retrieve the current property
 * value this might be out of sync with the hardware, depending upon the driver
 * and property.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_object_property_get_value(struct drm_mode_object *obj,
				  struct drm_property *property, uint64_t *val)
{
	int i;

	/* read-only properties bypass atomic mechanism and still store
	 * their value in obj->properties->values[].. mostly to avoid
	 * having to deal w/ EDID and similar props in atomic paths:
	 */
	if (drm_core_check_feature(property->dev, DRIVER_ATOMIC) &&
			!(property->flags & DRM_MODE_PROP_IMMUTABLE))
		return drm_atomic_get_property(obj, property, val);

	for (i = 0; i < obj->properties->count; i++) {
		if (obj->properties->properties[i] == property) {
			*val = obj->properties->values[i];
			return 0;
		}
	}

	return -EINVAL;
}
EXPORT_SYMBOL(drm_object_property_get_value);

/**
 * drm_mode_getproperty_ioctl - get the property metadata
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * This function retrieves the metadata for a given property, like the different
 * possible values for an enum property or the limits for a range property.
 *
 * Blob properties are special
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_getproperty_ioctl(struct drm_device *dev,
			       void *data, struct drm_file *file_priv)
{
	struct drm_mode_get_property *out_resp = data;
	struct drm_property *property;
	int enum_count = 0;
	int value_count = 0;
	int ret = 0, i;
	int copied;
	struct drm_property_enum *prop_enum;
	struct drm_mode_property_enum __user *enum_ptr;
	uint64_t __user *values_ptr;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	drm_modeset_lock_all(dev);
	property = drm_property_find(dev, out_resp->prop_id);
	if (!property) {
		ret = -ENOENT;
		goto done;
	}

	if (drm_property_type_is(property, DRM_MODE_PROP_ENUM) ||
			drm_property_type_is(property, DRM_MODE_PROP_BITMASK)) {
		list_for_each_entry(prop_enum, &property->enum_list, head)
			enum_count++;
	}

	value_count = property->num_values;

	strncpy(out_resp->name, property->name, DRM_PROP_NAME_LEN);
	out_resp->name[DRM_PROP_NAME_LEN-1] = 0;
	out_resp->flags = property->flags;

	if ((out_resp->count_values >= value_count) && value_count) {
		values_ptr = (uint64_t __user *)(unsigned long)out_resp->values_ptr;
		for (i = 0; i < value_count; i++) {
			if (copy_to_user(values_ptr + i, &property->values[i], sizeof(uint64_t))) {
				ret = -EFAULT;
				goto done;
			}
		}
	}
	out_resp->count_values = value_count;

	if (drm_property_type_is(property, DRM_MODE_PROP_ENUM) ||
			drm_property_type_is(property, DRM_MODE_PROP_BITMASK)) {
		if ((out_resp->count_enum_blobs >= enum_count) && enum_count) {
			copied = 0;
			enum_ptr = (struct drm_mode_property_enum __user *)(unsigned long)out_resp->enum_blob_ptr;
			list_for_each_entry(prop_enum, &property->enum_list, head) {

				if (copy_to_user(&enum_ptr[copied].value, &prop_enum->value, sizeof(uint64_t))) {
					ret = -EFAULT;
					goto done;
				}

				if (copy_to_user(&enum_ptr[copied].name,
						 &prop_enum->name, DRM_PROP_NAME_LEN)) {
					ret = -EFAULT;
					goto done;
				}
				copied++;
			}
		}
		out_resp->count_enum_blobs = enum_count;
	}

	/*
	 * NOTE: The idea seems to have been to use this to read all the blob
	 * property values. But nothing ever added them to the corresponding
	 * list, userspace always used the special-purpose get_blob ioctl to
	 * read the value for a blob property. It also doesn't make a lot of
	 * sense to return values here when everything else is just metadata for
	 * the property itself.
	 */
	if (drm_property_type_is(property, DRM_MODE_PROP_BLOB))
		out_resp->count_enum_blobs = 0;
done:
	drm_modeset_unlock_all(dev);
	return ret;
}

/**
 * drm_property_create_blob - Create new blob property
 *
 * Creates a new blob property for a specified DRM device, optionally
 * copying data.
 *
 * @dev: DRM device to create property for
 * @length: Length to allocate for blob data
 * @data: If specified, copies data into blob
 *
 * Returns:
 * New blob property with a single reference on success, or an ERR_PTR
 * value on failure.
 */
struct drm_property_blob *
drm_property_create_blob(struct drm_device *dev, size_t length,
			 const void *data)
{
	struct drm_property_blob *blob;
	int ret;

	if (!length || length > ULONG_MAX - sizeof(struct drm_property_blob))
		return ERR_PTR(-EINVAL);

	blob = kzalloc(sizeof(struct drm_property_blob)+length, GFP_KERNEL);
	if (!blob)
		return ERR_PTR(-ENOMEM);

	/* This must be explicitly initialised, so we can safely call list_del
	 * on it in the removal handler, even if it isn't in a file list. */
	INIT_LIST_HEAD(&blob->head_file);
	blob->length = length;
	blob->dev = dev;

	if (data)
		memcpy(blob->data, data, length);

	mutex_lock(&dev->mode_config.blob_lock);

	ret = drm_mode_object_get(dev, &blob->base, DRM_MODE_OBJECT_BLOB);
	if (ret) {
		kfree(blob);
		mutex_unlock(&dev->mode_config.blob_lock);
		return ERR_PTR(-EINVAL);
	}

	kref_init(&blob->refcount);

	list_add_tail(&blob->head_global,
	              &dev->mode_config.property_blob_list);

	mutex_unlock(&dev->mode_config.blob_lock);

	return blob;
}
EXPORT_SYMBOL(drm_property_create_blob);

/**
 * drm_property_free_blob - Blob property destructor
 *
 * Internal free function for blob properties; must not be used directly.
 *
 * @kref: Reference
 */
static void drm_property_free_blob(struct kref *kref)
{
	struct drm_property_blob *blob =
		container_of(kref, struct drm_property_blob, refcount);

	WARN_ON(!mutex_is_locked(&blob->dev->mode_config.blob_lock));

	list_del(&blob->head_global);
	list_del(&blob->head_file);
	drm_mode_object_put(blob->dev, &blob->base);

	kfree(blob);
}

/**
 * drm_property_unreference_blob - Unreference a blob property
 *
 * Drop a reference on a blob property. May free the object.
 *
 * @blob: Pointer to blob property
 */
void drm_property_unreference_blob(struct drm_property_blob *blob)
{
	struct drm_device *dev;

	if (!blob)
		return;

	dev = blob->dev;

	DRM_DEBUG("%p: blob ID: %d (%d)\n", blob, blob->base.id, atomic_read(&blob->refcount.refcount));

	if (kref_put_mutex(&blob->refcount, drm_property_free_blob,
			   &dev->mode_config.blob_lock))
		mutex_unlock(&dev->mode_config.blob_lock);
	else
		might_lock(&dev->mode_config.blob_lock);
}
EXPORT_SYMBOL(drm_property_unreference_blob);

/**
 * drm_property_unreference_blob_locked - Unreference a blob property with blob_lock held
 *
 * Drop a reference on a blob property. May free the object. This must be
 * called with blob_lock held.
 *
 * @blob: Pointer to blob property
 */
static void drm_property_unreference_blob_locked(struct drm_property_blob *blob)
{
	if (!blob)
		return;

	DRM_DEBUG("%p: blob ID: %d (%d)\n", blob, blob->base.id, atomic_read(&blob->refcount.refcount));

	kref_put(&blob->refcount, drm_property_free_blob);
}

/**
 * drm_property_destroy_user_blobs - destroy all blobs created by this client
 * @dev:       DRM device
 * @file_priv: destroy all blobs owned by this file handle
 */
void drm_property_destroy_user_blobs(struct drm_device *dev,
				     struct drm_file *file_priv)
{
	struct drm_property_blob *blob, *bt;

	mutex_lock(&dev->mode_config.blob_lock);

	list_for_each_entry_safe(blob, bt, &file_priv->blobs, head_file) {
		list_del_init(&blob->head_file);
		drm_property_unreference_blob_locked(blob);
	}

	mutex_unlock(&dev->mode_config.blob_lock);
}

/**
 * drm_property_reference_blob - Take a reference on an existing property
 *
 * Take a new reference on an existing blob property.
 *
 * @blob: Pointer to blob property
 */
struct drm_property_blob *drm_property_reference_blob(struct drm_property_blob *blob)
{
	DRM_DEBUG("%p: blob ID: %d (%d)\n", blob, blob->base.id, atomic_read(&blob->refcount.refcount));
	kref_get(&blob->refcount);
	return blob;
}
EXPORT_SYMBOL(drm_property_reference_blob);

/*
 * Like drm_property_lookup_blob, but does not return an additional reference.
 * Must be called with blob_lock held.
 */
static struct drm_property_blob *__drm_property_lookup_blob(struct drm_device *dev,
							    uint32_t id)
{
	struct drm_mode_object *obj = NULL;
	struct drm_property_blob *blob;

	WARN_ON(!mutex_is_locked(&dev->mode_config.blob_lock));

	mutex_lock(&dev->mode_config.idr_mutex);
	obj = idr_find(&dev->mode_config.crtc_idr, id);
	if (!obj || (obj->type != DRM_MODE_OBJECT_BLOB) || (obj->id != id))
		blob = NULL;
	else
		blob = obj_to_blob(obj);
	mutex_unlock(&dev->mode_config.idr_mutex);

	return blob;
}

/**
 * drm_property_lookup_blob - look up a blob property and take a reference
 * @dev: drm device
 * @id: id of the blob property
 *
 * If successful, this takes an additional reference to the blob property.
 * callers need to make sure to eventually unreference the returned property
 * again, using @drm_property_unreference_blob.
 */
struct drm_property_blob *drm_property_lookup_blob(struct drm_device *dev,
					           uint32_t id)
{
	struct drm_property_blob *blob;

	mutex_lock(&dev->mode_config.blob_lock);
	blob = __drm_property_lookup_blob(dev, id);
	if (blob) {
		if (!kref_get_unless_zero(&blob->refcount))
			blob = NULL;
	}
	mutex_unlock(&dev->mode_config.blob_lock);

	return blob;
}
EXPORT_SYMBOL(drm_property_lookup_blob);

/**
 * drm_property_replace_global_blob - atomically replace existing blob property
 * @dev: drm device
 * @replace: location of blob property pointer to be replaced
 * @length: length of data for new blob, or 0 for no data
 * @data: content for new blob, or NULL for no data
 * @obj_holds_id: optional object for property holding blob ID
 * @prop_holds_id: optional property holding blob ID
 * @return 0 on success or error on failure
 *
 * This function will atomically replace a global property in the blob list,
 * optionally updating a property which holds the ID of that property. It is
 * guaranteed to be atomic: no caller will be allowed to see intermediate
 * results, and either the entire operation will succeed and clean up the
 * previous property, or it will fail and the state will be unchanged.
 *
 * If length is 0 or data is NULL, no new blob will be created, and the holding
 * property, if specified, will be set to 0.
 *
 * Access to the replace pointer is assumed to be protected by the caller, e.g.
 * by holding the relevant modesetting object lock for its parent.
 *
 * For example, a drm_connector has a 'PATH' property, which contains the ID
 * of a blob property with the value of the MST path information. Calling this
 * function with replace pointing to the connector's path_blob_ptr, length and
 * data set for the new path information, obj_holds_id set to the connector's
 * base object, and prop_holds_id set to the path property name, will perform
 * a completely atomic update. The access to path_blob_ptr is protected by the
 * caller holding a lock on the connector.
 */
static int drm_property_replace_global_blob(struct drm_device *dev,
                                            struct drm_property_blob **replace,
                                            size_t length,
                                            const void *data,
                                            struct drm_mode_object *obj_holds_id,
                                            struct drm_property *prop_holds_id)
{
	struct drm_property_blob *new_blob = NULL;
	struct drm_property_blob *old_blob = NULL;
	int ret;

	WARN_ON(replace == NULL);

	old_blob = *replace;

	if (length && data) {
		new_blob = drm_property_create_blob(dev, length, data);
		if (IS_ERR(new_blob))
			return PTR_ERR(new_blob);
	}

	/* This does not need to be synchronised with blob_lock, as the
	 * get_properties ioctl locks all modesetting objects, and
	 * obj_holds_id must be locked before calling here, so we cannot
	 * have its value out of sync with the list membership modified
	 * below under blob_lock. */
	if (obj_holds_id) {
		ret = drm_object_property_set_value(obj_holds_id,
						    prop_holds_id,
						    new_blob ?
						        new_blob->base.id : 0);
		if (ret != 0)
			goto err_created;
	}

	drm_property_unreference_blob(old_blob);
	*replace = new_blob;

	return 0;

err_created:
	drm_property_unreference_blob(new_blob);
	return ret;
}

/**
 * drm_mode_getblob_ioctl - get the contents of a blob property value
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * This function retrieves the contents of a blob property. The value stored in
 * an object's blob property is just a normal modeset object id.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_getblob_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv)
{
	struct drm_mode_get_blob *out_resp = data;
	struct drm_property_blob *blob;
	int ret = 0;
	void __user *blob_ptr;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	drm_modeset_lock_all(dev);
	mutex_lock(&dev->mode_config.blob_lock);
	blob = __drm_property_lookup_blob(dev, out_resp->blob_id);
	if (!blob) {
		ret = -ENOENT;
		goto done;
	}

	if (out_resp->length == blob->length) {
		blob_ptr = (void __user *)(unsigned long)out_resp->data;
		if (copy_to_user(blob_ptr, blob->data, blob->length)) {
			ret = -EFAULT;
			goto done;
		}
	}
	out_resp->length = blob->length;

done:
	mutex_unlock(&dev->mode_config.blob_lock);
	drm_modeset_unlock_all(dev);
	return ret;
}

/**
 * drm_mode_createblob_ioctl - create a new blob property
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * This function creates a new blob property with user-defined values. In order
 * to give us sensible validation and checking when creating, rather than at
 * every potential use, we also require a type to be provided upfront.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_createblob_ioctl(struct drm_device *dev,
			      void *data, struct drm_file *file_priv)
{
	struct drm_mode_create_blob *out_resp = data;
	struct drm_property_blob *blob;
	void __user *blob_ptr;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	blob = drm_property_create_blob(dev, out_resp->length, NULL);
	if (IS_ERR(blob))
		return PTR_ERR(blob);

	blob_ptr = (void __user *)(unsigned long)out_resp->data;
	if (copy_from_user(blob->data, blob_ptr, out_resp->length)) {
		ret = -EFAULT;
		goto out_blob;
	}

	/* Dropping the lock between create_blob and our access here is safe
	 * as only the same file_priv can remove the blob; at this point, it is
	 * not associated with any file_priv. */
	mutex_lock(&dev->mode_config.blob_lock);
	out_resp->blob_id = blob->base.id;
	list_add_tail(&blob->head_file, &file_priv->blobs);
	mutex_unlock(&dev->mode_config.blob_lock);

	return 0;

out_blob:
	drm_property_unreference_blob(blob);
	return ret;
}

/**
 * drm_mode_destroyblob_ioctl - destroy a user blob property
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * Destroy an existing user-defined blob property.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_destroyblob_ioctl(struct drm_device *dev,
			       void *data, struct drm_file *file_priv)
{
	struct drm_mode_destroy_blob *out_resp = data;
	struct drm_property_blob *blob = NULL, *bt;
	bool found = false;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	mutex_lock(&dev->mode_config.blob_lock);
	blob = __drm_property_lookup_blob(dev, out_resp->blob_id);
	if (!blob) {
		ret = -ENOENT;
		goto err;
	}

	/* Ensure the property was actually created by this user. */
	list_for_each_entry(bt, &file_priv->blobs, head_file) {
		if (bt == blob) {
			found = true;
			break;
		}
	}

	if (!found) {
		ret = -EPERM;
		goto err;
	}

	/* We must drop head_file here, because we may not be the last
	 * reference on the blob. */
	list_del_init(&blob->head_file);
	drm_property_unreference_blob_locked(blob);
	mutex_unlock(&dev->mode_config.blob_lock);

	return 0;

err:
	mutex_unlock(&dev->mode_config.blob_lock);
	return ret;
}

/**
 * drm_mode_connector_set_path_property - set tile property on connector
 * @connector: connector to set property on.
 * @path: path to use for property; must not be NULL.
 *
 * This creates a property to expose to userspace to specify a
 * connector path. This is mainly used for DisplayPort MST where
 * connectors have a topology and we want to allow userspace to give
 * them more meaningful names.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_connector_set_path_property(struct drm_connector *connector,
					 const char *path)
{
	struct drm_device *dev = connector->dev;
	int ret;

	ret = drm_property_replace_global_blob(dev,
	                                       &connector->path_blob_ptr,
	                                       strlen(path) + 1,
	                                       path,
	                                       &connector->base,
	                                       dev->mode_config.path_property);
	return ret;
}
EXPORT_SYMBOL(drm_mode_connector_set_path_property);

/**
 * drm_mode_connector_set_tile_property - set tile property on connector
 * @connector: connector to set property on.
 *
 * This looks up the tile information for a connector, and creates a
 * property for userspace to parse if it exists. The property is of
 * the form of 8 integers using ':' as a separator.
 *
 * Returns:
 * Zero on success, errno on failure.
 */
int drm_mode_connector_set_tile_property(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	char tile[256];
	int ret;

	if (!connector->has_tile) {
		ret  = drm_property_replace_global_blob(dev,
		                                        &connector->tile_blob_ptr,
		                                        0,
		                                        NULL,
		                                        &connector->base,
		                                        dev->mode_config.tile_property);
		return ret;
	}

	snprintf(tile, 256, "%d:%d:%d:%d:%d:%d:%d:%d",
		 connector->tile_group->id, connector->tile_is_single_monitor,
		 connector->num_h_tile, connector->num_v_tile,
		 connector->tile_h_loc, connector->tile_v_loc,
		 connector->tile_h_size, connector->tile_v_size);

	ret = drm_property_replace_global_blob(dev,
	                                       &connector->tile_blob_ptr,
	                                       strlen(tile) + 1,
	                                       tile,
	                                       &connector->base,
	                                       dev->mode_config.tile_property);
	return ret;
}
EXPORT_SYMBOL(drm_mode_connector_set_tile_property);

/**
 * drm_mode_connector_update_edid_property - update the edid property of a connector
 * @connector: drm connector
 * @edid: new value of the edid property
 *
 * This function creates a new blob modeset object and assigns its id to the
 * connector's edid property.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_connector_update_edid_property(struct drm_connector *connector,
					    const struct edid *edid)
{
	struct drm_device *dev = connector->dev;
	size_t size = 0;
	int ret;

	/* ignore requests to set edid when overridden */
	if (connector->override_edid)
		return 0;

	if (edid)
		size = EDID_LENGTH * (1 + edid->extensions);

	ret = drm_property_replace_global_blob(dev,
					       &connector->edid_blob_ptr,
	                                       size,
	                                       edid,
	                                       &connector->base,
	                                       dev->mode_config.edid_property);
	return ret;
}
EXPORT_SYMBOL(drm_mode_connector_update_edid_property);

/* Some properties could refer to dynamic refcnt'd objects, or things that
 * need special locking to handle lifetime issues (ie. to ensure the prop
 * value doesn't become invalid part way through the property update due to
 * race).  The value returned by reference via 'obj' should be passed back
 * to drm_property_change_valid_put() after the property is set (and the
 * object to which the property is attached has a chance to take it's own
 * reference).
 */
bool drm_property_change_valid_get(struct drm_property *property,
					 uint64_t value, struct drm_mode_object **ref)
{
	int i;

	if (property->flags & DRM_MODE_PROP_IMMUTABLE)
		return false;

	*ref = NULL;

	if (drm_property_type_is(property, DRM_MODE_PROP_RANGE)) {
		if (value < property->values[0] || value > property->values[1])
			return false;
		return true;
	} else if (drm_property_type_is(property, DRM_MODE_PROP_SIGNED_RANGE)) {
		int64_t svalue = U642I64(value);

		if (svalue < U642I64(property->values[0]) ||
				svalue > U642I64(property->values[1]))
			return false;
		return true;
	} else if (drm_property_type_is(property, DRM_MODE_PROP_BITMASK)) {
		uint64_t valid_mask = 0;

		for (i = 0; i < property->num_values; i++)
			valid_mask |= (1ULL << property->values[i]);
		return !(value & ~valid_mask);
	} else if (drm_property_type_is(property, DRM_MODE_PROP_BLOB)) {
		struct drm_property_blob *blob;

		if (value == 0)
			return true;

		blob = drm_property_lookup_blob(property->dev, value);
		if (blob) {
			*ref = &blob->base;
			return true;
		} else {
			return false;
		}
	} else if (drm_property_type_is(property, DRM_MODE_PROP_OBJECT)) {
		/* a zero value for an object property translates to null: */
		if (value == 0)
			return true;

		/* handle refcnt'd objects specially: */
		if (property->values[0] == DRM_MODE_OBJECT_FB) {
			struct drm_framebuffer *fb;
			fb = drm_framebuffer_lookup(property->dev, value);
			if (fb) {
				*ref = &fb->base;
				return true;
			} else {
				return false;
			}
		} else {
			return _object_find(property->dev, value, property->values[0]) != NULL;
		}
	}

	for (i = 0; i < property->num_values; i++)
		if (property->values[i] == value)
			return true;
	return false;
}

void drm_property_change_valid_put(struct drm_property *property,
		struct drm_mode_object *ref)
{
	if (!ref)
		return;

	if (drm_property_type_is(property, DRM_MODE_PROP_OBJECT)) {
		if (property->values[0] == DRM_MODE_OBJECT_FB)
			drm_framebuffer_unreference(obj_to_fb(ref));
	} else if (drm_property_type_is(property, DRM_MODE_PROP_BLOB))
		drm_property_unreference_blob(obj_to_blob(ref));
}

/**
 * drm_mode_connector_property_set_ioctl - set the current value of a connector property
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * This function sets the current value for a connectors's property. It also
 * calls into a driver's ->set_property callback to update the hardware state
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_connector_property_set_ioctl(struct drm_device *dev,
				       void *data, struct drm_file *file_priv)
{
	struct drm_mode_connector_set_property *conn_set_prop = data;
	struct drm_mode_obj_set_property obj_set_prop = {
		.value = conn_set_prop->value,
		.prop_id = conn_set_prop->prop_id,
		.obj_id = conn_set_prop->connector_id,
		.obj_type = DRM_MODE_OBJECT_CONNECTOR
	};

	/* It does all the locking and checking we need */
	return drm_mode_obj_set_property_ioctl(dev, &obj_set_prop, file_priv);
}

static int drm_mode_connector_set_obj_prop(struct drm_mode_object *obj,
					   struct drm_property *property,
					   uint64_t value)
{
	int ret = -EINVAL;
	struct drm_connector *connector = obj_to_connector(obj);

	/* Do DPMS ourselves */
	if (property == connector->dev->mode_config.dpms_property) {
		ret = 0;
		if (connector->funcs->dpms)
			ret = (*connector->funcs->dpms)(connector, (int)value);
	} else if (connector->funcs->set_property)
		ret = connector->funcs->set_property(connector, property, value);

	/* store the property value if successful */
	if (!ret)
		drm_object_property_set_value(&connector->base, property, value);
	return ret;
}

static int drm_mode_crtc_set_obj_prop(struct drm_mode_object *obj,
				      struct drm_property *property,
				      uint64_t value)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
{
	int ret = -EINVAL;
	struct drm_crtc *crtc = obj_to_crtc(obj);

	if (crtc->funcs->set_property)
		ret = crtc->funcs->set_property(crtc, property, value);
	if (!ret)
		drm_object_property_set_value(obj, property, value);

	return ret;
}
<<<<<<< HEAD
=======

/**
 * drm_mode_plane_set_obj_prop - set the value of a property
 * @plane: drm plane object to set property value for
 * @property: property to set
 * @value: value the property should be set to
 *
 * This functions sets a given property on a given plane object. This function
 * calls the driver's ->set_property callback and changes the software state of
 * the property if the callback succeeds.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_mode_plane_set_obj_prop(struct drm_plane *plane,
				struct drm_property *property,
				uint64_t value)
{
	int ret = -EINVAL;
	struct drm_mode_object *obj = &plane->base;

	if (plane->funcs->set_property)
		ret = plane->funcs->set_property(plane, property, value);
	if (!ret)
		drm_object_property_set_value(obj, property, value);

	return ret;
}
EXPORT_SYMBOL(drm_mode_plane_set_obj_prop);

/**
 * drm_mode_obj_get_properties_ioctl - get the current value of a object's property
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * This function retrieves the current value for an object's property. Compared
 * to the connector specific ioctl this one is extended to also work on crtc and
 * plane objects.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_obj_get_properties_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv)
{
	struct drm_mode_obj_get_properties *arg = data;
	struct drm_mode_object *obj;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	drm_modeset_lock_all(dev);

	obj = drm_mode_object_find(dev, arg->obj_id, arg->obj_type);
	if (!obj) {
		ret = -ENOENT;
		goto out;
	}
	if (!obj->properties) {
		ret = -EINVAL;
		goto out;
	}

	ret = get_properties(obj, file_priv->atomic,
			(uint32_t __user *)(unsigned long)(arg->props_ptr),
			(uint64_t __user *)(unsigned long)(arg->prop_values_ptr),
			&arg->count_props);

out:
	drm_modeset_unlock_all(dev);
	return ret;
}

/**
 * drm_mode_obj_set_property_ioctl - set the current value of an object's property
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * This function sets the current value for an object's property. It also calls
 * into a driver's ->set_property callback to update the hardware state.
 * Compared to the connector specific ioctl this one is extended to also work on
 * crtc and plane objects.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_obj_set_property_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv)
{
	struct drm_mode_obj_set_property *arg = data;
	struct drm_mode_object *arg_obj;
	struct drm_mode_object *prop_obj;
	struct drm_property *property;
	int i, ret = -EINVAL;
	struct drm_mode_object *ref;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	drm_modeset_lock_all(dev);

	arg_obj = drm_mode_object_find(dev, arg->obj_id, arg->obj_type);
	if (!arg_obj) {
		ret = -ENOENT;
		goto out;
	}
	if (!arg_obj->properties)
		goto out;

	for (i = 0; i < arg_obj->properties->count; i++)
		if (arg_obj->properties->properties[i]->base.id == arg->prop_id)
			break;

	if (i == arg_obj->properties->count)
		goto out;

	prop_obj = drm_mode_object_find(dev, arg->prop_id,
					DRM_MODE_OBJECT_PROPERTY);
	if (!prop_obj) {
		ret = -ENOENT;
		goto out;
	}
	property = obj_to_property(prop_obj);

	if (!drm_property_change_valid_get(property, arg->value, &ref))
		goto out;

	switch (arg_obj->type) {
	case DRM_MODE_OBJECT_CONNECTOR:
		ret = drm_mode_connector_set_obj_prop(arg_obj, property,
						      arg->value);
		break;
	case DRM_MODE_OBJECT_CRTC:
		ret = drm_mode_crtc_set_obj_prop(arg_obj, property, arg->value);
		break;
	case DRM_MODE_OBJECT_PLANE:
		ret = drm_mode_plane_set_obj_prop(obj_to_plane(arg_obj),
						  property, arg->value);
		break;
	}

	drm_property_change_valid_put(property, ref);

out:
	drm_modeset_unlock_all(dev);
	return ret;
}

/**
 * drm_mode_connector_attach_encoder - attach a connector to an encoder
 * @connector: connector to attach
 * @encoder: encoder to attach @connector to
 *
 * This function links up a connector to an encoder. Note that the routing
 * restrictions between encoders and crtcs are exposed to userspace through the
 * possible_clones and possible_crtcs bitmasks.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_connector_attach_encoder(struct drm_connector *connector,
				      struct drm_encoder *encoder)
{
	int i;

	for (i = 0; i < DRM_CONNECTOR_MAX_ENCODER; i++) {
		if (connector->encoder_ids[i] == 0) {
			connector->encoder_ids[i] = encoder->base.id;
			return 0;
		}
	}
	return -ENOMEM;
}
EXPORT_SYMBOL(drm_mode_connector_attach_encoder);

/**
 * drm_mode_crtc_set_gamma_size - set the gamma table size
 * @crtc: CRTC to set the gamma table size for
 * @gamma_size: size of the gamma table
 *
 * Drivers which support gamma tables should set this to the supported gamma
 * table size when initializing the CRTC. Currently the drm core only supports a
 * fixed gamma table size.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_crtc_set_gamma_size(struct drm_crtc *crtc,
				 int gamma_size)
{
	crtc->gamma_size = gamma_size;

	crtc->gamma_store = kcalloc(gamma_size, sizeof(uint16_t) * 3,
				    GFP_KERNEL);
	if (!crtc->gamma_store) {
		crtc->gamma_size = 0;
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(drm_mode_crtc_set_gamma_size);

/**
 * drm_mode_gamma_set_ioctl - set the gamma table
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * Set the gamma table of a CRTC to the one passed in by the user. Userspace can
 * inquire the required gamma table size through drm_mode_gamma_get_ioctl.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_gamma_set_ioctl(struct drm_device *dev,
			     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc_lut *crtc_lut = data;
	struct drm_crtc *crtc;
	void *r_base, *g_base, *b_base;
	int size;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	drm_modeset_lock_all(dev);
	crtc = drm_crtc_find(dev, crtc_lut->crtc_id);
	if (!crtc) {
		ret = -ENOENT;
		goto out;
	}

	if (crtc->funcs->gamma_set == NULL) {
		ret = -ENOSYS;
		goto out;
	}

	/* memcpy into gamma store */
	if (crtc_lut->gamma_size != crtc->gamma_size) {
		ret = -EINVAL;
		goto out;
	}

	size = crtc_lut->gamma_size * (sizeof(uint16_t));
	r_base = crtc->gamma_store;
	if (copy_from_user(r_base, (void __user *)(unsigned long)crtc_lut->red, size)) {
		ret = -EFAULT;
		goto out;
	}

	g_base = r_base + size;
	if (copy_from_user(g_base, (void __user *)(unsigned long)crtc_lut->green, size)) {
		ret = -EFAULT;
		goto out;
	}

	b_base = g_base + size;
	if (copy_from_user(b_base, (void __user *)(unsigned long)crtc_lut->blue, size)) {
		ret = -EFAULT;
		goto out;
	}

	crtc->funcs->gamma_set(crtc, r_base, g_base, b_base, 0, crtc->gamma_size);

out:
	drm_modeset_unlock_all(dev);
	return ret;

}

/**
 * drm_mode_gamma_get_ioctl - get the gamma table
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * Copy the current gamma table into the storage provided. This also provides
 * the gamma table size the driver expects, which can be used to size the
 * allocated storage.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_gamma_get_ioctl(struct drm_device *dev,
			     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc_lut *crtc_lut = data;
	struct drm_crtc *crtc;
	void *r_base, *g_base, *b_base;
	int size;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	drm_modeset_lock_all(dev);
	crtc = drm_crtc_find(dev, crtc_lut->crtc_id);
	if (!crtc) {
		ret = -ENOENT;
		goto out;
	}

	/* memcpy into gamma store */
	if (crtc_lut->gamma_size != crtc->gamma_size) {
		ret = -EINVAL;
		goto out;
	}

	size = crtc_lut->gamma_size * (sizeof(uint16_t));
	r_base = crtc->gamma_store;
	if (copy_to_user((void __user *)(unsigned long)crtc_lut->red, r_base, size)) {
		ret = -EFAULT;
		goto out;
	}

	g_base = r_base + size;
	if (copy_to_user((void __user *)(unsigned long)crtc_lut->green, g_base, size)) {
		ret = -EFAULT;
		goto out;
	}

	b_base = g_base + size;
	if (copy_to_user((void __user *)(unsigned long)crtc_lut->blue, b_base, size)) {
		ret = -EFAULT;
		goto out;
	}
out:
	drm_modeset_unlock_all(dev);
	return ret;
}

/**
 * drm_mode_page_flip_ioctl - schedule an asynchronous fb update
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * This schedules an asynchronous update on a given CRTC, called page flip.
 * Optionally a drm event is generated to signal the completion of the event.
 * Generic drivers cannot assume that a pageflip with changed framebuffer
 * properties (including driver specific metadata like tiling layout) will work,
 * but some drivers support e.g. pixel format changes through the pageflip
 * ioctl.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_page_flip_ioctl(struct drm_device *dev,
			     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc_page_flip *page_flip = data;
	struct drm_crtc *crtc;
	struct drm_framebuffer *fb = NULL;
	struct drm_pending_vblank_event *e = NULL;
	unsigned long flags;
	int ret = -EINVAL;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	if (page_flip->flags & ~DRM_MODE_PAGE_FLIP_FLAGS ||
	    page_flip->reserved != 0)
		return -EINVAL;

	if ((page_flip->flags & DRM_MODE_PAGE_FLIP_ASYNC) && !dev->mode_config.async_page_flip)
		return -EINVAL;

	crtc = drm_crtc_find(dev, page_flip->crtc_id);
	if (!crtc)
		return -ENOENT;

	drm_modeset_lock_crtc(crtc, crtc->primary);
	if (crtc->primary->fb == NULL) {
		/* The framebuffer is currently unbound, presumably
		 * due to a hotplug event, that userspace has not
		 * yet discovered.
		 */
		ret = -EBUSY;
		goto out;
	}

	if (crtc->funcs->page_flip == NULL)
		goto out;

	fb = drm_framebuffer_lookup(dev, page_flip->fb_id);
	if (!fb) {
		ret = -ENOENT;
		goto out;
	}

	if (crtc->state) {
		const struct drm_plane_state *state = crtc->primary->state;

		ret = check_src_coords(state->src_x, state->src_y,
				       state->src_w, state->src_h, fb);
	} else {
		ret = drm_crtc_check_viewport(crtc, crtc->x, crtc->y, &crtc->mode, fb);
	}
	if (ret)
		goto out;

	if (crtc->primary->fb->pixel_format != fb->pixel_format) {
		DRM_DEBUG_KMS("Page flip is not allowed to change frame buffer format.\n");
		ret = -EINVAL;
		goto out;
	}

	if (page_flip->flags & DRM_MODE_PAGE_FLIP_EVENT) {
		ret = -ENOMEM;
		spin_lock_irqsave(&dev->event_lock, flags);
		if (file_priv->event_space < sizeof(e->event)) {
			spin_unlock_irqrestore(&dev->event_lock, flags);
			goto out;
		}
		file_priv->event_space -= sizeof(e->event);
		spin_unlock_irqrestore(&dev->event_lock, flags);

		e = kzalloc(sizeof(*e), GFP_KERNEL);
		if (e == NULL) {
			spin_lock_irqsave(&dev->event_lock, flags);
			file_priv->event_space += sizeof(e->event);
			spin_unlock_irqrestore(&dev->event_lock, flags);
			goto out;
		}

		e->event.base.type = DRM_EVENT_FLIP_COMPLETE;
		e->event.base.length = sizeof(e->event);
		e->event.user_data = page_flip->user_data;
		e->base.event = &e->event.base;
		e->base.file_priv = file_priv;
		e->base.destroy =
			(void (*) (struct drm_pending_event *)) kfree;
	}

	crtc->primary->old_fb = crtc->primary->fb;
	ret = crtc->funcs->page_flip(crtc, fb, e, page_flip->flags);
	if (ret) {
		if (page_flip->flags & DRM_MODE_PAGE_FLIP_EVENT) {
			spin_lock_irqsave(&dev->event_lock, flags);
			file_priv->event_space += sizeof(e->event);
			spin_unlock_irqrestore(&dev->event_lock, flags);
			kfree(e);
		}
		/* Keep the old fb, don't unref it. */
		crtc->primary->old_fb = NULL;
	} else {
		crtc->primary->fb = fb;
		/* Unref only the old framebuffer. */
		fb = NULL;
	}

out:
	if (fb)
		drm_framebuffer_unreference(fb);
	if (crtc->primary->old_fb)
		drm_framebuffer_unreference(crtc->primary->old_fb);
	crtc->primary->old_fb = NULL;
	drm_modeset_unlock_crtc(crtc);

	return ret;
}

/**
 * drm_mode_config_reset - call ->reset callbacks
 * @dev: drm device
 *
 * This functions calls all the crtc's, encoder's and connector's ->reset
 * callback. Drivers can use this in e.g. their driver load or resume code to
 * reset hardware and software state.
 */
void drm_mode_config_reset(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	struct drm_plane *plane;
	struct drm_encoder *encoder;
	struct drm_connector *connector;

	drm_for_each_plane(plane, dev)
		if (plane->funcs->reset)
			plane->funcs->reset(plane);

	drm_for_each_crtc(crtc, dev)
		if (crtc->funcs->reset)
			crtc->funcs->reset(crtc);

	drm_for_each_encoder(encoder, dev)
		if (encoder->funcs->reset)
			encoder->funcs->reset(encoder);

	mutex_lock(&dev->mode_config.mutex);
	drm_for_each_connector(connector, dev)
		if (connector->funcs->reset)
			connector->funcs->reset(connector);
	mutex_unlock(&dev->mode_config.mutex);
}
EXPORT_SYMBOL(drm_mode_config_reset);

/**
 * drm_mode_create_dumb_ioctl - create a dumb backing storage buffer
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * This creates a new dumb buffer in the driver's backing storage manager (GEM,
 * TTM or something else entirely) and returns the resulting buffer handle. This
 * handle can then be wrapped up into a framebuffer modeset object.
 *
 * Note that userspace is not allowed to use such objects for render
 * acceleration - drivers must create their own private ioctls for such a use
 * case.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_create_dumb_ioctl(struct drm_device *dev,
			       void *data, struct drm_file *file_priv)
{
	struct drm_mode_create_dumb *args = data;
	u32 cpp, stride, size;

	if (!dev->driver->dumb_create)
		return -ENOSYS;
	if (!args->width || !args->height || !args->bpp)
		return -EINVAL;

	/* overflow checks for 32bit size calculations */
	/* NOTE: DIV_ROUND_UP() can overflow */
	cpp = DIV_ROUND_UP(args->bpp, 8);
	if (!cpp || cpp > 0xffffffffU / args->width)
		return -EINVAL;
	stride = cpp * args->width;
	if (args->height > 0xffffffffU / stride)
		return -EINVAL;

	/* test for wrap-around */
	size = args->height * stride;
	if (PAGE_ALIGN(size) == 0)
		return -EINVAL;

	/*
	 * handle, pitch and size are output parameters. Zero them out to
	 * prevent drivers from accidentally using uninitialized data. Since
	 * not all existing userspace is clearing these fields properly we
	 * cannot reject IOCTL with garbage in them.
	 */
	args->handle = 0;
	args->pitch = 0;
	args->size = 0;

	return dev->driver->dumb_create(file_priv, dev, args);
}

/**
 * drm_mode_mmap_dumb_ioctl - create an mmap offset for a dumb backing storage buffer
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * Allocate an offset in the drm device node's address space to be able to
 * memory map a dumb buffer.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_mmap_dumb_ioctl(struct drm_device *dev,
			     void *data, struct drm_file *file_priv)
{
	struct drm_mode_map_dumb *args = data;

	/* call driver ioctl to get mmap offset */
	if (!dev->driver->dumb_map_offset)
		return -ENOSYS;

	return dev->driver->dumb_map_offset(file_priv, dev, args->handle, &args->offset);
}

/**
 * drm_mode_destroy_dumb_ioctl - destroy a dumb backing strage buffer
 * @dev: DRM device
 * @data: ioctl data
 * @file_priv: DRM file info
 *
 * This destroys the userspace handle for the given dumb backing storage buffer.
 * Since buffer objects must be reference counted in the kernel a buffer object
 * won't be immediately freed if a framebuffer modeset object still uses it.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_destroy_dumb_ioctl(struct drm_device *dev,
				void *data, struct drm_file *file_priv)
{
	struct drm_mode_destroy_dumb *args = data;

	if (!dev->driver->dumb_destroy)
		return -ENOSYS;

	return dev->driver->dumb_destroy(file_priv, dev, args->handle);
}

/**
 * drm_fb_get_bpp_depth - get the bpp/depth values for format
 * @format: pixel format (DRM_FORMAT_*)
 * @depth: storage for the depth value
 * @bpp: storage for the bpp value
 *
 * This only supports RGB formats here for compat with code that doesn't use
 * pixel formats directly yet.
 */
void drm_fb_get_bpp_depth(uint32_t format, unsigned int *depth,
			  int *bpp)
{
	switch (format) {
	case DRM_FORMAT_C8:
	case DRM_FORMAT_RGB332:
	case DRM_FORMAT_BGR233:
		*depth = 8;
		*bpp = 8;
		break;
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_BGRA5551:
		*depth = 15;
		*bpp = 16;
		break;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		*depth = 16;
		*bpp = 16;
		break;
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		*depth = 24;
		*bpp = 24;
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
		*depth = 24;
		*bpp = 32;
		break;
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
		*depth = 30;
		*bpp = 32;
		break;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
		*depth = 32;
		*bpp = 32;
		break;
	default:
		DRM_DEBUG_KMS("unsupported pixel format %s\n",
			      drm_get_format_name(format));
		*depth = 0;
		*bpp = 0;
		break;
	}
}
EXPORT_SYMBOL(drm_fb_get_bpp_depth);

/**
 * drm_format_num_planes - get the number of planes for format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * Returns:
 * The number of planes used by the specified pixel format.
 */
int drm_format_num_planes(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YVU410:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YVU411:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		return 3;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV24:
	case DRM_FORMAT_NV42:
		return 2;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(drm_format_num_planes);

/**
 * drm_format_plane_cpp - determine the bytes per pixel value
 * @format: pixel format (DRM_FORMAT_*)
 * @plane: plane index
 *
 * Returns:
 * The bytes per pixel value for the specified plane.
 */
int drm_format_plane_cpp(uint32_t format, int plane)
{
	unsigned int depth;
	int bpp;

	if (plane >= drm_format_num_planes(format))
		return 0;

	switch (format) {
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		return 2;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV24:
	case DRM_FORMAT_NV42:
		return plane ? 2 : 1;
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YVU410:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YVU411:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		return 1;
	default:
		drm_fb_get_bpp_depth(format, &depth, &bpp);
		return bpp >> 3;
	}
}
EXPORT_SYMBOL(drm_format_plane_cpp);

/**
 * drm_format_horz_chroma_subsampling - get the horizontal chroma subsampling factor
 * @format: pixel format (DRM_FORMAT_*)
 *
 * Returns:
 * The horizontal chroma subsampling factor for the
 * specified pixel format.
 */
int drm_format_horz_chroma_subsampling(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YVU411:
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YVU410:
		return 4;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		return 2;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(drm_format_horz_chroma_subsampling);

/**
 * drm_format_vert_chroma_subsampling - get the vertical chroma subsampling factor
 * @format: pixel format (DRM_FORMAT_*)
 *
 * Returns:
 * The vertical chroma subsampling factor for the
 * specified pixel format.
 */
int drm_format_vert_chroma_subsampling(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YVU410:
		return 4;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		return 2;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(drm_format_vert_chroma_subsampling);

/**
 * drm_rotation_simplify() - Try to simplify the rotation
 * @rotation: Rotation to be simplified
 * @supported_rotations: Supported rotations
 *
 * Attempt to simplify the rotation to a form that is supported.
 * Eg. if the hardware supports everything except DRM_REFLECT_X
 * one could call this function like this:
 *
 * drm_rotation_simplify(rotation, BIT(DRM_ROTATE_0) |
 *                       BIT(DRM_ROTATE_90) | BIT(DRM_ROTATE_180) |
 *                       BIT(DRM_ROTATE_270) | BIT(DRM_REFLECT_Y));
 *
 * to eliminate the DRM_ROTATE_X flag. Depending on what kind of
 * transforms the hardware supports, this function may not
 * be able to produce a supported transform, so the caller should
 * check the result afterwards.
 */
unsigned int drm_rotation_simplify(unsigned int rotation,
				   unsigned int supported_rotations)
{
	if (rotation & ~supported_rotations) {
		rotation ^= BIT(DRM_REFLECT_X) | BIT(DRM_REFLECT_Y);
		rotation = (rotation & DRM_REFLECT_MASK) |
		           BIT((ffs(rotation & DRM_ROTATE_MASK) + 1) % 4);
	}

	return rotation;
}
EXPORT_SYMBOL(drm_rotation_simplify);

/**
 * drm_mode_config_init - initialize DRM mode_configuration structure
 * @dev: DRM device
 *
 * Initialize @dev's mode_config structure, used for tracking the graphics
 * configuration of @dev.
 *
 * Since this initializes the modeset locks, no locking is possible. Which is no
 * problem, since this should happen single threaded at init time. It is the
 * driver's problem to ensure this guarantee.
 *
 */
void drm_mode_config_init(struct drm_device *dev)
{
	mutex_init(&dev->mode_config.mutex);
	drm_modeset_lock_init(&dev->mode_config.connection_mutex);
	mutex_init(&dev->mode_config.idr_mutex);
	mutex_init(&dev->mode_config.fb_lock);
	mutex_init(&dev->mode_config.blob_lock);
	INIT_LIST_HEAD(&dev->mode_config.fb_list);
	INIT_LIST_HEAD(&dev->mode_config.crtc_list);
	INIT_LIST_HEAD(&dev->mode_config.connector_list);
	INIT_LIST_HEAD(&dev->mode_config.encoder_list);
	INIT_LIST_HEAD(&dev->mode_config.property_list);
	INIT_LIST_HEAD(&dev->mode_config.property_blob_list);
	INIT_LIST_HEAD(&dev->mode_config.plane_list);
	idr_init(&dev->mode_config.crtc_idr);
	idr_init(&dev->mode_config.tile_idr);

	drm_modeset_lock_all(dev);
	drm_mode_create_standard_properties(dev);
	drm_modeset_unlock_all(dev);

	/* Just to be sure */
	dev->mode_config.num_fb = 0;
	dev->mode_config.num_connector = 0;
	dev->mode_config.num_crtc = 0;
	dev->mode_config.num_encoder = 0;
	dev->mode_config.num_overlay_plane = 0;
	dev->mode_config.num_total_plane = 0;
}
EXPORT_SYMBOL(drm_mode_config_init);

/**
 * drm_mode_config_cleanup - free up DRM mode_config info
 * @dev: DRM device
 *
 * Free up all the connectors and CRTCs associated with this DRM device, then
 * free up the framebuffers and associated buffer objects.
 *
 * Note that since this /should/ happen single-threaded at driver/device
 * teardown time, no locking is required. It's the driver's job to ensure that
 * this guarantee actually holds true.
 *
 * FIXME: cleanup any dangling user buffer objects too
 */
void drm_mode_config_cleanup(struct drm_device *dev)
{
	struct drm_connector *connector, *ot;
	struct drm_crtc *crtc, *ct;
	struct drm_encoder *encoder, *enct;
	struct drm_framebuffer *fb, *fbt;
	struct drm_property *property, *pt;
	struct drm_property_blob *blob, *bt;
	struct drm_plane *plane, *plt;

	list_for_each_entry_safe(encoder, enct, &dev->mode_config.encoder_list,
				 head) {
		encoder->funcs->destroy(encoder);
	}

	list_for_each_entry_safe(connector, ot,
				 &dev->mode_config.connector_list, head) {
		connector->funcs->destroy(connector);
	}

	list_for_each_entry_safe(property, pt, &dev->mode_config.property_list,
				 head) {
		drm_property_destroy(dev, property);
	}

	list_for_each_entry_safe(blob, bt, &dev->mode_config.property_blob_list,
				 head_global) {
		drm_property_unreference_blob(blob);
	}

	/*
	 * Single-threaded teardown context, so it's not required to grab the
	 * fb_lock to protect against concurrent fb_list access. Contrary, it
	 * would actually deadlock with the drm_framebuffer_cleanup function.
	 *
	 * Also, if there are any framebuffers left, that's a driver leak now,
	 * so politely WARN about this.
	 */
	WARN_ON(!list_empty(&dev->mode_config.fb_list));
	list_for_each_entry_safe(fb, fbt, &dev->mode_config.fb_list, head) {
		drm_framebuffer_free(&fb->refcount);
	}

	list_for_each_entry_safe(plane, plt, &dev->mode_config.plane_list,
				 head) {
		plane->funcs->destroy(plane);
	}

	list_for_each_entry_safe(crtc, ct, &dev->mode_config.crtc_list, head) {
		crtc->funcs->destroy(crtc);
	}

	idr_destroy(&dev->mode_config.tile_idr);
	idr_destroy(&dev->mode_config.crtc_idr);
	drm_modeset_lock_fini(&dev->mode_config.connection_mutex);
}
EXPORT_SYMBOL(drm_mode_config_cleanup);

struct drm_property *drm_mode_create_rotation_property(struct drm_device *dev,
						       unsigned int supported_rotations)
{
	static const struct drm_prop_enum_list props[] = {
		{ DRM_ROTATE_0,   "rotate-0" },
		{ DRM_ROTATE_90,  "rotate-90" },
		{ DRM_ROTATE_180, "rotate-180" },
		{ DRM_ROTATE_270, "rotate-270" },
		{ DRM_REFLECT_X,  "reflect-x" },
		{ DRM_REFLECT_Y,  "reflect-y" },
	};

	return drm_property_create_bitmask(dev, 0, "rotation",
					   props, ARRAY_SIZE(props),
					   supported_rotations);
}
EXPORT_SYMBOL(drm_mode_create_rotation_property);

/**
 * DOC: Tile group
 *
 * Tile groups are used to represent tiled monitors with a unique
 * integer identifier. Tiled monitors using DisplayID v1.3 have
 * a unique 8-byte handle, we store this in a tile group, so we
 * have a common identifier for all tiles in a monitor group.
 */
static void drm_tile_group_free(struct kref *kref)
{
	struct drm_tile_group *tg = container_of(kref, struct drm_tile_group, refcount);
	struct drm_device *dev = tg->dev;
	mutex_lock(&dev->mode_config.idr_mutex);
	idr_remove(&dev->mode_config.tile_idr, tg->id);
	mutex_unlock(&dev->mode_config.idr_mutex);
	kfree(tg);
}

/**
 * drm_mode_put_tile_group - drop a reference to a tile group.
 * @dev: DRM device
 * @tg: tile group to drop reference to.
 *
 * drop reference to tile group and free if 0.
 */
void drm_mode_put_tile_group(struct drm_device *dev,
			     struct drm_tile_group *tg)
{
	kref_put(&tg->refcount, drm_tile_group_free);
}

/**
 * drm_mode_get_tile_group - get a reference to an existing tile group
 * @dev: DRM device
 * @topology: 8-bytes unique per monitor.
 *
 * Use the unique bytes to get a reference to an existing tile group.
 *
 * RETURNS:
 * tile group or NULL if not found.
 */
struct drm_tile_group *drm_mode_get_tile_group(struct drm_device *dev,
					       char topology[8])
{
	struct drm_tile_group *tg;
	int id;
	mutex_lock(&dev->mode_config.idr_mutex);
	idr_for_each_entry(&dev->mode_config.tile_idr, tg, id) {
		if (!memcmp(tg->group_data, topology, 8)) {
			if (!kref_get_unless_zero(&tg->refcount))
				tg = NULL;
			mutex_unlock(&dev->mode_config.idr_mutex);
			return tg;
		}
	}
	mutex_unlock(&dev->mode_config.idr_mutex);
	return NULL;
}
EXPORT_SYMBOL(drm_mode_get_tile_group);

/**
 * drm_mode_create_tile_group - create a tile group from a displayid description
 * @dev: DRM device
 * @topology: 8-bytes unique per monitor.
 *
 * Create a tile group for the unique monitor, and get a unique
 * identifier for the tile group.
 *
 * RETURNS:
 * new tile group or error.
 */
struct drm_tile_group *drm_mode_create_tile_group(struct drm_device *dev,
						  char topology[8])
{
	struct drm_tile_group *tg;
	int ret;

	tg = kzalloc(sizeof(*tg), GFP_KERNEL);
	if (!tg)
		return ERR_PTR(-ENOMEM);

	kref_init(&tg->refcount);
	memcpy(tg->group_data, topology, 8);
	tg->dev = dev;

	mutex_lock(&dev->mode_config.idr_mutex);
	ret = idr_alloc(&dev->mode_config.tile_idr, tg, 1, 0, GFP_KERNEL);
	if (ret >= 0) {
		tg->id = ret;
	} else {
		kfree(tg);
		tg = ERR_PTR(ret);
	}

	mutex_unlock(&dev->mode_config.idr_mutex);
	return tg;
}
EXPORT_SYMBOL(drm_mode_create_tile_group);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
