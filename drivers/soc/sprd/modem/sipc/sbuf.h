/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SBUF_H
#define __SBUF_H

/* flag for CMD/DONE msg type */
#define SMSG_CMD_SBUF_INIT	0x0001
#define SMSG_DONE_SBUF_INIT	0x0002

/* flag for EVENT msg type */
#define SMSG_EVENT_SBUF_WRPTR	0x0001
#define SMSG_EVENT_SBUF_RDPTR	0x0002

#define SIPC_DEBUG_SBUF_RDWT_OWNER

/* ring buf header */
struct sbuf_ring_header {
	/* send-buffer info */
	u32		txbuf_addr;
	u32		txbuf_size;
	u32		txbuf_rdptr;
	u32		txbuf_wrptr;

	/* recv-buffer info */
	u32		rxbuf_addr;
	u32		rxbuf_size;
	u32		rxbuf_rdptr;
	u32		rxbuf_wrptr;
};

/* sbuf_mem is the structure of smem for rings */
struct sbuf_smem_header {
	u32		ringnr;

	struct sbuf_ring_header	headers[0];
};

struct sbuf_ring {
	/* tx/rx buffer info */
	volatile struct sbuf_ring_header	*header;

	void			*txbuf_virt;
	void			*rxbuf_virt;

	/* send/recv wait queue */
	wait_queue_head_t	txwait;
	wait_queue_head_t	rxwait;

#if defined(SIPC_DEBUG_SBUF_RDWT_OWNER)
	/* record all  task histoy */
	struct list_head	tx_list;
	struct list_head	rx_list;
#endif

	/* send/recv mutex */
	struct mutex		txlock;
	struct mutex		rxlock;

	struct wakeup_source	tx_wake_lock;
	struct wakeup_source	rx_wake_lock;
	char			tx_wakelock_name[20];
	char			rx_wakelock_name[20];

	u8			tx_wakelock_state;
	u8			rx_wakelock_state;
	u16			need_wake_lock;

	void			(*handler)(int event, void *data);
	void			*data;
};

#define SBUF_STATE_IDLE		0
#define SBUF_STATE_READY	1

struct sbuf_mgr {
	u8			dst;
	u8			channel;
	u16			smem;
	u32			state;

	void			*smem_virt;
	u32			smem_addr;
	u32			smem_size;
	u32			mapped_smem_addr;
	u32			ringnr;
	struct sbuf_ring	*rings;
	struct task_struct	*thread;
};

#endif
