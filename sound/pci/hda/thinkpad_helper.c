// SPDX-License-Identifier: GPL-2.0
/* Helper functions for Thinkpad LED control;
 * to be included from codec driver
 */

#if IS_ENABLED(CONFIG_THINKPAD_ACPI) && IS_REACHABLE(CONFIG_LEDS_TRIGGER_AUDIO)

#include <linux/acpi.h>
#include <linux/leds.h>

static void (*old_vmaster_hook)(void *, int);

static bool is_thinkpad(struct hda_codec *codec)
{
	return (codec->core.subsystem_id >> 16 == 0x17aa) &&
	       (acpi_dev_found("LEN0068") || acpi_dev_found("LEN0268") ||
		acpi_dev_found("IBM0068"));
}

static void update_tpacpi_mute_led(void *private_data, int enabled)
{
	if (old_vmaster_hook)
		old_vmaster_hook(private_data, enabled);

	ledtrig_audio_set(LED_AUDIO_MUTE, enabled ? LED_OFF : LED_ON);
}

static void hda_fixup_thinkpad_acpi(struct hda_codec *codec,
				    const struct hda_fixup *fix, int action)
{
	struct hda_gen_spec *spec = codec->spec;

	if (action == HDA_FIXUP_ACT_PROBE) {
		if (!is_thinkpad(codec))
			return;
<<<<<<< HEAD
		old_vmaster_hook = spec->vmaster_mute.hook;
		spec->vmaster_mute.hook = update_tpacpi_mute_led;
		snd_hda_gen_fixup_micmute_led(codec, fix, action);
=======
		if (!led_set_func)
			led_set_func = symbol_request(tpacpi_led_set);
		if (!led_set_func) {
			codec_warn(codec,
				   "Failed to find thinkpad-acpi symbol tpacpi_led_set\n");
			return;
		}

		removefunc = true;
		if (led_set_func(TPACPI_LED_MUTE, false) >= 0) {
			old_vmaster_hook = spec->vmaster_mute.hook;
			spec->vmaster_mute.hook = update_tpacpi_mute_led;
			removefunc = false;
		}
		if (led_set_func(TPACPI_LED_MICMUTE, false) >= 0) {
			if (spec->num_adc_nids > 1 && !spec->dyn_adc_switch)
				codec_dbg(codec,
					  "Skipping micmute LED control due to several ADCs");
			else {
				spec->cap_sync_hook = update_tpacpi_micmute_led;
				removefunc = false;
			}
		}
	}

	if (led_set_func && (action == HDA_FIXUP_ACT_FREE || removefunc)) {
		symbol_put(tpacpi_led_set);
		led_set_func = NULL;
		old_vmaster_hook = NULL;
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
	}
}

#else /* CONFIG_THINKPAD_ACPI */

static void hda_fixup_thinkpad_acpi(struct hda_codec *codec,
				    const struct hda_fixup *fix, int action)
{
}

#endif /* CONFIG_THINKPAD_ACPI */
