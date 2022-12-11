<<<<<<< HEAD
# SPDX-License-Identifier: GPL-2.0
=======
$(obj)/qat_rsapubkey-asn1.o: $(obj)/qat_rsapubkey-asn1.c \
			     $(obj)/qat_rsapubkey-asn1.h
$(obj)/qat_rsaprivkey-asn1.o: $(obj)/qat_rsaprivkey-asn1.c \
			      $(obj)/qat_rsaprivkey-asn1.h
$(obj)/qat_asym_algs.o: $(obj)/qat_rsapubkey-asn1.h $(obj)/qat_rsaprivkey-asn1.h

clean-files += qat_rsapubkey-asn1.c qat_rsapubkey-asn1.h
clean-files += qat_rsaprivkey-asn1.c qat_rsapvivkey-asn1.h

>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
obj-$(CONFIG_CRYPTO_DEV_QAT) += intel_qat.o
intel_qat-objs := adf_cfg.o \
	adf_isr.o \
	adf_ctl_drv.o \
	adf_dev_mgr.o \
	adf_init.o \
	adf_accel_engine.o \
	adf_aer.o \
	adf_transport.o \
	adf_admin.o \
	adf_hw_arbiter.o \
	qat_crypto.o \
	qat_algs.o \
	qat_asym_algs.o \
	qat_uclo.o \
	qat_hal.o

intel_qat-$(CONFIG_DEBUG_FS) += adf_transport_debug.o
intel_qat-$(CONFIG_PCI_IOV) += adf_sriov.o adf_pf2vf_msg.o \
			       adf_vf2pf_msg.o adf_vf_isr.o
