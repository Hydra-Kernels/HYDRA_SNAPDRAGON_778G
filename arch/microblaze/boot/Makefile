# SPDX-License-Identifier: GPL-2.0
#
# arch/microblaze/boot/Makefile
#

targets := linux.bin linux.bin.gz linux.bin.ub simpleImage.*

OBJCOPYFLAGS := -R .note -R .comment -R .note.gnu.build-id -O binary

$(obj)/linux.bin: vmlinux FORCE
	$(call if_changed,objcopy)

$(obj)/linux.bin.ub: $(obj)/linux.bin FORCE
	$(call if_changed,uimage)

$(obj)/linux.bin.gz: $(obj)/linux.bin FORCE
	$(call if_changed,gzip)

<<<<<<< HEAD
=======
quiet_cmd_cp = CP      $< $@$2
	cmd_cp = cat $< >$@$2 || (rm -f $@ && echo false)

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
quiet_cmd_strip = STRIP   $< $@$2
	cmd_strip = $(STRIP) -K microblaze_start -K _end -K __log_buf \
				-K _fdt_start $< -o $@$2

UIMAGE_LOADADDR = $(CONFIG_KERNEL_BASE_ADDR)
UIMAGE_IN = $@
UIMAGE_OUT = $@.ub

$(obj)/simpleImage.$(DTB): vmlinux FORCE
	$(call if_changed,objcopy)
<<<<<<< HEAD
=======
	$(call if_changed,uimage)
	$(call if_changed,strip,.strip)
	@echo 'Kernel: $(UIMAGE_OUT) is ready' ' (#'`cat .version`')'
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

$(obj)/simpleImage.$(DTB).ub: $(obj)/simpleImage.$(DTB) FORCE
	$(call if_changed,uimage)

$(obj)/simpleImage.$(DTB).unstrip: vmlinux FORCE
	$(call if_changed,shipped)

$(obj)/simpleImage.$(DTB).strip: vmlinux FORCE
	$(call if_changed,strip)
