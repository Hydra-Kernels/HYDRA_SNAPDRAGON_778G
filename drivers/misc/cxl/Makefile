<<<<<<< HEAD
# SPDX-License-Identifier: GPL-2.0
ccflags-y			:= $(call cc-disable-warning, unused-const-variable)
ccflags-$(CONFIG_PPC_WERROR)	+= -Werror
=======
ccflags-y := -Werror $(call cc-disable-warning, unused-const-variable)
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

cxl-y				+= main.o file.o irq.o fault.o native.o
cxl-y				+= context.o sysfs.o pci.o trace.o
cxl-y				+= vphb.o api.o cxllib.o
cxl-$(CONFIG_PPC_PSERIES)	+= flash.o guest.o of.o hcalls.o
cxl-$(CONFIG_DEBUG_FS)		+= debugfs.o
obj-$(CONFIG_CXL)		+= cxl.o
obj-$(CONFIG_CXL_BASE)		+= base.o

# For tracepoints to include our trace.h from tracepoint infrastructure:
CFLAGS_trace.o := -I$(src)
