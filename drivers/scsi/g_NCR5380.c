// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generic Generic NCR5380 driver
 *
 * Copyright 1993, Drew Eckhardt
 * Visionary Computing
 * (Unix and Linux consulting and custom programming)
 * drew@colorado.edu
 * +1 (303) 440-4894
 *
 * NCR53C400 extensions (c) 1994,1995,1996, Kevin Lentin
 * K.Lentin@cs.monash.edu.au
 *
 * NCR53C400A extensions (c) 1996, Ingmar Baumgart
 * ingmar@gonzo.schwaben.de
 *
 * DTC3181E extensions (c) 1997, Ronald van Cuijlenborg
 * ronald.van.cuijlenborg@tip.nl or nutty@dds.nl
 *
 * Added ISAPNP support for DTC436 adapters,
 * Thomas Sailer, sailer@ife.ee.ethz.ch
 *
 * See Documentation/scsi/g_NCR5380.txt for more info.
 */

#include <asm/io.h>
#include <linux/blkdev.h>
#include <linux/module.h>
#include <scsi/scsi_host.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/isa.h>
#include <linux/pnp.h>
#include <linux/interrupt.h>

/* Definitions for the core NCR5380 driver. */

#define NCR5380_read(reg) \
	ioread8(hostdata->io + hostdata->offset + (reg))
#define NCR5380_write(reg, value) \
	iowrite8(value, hostdata->io + hostdata->offset + (reg))

#define NCR5380_implementation_fields \
	int offset; \
	int c400_ctl_status; \
	int c400_blk_cnt; \
	int c400_host_buf; \
	int io_width; \
	int pdma_residual; \
	int board

#define NCR5380_dma_xfer_len            generic_NCR5380_dma_xfer_len
#define NCR5380_dma_recv_setup          generic_NCR5380_precv
#define NCR5380_dma_send_setup          generic_NCR5380_psend
#define NCR5380_dma_residual            generic_NCR5380_dma_residual

#define NCR5380_intr                    generic_NCR5380_intr
#define NCR5380_queue_command           generic_NCR5380_queue_command
#define NCR5380_abort                   generic_NCR5380_abort
#define NCR5380_host_reset              generic_NCR5380_host_reset
#define NCR5380_info                    generic_NCR5380_info

#define NCR5380_io_delay(x)             udelay(x)

#include "NCR5380.h"

#define DRV_MODULE_NAME "g_NCR5380"

#define NCR53C400_mem_base 0x3880
#define NCR53C400_host_buffer 0x3900
#define NCR53C400_region_size 0x3a00

#define BOARD_NCR5380 0
#define BOARD_NCR53C400 1
#define BOARD_NCR53C400A 2
#define BOARD_DTC3181E 3
#define BOARD_HP_C2502 4

#define IRQ_AUTO 254

#define MAX_CARDS 8
#define DMA_MAX_SIZE 32768

<<<<<<< HEAD
/* old-style parameters for compatibility */
static int ncr_irq = -1;
static int ncr_addr;
static int ncr_5380;
static int ncr_53c400;
static int ncr_53c400a;
static int dtc_3181e;
static int hp_c2502;
module_param_hw(ncr_irq, int, irq, 0);
module_param_hw(ncr_addr, int, ioport, 0);
=======
static int __init do_NCR53C400_setup(char *str)
{
	int ints[10];

	get_options(str, ARRAY_SIZE(ints), ints);
	internal_setup(BOARD_NCR53C400, str, ints);
	return 1;
}

/**
 * 	do_NCR53C400A_setup	-	set up entry point
 *	@str: unused
 *	@ints: integer parameters from kernel setup code
 *
 *	Setup function invoked at boot to parse the ncr53c400a= command
 *	line.
 */

static int __init do_NCR53C400A_setup(char *str)
{
	int ints[10];

	get_options(str, ARRAY_SIZE(ints), ints);
	internal_setup(BOARD_NCR53C400A, str, ints);
	return 1;
}

/**
 * 	do_DTC3181E_setup	-	set up entry point
 *	@str: unused
 *	@ints: integer parameters from kernel setup code
 *
 *	Setup function invoked at boot to parse the dtc3181e= command
 *	line.
 */

static int __init do_DTC3181E_setup(char *str)
{
	int ints[10];

	get_options(str, ARRAY_SIZE(ints), ints);
	internal_setup(BOARD_DTC3181E, str, ints);
	return 1;
}

#endif

/**
 * 	generic_NCR5380_detect	-	look for NCR5380 controllers
 *	@tpnt: the scsi template
 *
 *	Scan for the present of NCR5380, NCR53C400, NCR53C400A, DTC3181E
 *	and DTC436(ISAPnP) controllers. If overrides have been set we use
 *	them.
 *
 *	The caller supplied NCR5380_init function is invoked from here, before
 *	the interrupt line is taken.
 *
 *	Locks: none
 */

static int __init generic_NCR5380_detect(struct scsi_host_template *tpnt)
{
	static int current_override = 0;
	int count;
	unsigned int *ports;
#ifndef SCSI_G_NCR5380_MEM
	int i;
	unsigned long region_size = 16;
#endif
	static unsigned int __initdata ncr_53c400a_ports[] = {
		0x280, 0x290, 0x300, 0x310, 0x330, 0x340, 0x348, 0x350, 0
	};
	static unsigned int __initdata dtc_3181e_ports[] = {
		0x220, 0x240, 0x280, 0x2a0, 0x2c0, 0x300, 0x320, 0x340, 0
	};
	int flags = 0;
	struct Scsi_Host *instance;
#ifdef SCSI_G_NCR5380_MEM
	unsigned long base;
	void __iomem *iomem;
#endif

	if (ncr_irq != NCR_NOT_SET)
		overrides[0].irq = ncr_irq;
	if (ncr_dma != NCR_NOT_SET)
		overrides[0].dma = ncr_dma;
	if (ncr_addr != NCR_NOT_SET)
		overrides[0].NCR5380_map_name = (NCR5380_map_type) ncr_addr;
	if (ncr_5380 != NCR_NOT_SET)
		overrides[0].board = BOARD_NCR5380;
	else if (ncr_53c400 != NCR_NOT_SET)
		overrides[0].board = BOARD_NCR53C400;
	else if (ncr_53c400a != NCR_NOT_SET)
		overrides[0].board = BOARD_NCR53C400A;
	else if (dtc_3181e != NCR_NOT_SET)
		overrides[0].board = BOARD_DTC3181E;
#ifndef SCSI_G_NCR5380_MEM
	if (!current_override && isapnp_present()) {
		struct pnp_dev *dev = NULL;
		count = 0;
		while ((dev = pnp_find_dev(NULL, ISAPNP_VENDOR('D', 'T', 'C'), ISAPNP_FUNCTION(0x436e), dev))) {
			if (count >= NO_OVERRIDES)
				break;
			if (pnp_device_attach(dev) < 0)
				continue;
			if (pnp_activate_dev(dev) < 0) {
				printk(KERN_ERR "dtc436e probe: activate failed\n");
				pnp_device_detach(dev);
				continue;
			}
			if (!pnp_port_valid(dev, 0)) {
				printk(KERN_ERR "dtc436e probe: no valid port\n");
				pnp_device_detach(dev);
				continue;
			}
			if (pnp_irq_valid(dev, 0))
				overrides[count].irq = pnp_irq(dev, 0);
			else
				overrides[count].irq = NO_IRQ;
			if (pnp_dma_valid(dev, 0))
				overrides[count].dma = pnp_dma(dev, 0);
			else
				overrides[count].dma = DMA_NONE;
			overrides[count].NCR5380_map_name = (NCR5380_map_type) pnp_port_start(dev, 0);
			overrides[count].board = BOARD_DTC3181E;
			count++;
		}
	}
#endif
	tpnt->proc_name = "g_NCR5380";

	for (count = 0; current_override < NO_OVERRIDES; ++current_override) {
		if (!(overrides[current_override].NCR5380_map_name))
			continue;

		ports = NULL;
		switch (overrides[current_override].board) {
		case BOARD_NCR5380:
			flags = FLAG_NO_PSEUDO_DMA;
			break;
		case BOARD_NCR53C400:
			flags = FLAG_NCR53C400;
			break;
		case BOARD_NCR53C400A:
			flags = FLAG_NO_PSEUDO_DMA;
			ports = ncr_53c400a_ports;
			break;
		case BOARD_DTC3181E:
			flags = FLAG_NO_PSEUDO_DMA | FLAG_DTC3181E;
			ports = dtc_3181e_ports;
			break;
		}

#ifndef SCSI_G_NCR5380_MEM
		if (ports) {
			/* wakeup sequence for the NCR53C400A and DTC3181E */

			/* Disable the adapter and look for a free io port */
			outb(0x59, 0x779);
			outb(0xb9, 0x379);
			outb(0xc5, 0x379);
			outb(0xae, 0x379);
			outb(0xa6, 0x379);
			outb(0x00, 0x379);

			if (overrides[current_override].NCR5380_map_name != PORT_AUTO)
				for (i = 0; ports[i]; i++) {
					if (!request_region(ports[i],  16, "ncr53c80"))
						continue;
					if (overrides[current_override].NCR5380_map_name == ports[i])
						break;
					release_region(ports[i], 16);
			} else
				for (i = 0; ports[i]; i++) {
					if (!request_region(ports[i],  16, "ncr53c80"))
						continue;
					if (inb(ports[i]) == 0xff)
						break;
					release_region(ports[i], 16);
				}
			if (ports[i]) {
				/* At this point we have our region reserved */
				outb(0x59, 0x779);
				outb(0xb9, 0x379);
				outb(0xc5, 0x379);
				outb(0xae, 0x379);
				outb(0xa6, 0x379);
				outb(0x80 | i, 0x379);	/* set io port to be used */
				outb(0xc0, ports[i] + 9);
				if (inb(ports[i] + 9) != 0x80)
					continue;
				else
					overrides[current_override].NCR5380_map_name = ports[i];
			} else
				continue;
		}
		else
		{
			/* Not a 53C400A style setup - just grab */
			if(!(request_region(overrides[current_override].NCR5380_map_name, NCR5380_region_size, "ncr5380")))
				continue;
			region_size = NCR5380_region_size;
		}
#else
		base = overrides[current_override].NCR5380_map_name;
		if (!request_mem_region(base, NCR5380_region_size, "ncr5380"))
			continue;
		iomem = ioremap(base, NCR5380_region_size);
		if (!iomem) {
			release_mem_region(base, NCR5380_region_size);
			continue;
		}
#endif
		instance = scsi_register(tpnt, sizeof(struct NCR5380_hostdata));
		if (instance == NULL) {
#ifndef SCSI_G_NCR5380_MEM
			release_region(overrides[current_override].NCR5380_map_name, region_size);
#else
			iounmap(iomem);
			release_mem_region(base, NCR5380_region_size);
#endif
			continue;
		}

		instance->NCR5380_instance_name = overrides[current_override].NCR5380_map_name;
#ifndef SCSI_G_NCR5380_MEM
		instance->n_io_port = region_size;
#else
		((struct NCR5380_hostdata *)instance->hostdata)->iomem = iomem;
#endif

		NCR5380_init(instance, flags);

		if (overrides[current_override].irq != IRQ_AUTO)
			instance->irq = overrides[current_override].irq;
		else
			instance->irq = NCR5380_probe_irq(instance, 0xffff);

		/* Compatibility with documented NCR5380 kernel parameters */
		if (instance->irq == 255)
			instance->irq = NO_IRQ;

		if (instance->irq != NO_IRQ)
			if (request_irq(instance->irq, generic_NCR5380_intr,
					0, "NCR5380", instance)) {
				printk(KERN_WARNING "scsi%d : IRQ%d not free, interrupts disabled\n", instance->host_no, instance->irq);
				instance->irq = NO_IRQ;
			}

		if (instance->irq == NO_IRQ) {
			printk(KERN_INFO "scsi%d : interrupts not enabled. for better interactive performance,\n", instance->host_no);
			printk(KERN_INFO "scsi%d : please jumper the board for a free IRQ.\n", instance->host_no);
		}

		++current_override;
		++count;
	}
	return count;
}

/**
 *	generic_NCR5380_release_resources	-	free resources
 *	@instance: host adapter to clean up 
 *
 *	Free the generic interface resources from this adapter.
 *
 *	Locks: none
 */
 
static int generic_NCR5380_release_resources(struct Scsi_Host *instance)
{
	NCR5380_local_declare();
	NCR5380_setup(instance);
	
	if (instance->irq != NO_IRQ)
		free_irq(instance->irq, instance);
	NCR5380_exit(instance);

#ifndef SCSI_G_NCR5380_MEM
	release_region(instance->NCR5380_instance_name, instance->n_io_port);
#else
	iounmap(((struct NCR5380_hostdata *)instance->hostdata)->iomem);
	release_mem_region(instance->NCR5380_instance_name, NCR5380_region_size);
#endif


	return 0;
}

#ifdef BIOSPARAM
/**
 *	generic_NCR5380_biosparam
 *	@disk: disk to compute geometry for
 *	@dev: device identifier for this disk
 *	@ip: sizes to fill in
 *
 *	Generates a BIOS / DOS compatible H-C-S mapping for the specified 
 *	device / size.
 * 
 * 	XXX Most SCSI boards use this mapping, I could be incorrect.  Someone
 *	using hard disks on a trantor should verify that this mapping
 *	corresponds to that used by the BIOS / ASPI driver by running the linux
 *	fdisk program and matching the H_C_S coordinates to what DOS uses.
 *
 *	Locks: none
 */

static int
generic_NCR5380_biosparam(struct scsi_device *sdev, struct block_device *bdev,
			  sector_t capacity, int *ip)
{
	ip[0] = 64;
	ip[1] = 32;
	ip[2] = capacity >> 11;
	return 0;
}
#endif

#ifdef NCR53C400_PSEUDO_DMA

/**
 *	NCR5380_pread		-	pseudo DMA read
 *	@instance: adapter to read from
 *	@dst: buffer to read into
 *	@len: buffer length
 *
 *	Perform a pseudo DMA mode read from an NCR53C400 or equivalent
 *	controller
 */
 
static inline int NCR5380_pread(struct Scsi_Host *instance, unsigned char *dst, int len)
{
	int blocks = len / 128;
	int start = 0;
	int bl;

	NCR5380_local_declare();
	NCR5380_setup(instance);

	NCR5380_write(C400_CONTROL_STATUS_REG, CSR_BASE | CSR_TRANS_DIR);
	NCR5380_write(C400_BLOCK_COUNTER_REG, blocks);
	while (1) {
		if ((bl = NCR5380_read(C400_BLOCK_COUNTER_REG)) == 0) {
			break;
		}
		if (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_GATED_53C80_IRQ) {
			printk(KERN_ERR "53C400r: Got 53C80_IRQ start=%d, blocks=%d\n", start, blocks);
			return -1;
		}
		while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_HOST_BUF_NOT_RDY)
		{
			// FIXME - no timeout
		}

#ifndef SCSI_G_NCR5380_MEM
		{
			int i;
			for (i = 0; i < 128; i++)
				dst[start + i] = NCR5380_read(C400_HOST_BUFFER);
		}
#else
		/* implies SCSI_G_NCR5380_MEM */
		memcpy_fromio(dst + start, iomem + NCR53C400_host_buffer, 128);
#endif
		start += 128;
		blocks--;
	}

	if (blocks) {
		while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_HOST_BUF_NOT_RDY)
		{
			// FIXME - no timeout
		}

#ifndef SCSI_G_NCR5380_MEM
		{
			int i;	
			for (i = 0; i < 128; i++)
				dst[start + i] = NCR5380_read(C400_HOST_BUFFER);
		}
#else
		/* implies SCSI_G_NCR5380_MEM */
		memcpy_fromio(dst + start, iomem + NCR53C400_host_buffer, 128);
#endif
		start += 128;
		blocks--;
	}

	if (!(NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_GATED_53C80_IRQ))
		printk("53C400r: no 53C80 gated irq after transfer");

#if 0
	/*
	 *	DON'T DO THIS - THEY NEVER ARRIVE!
	 */
	printk("53C400r: Waiting for 53C80 registers\n");
	while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_53C80_REG)
		;
#endif
	if (!(NCR5380_read(BUS_AND_STATUS_REG) & BASR_END_DMA_TRANSFER))
		printk(KERN_ERR "53C400r: no end dma signal\n");
		
	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	return 0;
}

/**
 *	NCR5380_write		-	pseudo DMA write
 *	@instance: adapter to read from
 *	@dst: buffer to read into
 *	@len: buffer length
 *
 *	Perform a pseudo DMA mode read from an NCR53C400 or equivalent
 *	controller
 */

static inline int NCR5380_pwrite(struct Scsi_Host *instance, unsigned char *src, int len)
{
	int blocks = len / 128;
	int start = 0;
	int bl;
	int i;

	NCR5380_local_declare();
	NCR5380_setup(instance);

	NCR5380_write(C400_CONTROL_STATUS_REG, CSR_BASE);
	NCR5380_write(C400_BLOCK_COUNTER_REG, blocks);
	while (1) {
		if (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_GATED_53C80_IRQ) {
			printk(KERN_ERR "53C400w: Got 53C80_IRQ start=%d, blocks=%d\n", start, blocks);
			return -1;
		}

		if ((bl = NCR5380_read(C400_BLOCK_COUNTER_REG)) == 0) {
			break;
		}
		while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_HOST_BUF_NOT_RDY)
			; // FIXME - timeout
#ifndef SCSI_G_NCR5380_MEM
		{
			for (i = 0; i < 128; i++)
				NCR5380_write(C400_HOST_BUFFER, src[start + i]);
		}
#else
		/* implies SCSI_G_NCR5380_MEM */
		memcpy_toio(iomem + NCR53C400_host_buffer, src + start, 128);
#endif
		start += 128;
		blocks--;
	}
	if (blocks) {
		while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_HOST_BUF_NOT_RDY)
			; // FIXME - no timeout

#ifndef SCSI_G_NCR5380_MEM
		{
			for (i = 0; i < 128; i++)
				NCR5380_write(C400_HOST_BUFFER, src[start + i]);
		}
#else
		/* implies SCSI_G_NCR5380_MEM */
		memcpy_toio(iomem + NCR53C400_host_buffer, src + start, 128);
#endif
		start += 128;
		blocks--;
	}

#if 0
	printk("53C400w: waiting for registers to be available\n");
	THEY NEVER DO ! while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_53C80_REG);
	printk("53C400w: Got em\n");
#endif

	/* Let's wait for this instead - could be ugly */
	/* All documentation says to check for this. Maybe my hardware is too
	 * fast. Waiting for it seems to work fine! KLL
	 */
	while (!(i = NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_GATED_53C80_IRQ))
		;	// FIXME - no timeout

	/*
	 * I know. i is certainly != 0 here but the loop is new. See previous
	 * comment.
	 */
	if (i) {
		if (!((i = NCR5380_read(BUS_AND_STATUS_REG)) & BASR_END_DMA_TRANSFER))
			printk(KERN_ERR "53C400w: No END OF DMA bit - WHOOPS! BASR=%0x\n", i);
	} else
		printk(KERN_ERR "53C400w: no 53C80 gated irq after transfer (last block)\n");

#if 0
	if (!(NCR5380_read(BUS_AND_STATUS_REG) & BASR_END_DMA_TRANSFER)) {
		printk(KERN_ERR "53C400w: no end dma signal\n");
	}
#endif
	while (!(NCR5380_read(TARGET_COMMAND_REG) & TCR_LAST_BYTE_SENT))
		; 	// TIMEOUT
	return 0;
}
#endif				/* PSEUDO_DMA */

/*
 *	Include the NCR5380 core code that we build our driver around	
 */
 
#include "NCR5380.c"

static struct scsi_host_template driver_template = {
	.show_info      	= generic_NCR5380_show_info,
	.name           	= "Generic NCR5380/NCR53C400 SCSI",
	.detect         	= generic_NCR5380_detect,
	.release        	= generic_NCR5380_release_resources,
	.info           	= generic_NCR5380_info,
	.queuecommand   	= generic_NCR5380_queue_command,
	.eh_abort_handler	= generic_NCR5380_abort,
	.eh_bus_reset_handler	= generic_NCR5380_bus_reset,
	.bios_param     	= NCR5380_BIOSPARAM,
	.can_queue      	= CAN_QUEUE,
        .this_id        	= 7,
        .sg_tablesize   	= SG_ALL,
	.cmd_per_lun    	= CMD_PER_LUN,
        .use_clustering		= DISABLE_CLUSTERING,
};
#include <linux/module.h>
#include "scsi_module.c"

module_param(ncr_irq, int, 0);
module_param(ncr_dma, int, 0);
module_param(ncr_addr, int, 0);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
module_param(ncr_5380, int, 0);
module_param(ncr_53c400, int, 0);
module_param(ncr_53c400a, int, 0);
module_param(dtc_3181e, int, 0);
module_param(hp_c2502, int, 0);

static int irq[] = { -1, -1, -1, -1, -1, -1, -1, -1 };
module_param_hw_array(irq, int, irq, NULL, 0);
MODULE_PARM_DESC(irq, "IRQ number(s) (0=none, 254=auto [default])");

static int base[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
module_param_hw_array(base, int, ioport, NULL, 0);
MODULE_PARM_DESC(base, "base address(es)");

static int card[] = { -1, -1, -1, -1, -1, -1, -1, -1 };
module_param_array(card, int, NULL, 0);
MODULE_PARM_DESC(card, "card type (0=NCR5380, 1=NCR53C400, 2=NCR53C400A, 3=DTC3181E, 4=HP C2502)");

MODULE_ALIAS("g_NCR5380_mmio");
MODULE_LICENSE("GPL");

static void g_NCR5380_trigger_irq(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);

	/*
	 * An interrupt is triggered whenever BSY = false, SEL = true
	 * and a bit set in the SELECT_ENABLE_REG is asserted on the
	 * SCSI bus.
	 *
	 * Note that the bus is only driven when the phase control signals
	 * (I/O, C/D, and MSG) match those in the TCR.
	 */
	NCR5380_write(TARGET_COMMAND_REG,
	              PHASE_SR_TO_TCR(NCR5380_read(STATUS_REG) & PHASE_MASK));
	NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
	NCR5380_write(OUTPUT_DATA_REG, hostdata->id_mask);
	NCR5380_write(INITIATOR_COMMAND_REG,
	              ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_SEL);

	msleep(1);

	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	NCR5380_write(SELECT_ENABLE_REG, 0);
	NCR5380_write(TARGET_COMMAND_REG, 0);
}

/**
 * g_NCR5380_probe_irq - find the IRQ of a NCR5380 or equivalent
 * @instance: SCSI host instance
 *
 * Autoprobe for the IRQ line used by the card by triggering an IRQ
 * and then looking to see what interrupt actually turned up.
 */

static int g_NCR5380_probe_irq(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	int irq_mask, irq;

	NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	irq_mask = probe_irq_on();
	g_NCR5380_trigger_irq(instance);
	irq = probe_irq_off(irq_mask);
	NCR5380_read(RESET_PARITY_INTERRUPT_REG);

	if (irq <= 0)
		return NO_IRQ;
	return irq;
}

/*
 * Configure I/O address of 53C400A or DTC436 by writing magic numbers
 * to ports 0x779 and 0x379.
 */
static void magic_configure(int idx, u8 irq, u8 magic[])
{
	u8 cfg = 0;

	outb(magic[0], 0x779);
	outb(magic[1], 0x379);
	outb(magic[2], 0x379);
	outb(magic[3], 0x379);
	outb(magic[4], 0x379);

	if (irq == 9)
		irq = 2;

	if (idx >= 0 && idx <= 7)
		cfg = 0x80 | idx | (irq << 4);
	outb(cfg, 0x379);
}

static irqreturn_t legacy_empty_irq_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static int legacy_find_free_irq(int *irq_table)
{
	while (*irq_table != -1) {
		if (!request_irq(*irq_table, legacy_empty_irq_handler,
		                 IRQF_PROBE_SHARED, "Test IRQ",
		                 (void *)irq_table)) {
			free_irq(*irq_table, (void *) irq_table);
			return *irq_table;
		}
		irq_table++;
	}
	return -1;
}

static unsigned int ncr_53c400a_ports[] = {
	0x280, 0x290, 0x300, 0x310, 0x330, 0x340, 0x348, 0x350, 0
};
static unsigned int dtc_3181e_ports[] = {
	0x220, 0x240, 0x280, 0x2a0, 0x2c0, 0x300, 0x320, 0x340, 0
};
static u8 ncr_53c400a_magic[] = {	/* 53C400A & DTC436 */
	0x59, 0xb9, 0xc5, 0xae, 0xa6
};
static u8 hp_c2502_magic[] = {	/* HP C2502 */
	0x0f, 0x22, 0xf0, 0x20, 0x80
};
static int hp_c2502_irqs[] = {
	9, 5, 7, 3, 4, -1
};

static int generic_NCR5380_init_one(struct scsi_host_template *tpnt,
			struct device *pdev, int base, int irq, int board)
{
	bool is_pmio = base <= 0xffff;
	int ret;
	int flags = 0;
	unsigned int *ports = NULL;
	u8 *magic = NULL;
	int i;
	int port_idx = -1;
	unsigned long region_size;
	struct Scsi_Host *instance;
	struct NCR5380_hostdata *hostdata;
	u8 __iomem *iomem;

	switch (board) {
	case BOARD_NCR5380:
		flags = FLAG_NO_PSEUDO_DMA | FLAG_DMA_FIXUP;
		break;
	case BOARD_NCR53C400A:
		ports = ncr_53c400a_ports;
		magic = ncr_53c400a_magic;
		break;
	case BOARD_HP_C2502:
		ports = ncr_53c400a_ports;
		magic = hp_c2502_magic;
		break;
	case BOARD_DTC3181E:
		ports = dtc_3181e_ports;
		magic = ncr_53c400a_magic;
		break;
	}

	if (is_pmio && ports && magic) {
		/* wakeup sequence for the NCR53C400A and DTC3181E */

		/* Disable the adapter and look for a free io port */
		magic_configure(-1, 0, magic);

		region_size = 16;
		if (base)
			for (i = 0; ports[i]; i++) {
				if (base == ports[i]) {	/* index found */
					if (!request_region(ports[i],
							    region_size,
							    "ncr53c80"))
						return -EBUSY;
					break;
				}
			}
		else
			for (i = 0; ports[i]; i++) {
				if (!request_region(ports[i], region_size,
						    "ncr53c80"))
					continue;
				if (inb(ports[i]) == 0xff)
					break;
				release_region(ports[i], region_size);
			}
		if (ports[i]) {
			/* At this point we have our region reserved */
			magic_configure(i, 0, magic); /* no IRQ yet */
			base = ports[i];
			outb(0xc0, base + 9);
			if (inb(base + 9) != 0x80) {
				ret = -ENODEV;
				goto out_release;
			}
			port_idx = i;
		} else
			return -EINVAL;
	} else if (is_pmio) {
		/* NCR5380 - no configuration, just grab */
		region_size = 8;
		if (!base || !request_region(base, region_size, "ncr5380"))
			return -EBUSY;
	} else {	/* MMIO */
		region_size = NCR53C400_region_size;
		if (!request_mem_region(base, region_size, "ncr5380"))
			return -EBUSY;
	}

	if (is_pmio)
		iomem = ioport_map(base, region_size);
	else
		iomem = ioremap(base, region_size);

	if (!iomem) {
		ret = -ENOMEM;
		goto out_release;
	}

	instance = scsi_host_alloc(tpnt, sizeof(struct NCR5380_hostdata));
	if (instance == NULL) {
		ret = -ENOMEM;
		goto out_unmap;
	}
	hostdata = shost_priv(instance);

	hostdata->board = board;
	hostdata->io = iomem;
	hostdata->region_size = region_size;

	if (is_pmio) {
		hostdata->io_port = base;
		hostdata->io_width = 1; /* 8-bit PDMA by default */
		hostdata->offset = 0;

		/*
		 * On NCR53C400 boards, NCR5380 registers are mapped 8 past
		 * the base address.
		 */
		switch (board) {
		case BOARD_NCR53C400:
			hostdata->io_port += 8;
			hostdata->c400_ctl_status = 0;
			hostdata->c400_blk_cnt = 1;
			hostdata->c400_host_buf = 4;
			break;
		case BOARD_DTC3181E:
			hostdata->io_width = 2;	/* 16-bit PDMA */
			/* fall through */
		case BOARD_NCR53C400A:
		case BOARD_HP_C2502:
			hostdata->c400_ctl_status = 9;
			hostdata->c400_blk_cnt = 10;
			hostdata->c400_host_buf = 8;
			break;
		}
	} else {
		hostdata->base = base;
		hostdata->offset = NCR53C400_mem_base;
		switch (board) {
		case BOARD_NCR53C400:
			hostdata->c400_ctl_status = 0x100;
			hostdata->c400_blk_cnt = 0x101;
			hostdata->c400_host_buf = 0x104;
			break;
		case BOARD_DTC3181E:
		case BOARD_NCR53C400A:
		case BOARD_HP_C2502:
			pr_err(DRV_MODULE_NAME ": unknown register offsets\n");
			ret = -EINVAL;
			goto out_unregister;
		}
	}

	/* Check for vacant slot */
	NCR5380_write(MODE_REG, 0);
	if (NCR5380_read(MODE_REG) != 0) {
		ret = -ENODEV;
		goto out_unregister;
	}

	ret = NCR5380_init(instance, flags | FLAG_LATE_DMA_SETUP);
	if (ret)
		goto out_unregister;

	switch (board) {
	case BOARD_NCR53C400:
	case BOARD_DTC3181E:
	case BOARD_NCR53C400A:
	case BOARD_HP_C2502:
		NCR5380_write(hostdata->c400_ctl_status, CSR_BASE);
	}

	NCR5380_maybe_reset_bus(instance);

	/* Compatibility with documented NCR5380 kernel parameters */
	if (irq == 255 || irq == 0)
		irq = NO_IRQ;
	else if (irq == -1)
		irq = IRQ_AUTO;

	if (board == BOARD_HP_C2502) {
		int *irq_table = hp_c2502_irqs;
		int board_irq = -1;

		switch (irq) {
		case NO_IRQ:
			board_irq = 0;
			break;
		case IRQ_AUTO:
			board_irq = legacy_find_free_irq(irq_table);
			break;
		default:
			while (*irq_table != -1)
				if (*irq_table++ == irq)
					board_irq = irq;
		}

		if (board_irq <= 0) {
			board_irq = 0;
			irq = NO_IRQ;
		}

		magic_configure(port_idx, board_irq, magic);
	}

	if (irq == IRQ_AUTO) {
		instance->irq = g_NCR5380_probe_irq(instance);
		if (instance->irq == NO_IRQ)
			shost_printk(KERN_INFO, instance, "no irq detected\n");
	} else {
		instance->irq = irq;
		if (instance->irq == NO_IRQ)
			shost_printk(KERN_INFO, instance, "no irq provided\n");
	}

	if (instance->irq != NO_IRQ) {
		if (request_irq(instance->irq, generic_NCR5380_intr,
				0, "NCR5380", instance)) {
			instance->irq = NO_IRQ;
			shost_printk(KERN_INFO, instance,
			             "irq %d denied\n", instance->irq);
		} else {
			shost_printk(KERN_INFO, instance,
			             "irq %d acquired\n", instance->irq);
		}
	}

	ret = scsi_add_host(instance, pdev);
	if (ret)
		goto out_free_irq;
	scsi_scan_host(instance);
	dev_set_drvdata(pdev, instance);
	return 0;

out_free_irq:
	if (instance->irq != NO_IRQ)
		free_irq(instance->irq, instance);
	NCR5380_exit(instance);
out_unregister:
	scsi_host_put(instance);
out_unmap:
	iounmap(iomem);
out_release:
	if (is_pmio)
		release_region(base, region_size);
	else
		release_mem_region(base, region_size);
	return ret;
}

static void generic_NCR5380_release_resources(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	void __iomem *iomem = hostdata->io;
	unsigned long io_port = hostdata->io_port;
	unsigned long base = hostdata->base;
	unsigned long region_size = hostdata->region_size;

	scsi_remove_host(instance);
	if (instance->irq != NO_IRQ)
		free_irq(instance->irq, instance);
	NCR5380_exit(instance);
	scsi_host_put(instance);
	iounmap(iomem);
	if (io_port)
		release_region(io_port, region_size);
	else
		release_mem_region(base, region_size);
}

/* wait_for_53c80_access - wait for 53C80 registers to become accessible
 * @hostdata: scsi host private data
 *
 * The registers within the 53C80 logic block are inaccessible until
 * bit 7 in the 53C400 control status register gets asserted.
 */

static void wait_for_53c80_access(struct NCR5380_hostdata *hostdata)
{
	int count = 10000;

	do {
		if (hostdata->board == BOARD_DTC3181E)
			udelay(4); /* DTC436 chip hangs without this */
		if (NCR5380_read(hostdata->c400_ctl_status) & CSR_53C80_REG)
			return;
	} while (--count > 0);

	scmd_printk(KERN_ERR, hostdata->connected,
	            "53c80 registers not accessible, device will be reset\n");
	NCR5380_write(hostdata->c400_ctl_status, CSR_RESET);
	NCR5380_write(hostdata->c400_ctl_status, CSR_BASE);
}

/**
 * generic_NCR5380_precv - pseudo DMA receive
 * @hostdata: scsi host private data
 * @dst: buffer to write into
 * @len: transfer size
 *
 * Perform a pseudo DMA mode receive from a 53C400 or equivalent device.
 */

static inline int generic_NCR5380_precv(struct NCR5380_hostdata *hostdata,
                                        unsigned char *dst, int len)
{
	int residual;
	int start = 0;

	NCR5380_write(hostdata->c400_ctl_status, CSR_BASE | CSR_TRANS_DIR);
	NCR5380_write(hostdata->c400_blk_cnt, len / 128);

	do {
		if (start == len - 128) {
			/* Ignore End of DMA interrupt for the final buffer */
			if (NCR5380_poll_politely(hostdata, hostdata->c400_ctl_status,
			                          CSR_HOST_BUF_NOT_RDY, 0, HZ / 64) < 0)
				break;
		} else {
			if (NCR5380_poll_politely2(hostdata, hostdata->c400_ctl_status,
			                           CSR_HOST_BUF_NOT_RDY, 0,
			                           hostdata->c400_ctl_status,
			                           CSR_GATED_53C80_IRQ,
			                           CSR_GATED_53C80_IRQ, HZ / 64) < 0 ||
			    NCR5380_read(hostdata->c400_ctl_status) & CSR_HOST_BUF_NOT_RDY)
				break;
		}

		if (hostdata->io_port && hostdata->io_width == 2)
			insw(hostdata->io_port + hostdata->c400_host_buf,
			     dst + start, 64);
		else if (hostdata->io_port)
			insb(hostdata->io_port + hostdata->c400_host_buf,
			     dst + start, 128);
		else
			memcpy_fromio(dst + start,
				hostdata->io + NCR53C400_host_buffer, 128);
		start += 128;
	} while (start < len);

	residual = len - start;

	if (residual != 0) {
		/* 53c80 interrupt or transfer timeout. Reset 53c400 logic. */
		NCR5380_write(hostdata->c400_ctl_status, CSR_RESET);
		NCR5380_write(hostdata->c400_ctl_status, CSR_BASE);
	}
	wait_for_53c80_access(hostdata);

	if (residual == 0 && NCR5380_poll_politely(hostdata, BUS_AND_STATUS_REG,
	                                           BASR_END_DMA_TRANSFER,
	                                           BASR_END_DMA_TRANSFER,
	                                           HZ / 64) < 0)
		scmd_printk(KERN_ERR, hostdata->connected, "%s: End of DMA timeout\n",
		            __func__);

	hostdata->pdma_residual = residual;

	return 0;
}

/**
 * generic_NCR5380_psend - pseudo DMA send
 * @hostdata: scsi host private data
 * @src: buffer to read from
 * @len: transfer size
 *
 * Perform a pseudo DMA mode send to a 53C400 or equivalent device.
 */

static inline int generic_NCR5380_psend(struct NCR5380_hostdata *hostdata,
                                        unsigned char *src, int len)
{
	int residual;
	int start = 0;

	NCR5380_write(hostdata->c400_ctl_status, CSR_BASE);
	NCR5380_write(hostdata->c400_blk_cnt, len / 128);

	do {
		if (NCR5380_poll_politely2(hostdata, hostdata->c400_ctl_status,
		                           CSR_HOST_BUF_NOT_RDY, 0,
		                           hostdata->c400_ctl_status,
		                           CSR_GATED_53C80_IRQ,
		                           CSR_GATED_53C80_IRQ, HZ / 64) < 0 ||
		    NCR5380_read(hostdata->c400_ctl_status) & CSR_HOST_BUF_NOT_RDY) {
			/* Both 128 B buffers are in use */
			if (start >= 128)
				start -= 128;
			if (start >= 128)
				start -= 128;
			break;
		}

		if (start >= len && NCR5380_read(hostdata->c400_blk_cnt) == 0)
			break;

		if (NCR5380_read(hostdata->c400_ctl_status) & CSR_GATED_53C80_IRQ) {
			/* Host buffer is empty, other one is in use */
			if (start >= 128)
				start -= 128;
			break;
		}

		if (start >= len)
			continue;

		if (hostdata->io_port && hostdata->io_width == 2)
			outsw(hostdata->io_port + hostdata->c400_host_buf,
			      src + start, 64);
		else if (hostdata->io_port)
			outsb(hostdata->io_port + hostdata->c400_host_buf,
			      src + start, 128);
		else
			memcpy_toio(hostdata->io + NCR53C400_host_buffer,
			            src + start, 128);
		start += 128;
	} while (1);

	residual = len - start;

	if (residual != 0) {
		/* 53c80 interrupt or transfer timeout. Reset 53c400 logic. */
		NCR5380_write(hostdata->c400_ctl_status, CSR_RESET);
		NCR5380_write(hostdata->c400_ctl_status, CSR_BASE);
	}
	wait_for_53c80_access(hostdata);

	if (residual == 0) {
		if (NCR5380_poll_politely(hostdata, TARGET_COMMAND_REG,
		                          TCR_LAST_BYTE_SENT, TCR_LAST_BYTE_SENT,
		                          HZ / 64) < 0)
			scmd_printk(KERN_ERR, hostdata->connected,
			            "%s: Last Byte Sent timeout\n", __func__);

		if (NCR5380_poll_politely(hostdata, BUS_AND_STATUS_REG,
		                          BASR_END_DMA_TRANSFER, BASR_END_DMA_TRANSFER,
		                          HZ / 64) < 0)
			scmd_printk(KERN_ERR, hostdata->connected, "%s: End of DMA timeout\n",
			            __func__);
	}

	hostdata->pdma_residual = residual;

	return 0;
}

static int generic_NCR5380_dma_xfer_len(struct NCR5380_hostdata *hostdata,
                                        struct scsi_cmnd *cmd)
{
	int transfersize = cmd->SCp.this_residual;

	if (hostdata->flags & FLAG_NO_PSEUDO_DMA)
		return 0;

	/* 53C400 datasheet: non-modulo-128-byte transfers should use PIO */
	if (transfersize % 128)
		return 0;

	/* Limit PDMA send to 512 B to avoid random corruption on DTC3181E */
	if (hostdata->board == BOARD_DTC3181E &&
	    cmd->sc_data_direction == DMA_TO_DEVICE)
		transfersize = min(cmd->SCp.this_residual, 512);

	return min(transfersize, DMA_MAX_SIZE);
}

static int generic_NCR5380_dma_residual(struct NCR5380_hostdata *hostdata)
{
	return hostdata->pdma_residual;
}

/* Include the core driver code. */

#include "NCR5380.c"

static struct scsi_host_template driver_template = {
	.module			= THIS_MODULE,
	.proc_name		= DRV_MODULE_NAME,
	.name			= "Generic NCR5380/NCR53C400 SCSI",
	.info			= generic_NCR5380_info,
	.queuecommand		= generic_NCR5380_queue_command,
	.eh_abort_handler	= generic_NCR5380_abort,
	.eh_host_reset_handler	= generic_NCR5380_host_reset,
	.can_queue		= 16,
	.this_id		= 7,
	.sg_tablesize		= SG_ALL,
	.cmd_per_lun		= 2,
	.dma_boundary		= PAGE_SIZE - 1,
	.cmd_size		= NCR5380_CMD_SIZE,
	.max_sectors		= 128,
};

static int generic_NCR5380_isa_match(struct device *pdev, unsigned int ndev)
{
	int ret = generic_NCR5380_init_one(&driver_template, pdev, base[ndev],
	                                   irq[ndev], card[ndev]);
	if (ret) {
		if (base[ndev])
			printk(KERN_WARNING "Card not found at address 0x%03x\n",
			       base[ndev]);
		return 0;
	}

	return 1;
}

static int generic_NCR5380_isa_remove(struct device *pdev,
                                      unsigned int ndev)
{
	generic_NCR5380_release_resources(dev_get_drvdata(pdev));
	dev_set_drvdata(pdev, NULL);
	return 0;
}

static struct isa_driver generic_NCR5380_isa_driver = {
	.match		= generic_NCR5380_isa_match,
	.remove		= generic_NCR5380_isa_remove,
	.driver		= {
		.name	= DRV_MODULE_NAME
	},
};

#ifdef CONFIG_PNP
static const struct pnp_device_id generic_NCR5380_pnp_ids[] = {
	{ .id = "DTC436e", .driver_data = BOARD_DTC3181E },
	{ .id = "" }
};
MODULE_DEVICE_TABLE(pnp, generic_NCR5380_pnp_ids);

static int generic_NCR5380_pnp_probe(struct pnp_dev *pdev,
                                     const struct pnp_device_id *id)
{
	int base, irq;

	if (pnp_activate_dev(pdev) < 0)
		return -EBUSY;

	base = pnp_port_start(pdev, 0);
	irq = pnp_irq(pdev, 0);

	return generic_NCR5380_init_one(&driver_template, &pdev->dev, base, irq,
	                                id->driver_data);
}

static void generic_NCR5380_pnp_remove(struct pnp_dev *pdev)
{
	generic_NCR5380_release_resources(pnp_get_drvdata(pdev));
	pnp_set_drvdata(pdev, NULL);
}

static struct pnp_driver generic_NCR5380_pnp_driver = {
	.name		= DRV_MODULE_NAME,
	.id_table	= generic_NCR5380_pnp_ids,
	.probe		= generic_NCR5380_pnp_probe,
	.remove		= generic_NCR5380_pnp_remove,
};
#endif /* defined(CONFIG_PNP) */

static int pnp_registered, isa_registered;

static int __init generic_NCR5380_init(void)
{
	int ret = 0;

	/* compatibility with old-style parameters */
	if (irq[0] == -1 && base[0] == 0 && card[0] == -1) {
		irq[0] = ncr_irq;
		base[0] = ncr_addr;
		if (ncr_5380)
			card[0] = BOARD_NCR5380;
		if (ncr_53c400)
			card[0] = BOARD_NCR53C400;
		if (ncr_53c400a)
			card[0] = BOARD_NCR53C400A;
		if (dtc_3181e)
			card[0] = BOARD_DTC3181E;
		if (hp_c2502)
			card[0] = BOARD_HP_C2502;
	}

#ifdef CONFIG_PNP
	if (!pnp_register_driver(&generic_NCR5380_pnp_driver))
		pnp_registered = 1;
#endif
	ret = isa_register_driver(&generic_NCR5380_isa_driver, MAX_CARDS);
	if (!ret)
		isa_registered = 1;

	return (pnp_registered || isa_registered) ? 0 : ret;
}

static void __exit generic_NCR5380_exit(void)
{
#ifdef CONFIG_PNP
	if (pnp_registered)
		pnp_unregister_driver(&generic_NCR5380_pnp_driver);
#endif
	if (isa_registered)
		isa_unregister_driver(&generic_NCR5380_isa_driver);
}

module_init(generic_NCR5380_init);
module_exit(generic_NCR5380_exit);
