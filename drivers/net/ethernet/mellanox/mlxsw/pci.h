/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */
/* Copyright (c) 2016-2018 Mellanox Technologies. All rights reserved */

#ifndef _MLXSW_PCI_H
#define _MLXSW_PCI_H

#include <linux/pci.h>

#define PCI_DEVICE_ID_MELLANOX_SWITCHX2		0xc738
#define PCI_DEVICE_ID_MELLANOX_SPECTRUM		0xcb84
#define PCI_DEVICE_ID_MELLANOX_SPECTRUM2	0xcf6c
#define PCI_DEVICE_ID_MELLANOX_SPECTRUM3	0xcf70
#define PCI_DEVICE_ID_MELLANOX_SWITCHIB		0xcb20
#define PCI_DEVICE_ID_MELLANOX_SWITCHIB2	0xcf08

#if IS_ENABLED(CONFIG_MLXSW_PCI)

int mlxsw_pci_driver_register(struct pci_driver *pci_driver);
void mlxsw_pci_driver_unregister(struct pci_driver *pci_driver);

#else

static inline int
mlxsw_pci_driver_register(struct pci_driver *pci_driver)
{
	return 0;
}

<<<<<<< HEAD
static inline void
mlxsw_pci_driver_unregister(struct pci_driver *pci_driver)
{
}

#endif
=======
#define MLXSW_PCI_DOORBELL(offset, type_offset, num)	\
	((offset) + (type_offset) + (num) * 4)

#define MLXSW_PCI_CQS_MAX	96
#define MLXSW_PCI_EQS_COUNT	2
#define MLXSW_PCI_EQ_ASYNC_NUM	0
#define MLXSW_PCI_EQ_COMP_NUM	1

#define MLXSW_PCI_AQ_PAGES	8
#define MLXSW_PCI_AQ_SIZE	(MLXSW_PCI_PAGE_SIZE * MLXSW_PCI_AQ_PAGES)
#define MLXSW_PCI_WQE_SIZE	32 /* 32 bytes per element */
#define MLXSW_PCI_CQE_SIZE	16 /* 16 bytes per element */
#define MLXSW_PCI_EQE_SIZE	16 /* 16 bytes per element */
#define MLXSW_PCI_WQE_COUNT	(MLXSW_PCI_AQ_SIZE / MLXSW_PCI_WQE_SIZE)
#define MLXSW_PCI_CQE_COUNT	(MLXSW_PCI_AQ_SIZE / MLXSW_PCI_CQE_SIZE)
#define MLXSW_PCI_EQE_COUNT	(MLXSW_PCI_AQ_SIZE / MLXSW_PCI_EQE_SIZE)
#define MLXSW_PCI_EQE_UPDATE_COUNT	0x80

#define MLXSW_PCI_WQE_SG_ENTRIES	3
#define MLXSW_PCI_WQE_TYPE_ETHERNET	0xA

/* pci_wqe_c
 * If set it indicates that a completion should be reported upon
 * execution of this descriptor.
 */
MLXSW_ITEM32(pci, wqe, c, 0x00, 31, 1);

/* pci_wqe_lp
 * Local Processing, set if packet should be processed by the local
 * switch hardware:
 * For Ethernet EMAD (Direct Route and non Direct Route) -
 * must be set if packet destination is local device
 * For InfiniBand CTL - must be set if packet destination is local device
 * Otherwise it must be clear
 * Local Process packets must not exceed the size of 2K (including payload
 * and headers).
 */
MLXSW_ITEM32(pci, wqe, lp, 0x00, 30, 1);

/* pci_wqe_type
 * Packet type.
 */
MLXSW_ITEM32(pci, wqe, type, 0x00, 23, 4);

/* pci_wqe_byte_count
 * Size of i-th scatter/gather entry, 0 if entry is unused.
 */
MLXSW_ITEM16_INDEXED(pci, wqe, byte_count, 0x02, 0, 14, 0x02, 0x00, false);

/* pci_wqe_address
 * Physical address of i-th scatter/gather entry.
 * Gather Entries must be 2Byte aligned.
 */
MLXSW_ITEM64_INDEXED(pci, wqe, address, 0x08, 0, 64, 0x8, 0x0, false);

/* pci_cqe_lag
 * Packet arrives from a port which is a LAG
 */
MLXSW_ITEM32(pci, cqe, lag, 0x00, 23, 1);

/* pci_cqe_system_port
 * When lag=0: System port on which the packet was received
 * When lag=1:
 * bits [15:4] LAG ID on which the packet was received
 * bits [3:0] sub_port on which the packet was received
 */
MLXSW_ITEM32(pci, cqe, system_port, 0x00, 0, 16);

/* pci_cqe_wqe_counter
 * WQE count of the WQEs completed on the associated dqn
 */
MLXSW_ITEM32(pci, cqe, wqe_counter, 0x04, 16, 16);

/* pci_cqe_byte_count
 * Byte count of received packets including additional two
 * Reserved Bytes that are append to the end of the frame.
 * Reserved for Send CQE.
 */
MLXSW_ITEM32(pci, cqe, byte_count, 0x04, 0, 14);

/* pci_cqe_trap_id
 * Trap ID that captured the packet.
 */
MLXSW_ITEM32(pci, cqe, trap_id, 0x08, 0, 8);

/* pci_cqe_crc
 * Length include CRC. Indicates the length field includes
 * the packet's CRC.
 */
MLXSW_ITEM32(pci, cqe, crc, 0x0C, 8, 1);

/* pci_cqe_e
 * CQE with Error.
 */
MLXSW_ITEM32(pci, cqe, e, 0x0C, 7, 1);

/* pci_cqe_sr
 * 1 - Send Queue
 * 0 - Receive Queue
 */
MLXSW_ITEM32(pci, cqe, sr, 0x0C, 6, 1);

/* pci_cqe_dqn
 * Descriptor Queue (DQ) Number.
 */
MLXSW_ITEM32(pci, cqe, dqn, 0x0C, 1, 5);

/* pci_cqe_owner
 * Ownership bit.
 */
MLXSW_ITEM32(pci, cqe, owner, 0x0C, 0, 1);

/* pci_eqe_event_type
 * Event type.
 */
MLXSW_ITEM32(pci, eqe, event_type, 0x0C, 24, 8);
#define MLXSW_PCI_EQE_EVENT_TYPE_COMP	0x00
#define MLXSW_PCI_EQE_EVENT_TYPE_CMD	0x0A

/* pci_eqe_event_sub_type
 * Event type.
 */
MLXSW_ITEM32(pci, eqe, event_sub_type, 0x0C, 16, 8);

/* pci_eqe_cqn
 * Completion Queue that triggeret this EQE.
 */
MLXSW_ITEM32(pci, eqe, cqn, 0x0C, 8, 7);

/* pci_eqe_owner
 * Ownership bit.
 */
MLXSW_ITEM32(pci, eqe, owner, 0x0C, 0, 1);

/* pci_eqe_cmd_token
 * Command completion event - token
 */
MLXSW_ITEM32(pci, eqe, cmd_token, 0x00, 16, 16);

/* pci_eqe_cmd_status
 * Command completion event - status
 */
MLXSW_ITEM32(pci, eqe, cmd_status, 0x00, 0, 8);

/* pci_eqe_cmd_out_param_h
 * Command completion event - output parameter - higher part
 */
MLXSW_ITEM32(pci, eqe, cmd_out_param_h, 0x04, 0, 32);

/* pci_eqe_cmd_out_param_l
 * Command completion event - output parameter - lower part
 */
MLXSW_ITEM32(pci, eqe, cmd_out_param_l, 0x08, 0, 32);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

#endif
