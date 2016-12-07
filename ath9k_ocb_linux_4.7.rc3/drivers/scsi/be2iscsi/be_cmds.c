/**
 * Copyright (C) 2005 - 2015 Emulex
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.  The full GNU General
 * Public License is included in this distribution in the file called COPYING.
 *
 * Contact Information:
 * linux-drivers@avagotech.com
 *
 * Emulex
 * 3333 Susan Street
 * Costa Mesa, CA 92626
 */

#include <scsi/iscsi_proto.h>

#include "be_main.h"
#include "be.h"
#include "be_mgmt.h"

int beiscsi_pci_soft_reset(struct beiscsi_hba *phba)
{
	u32 sreset;
	u8 *pci_reset_offset = 0;
	u8 *pci_online0_offset = 0;
	u8 *pci_online1_offset = 0;
	u32 pconline0 = 0;
	u32 pconline1 = 0;
	u32 i;

	pci_reset_offset = (u8 *)phba->pci_va + BE2_SOFT_RESET;
	pci_online0_offset = (u8 *)phba->pci_va + BE2_PCI_ONLINE0;
	pci_online1_offset = (u8 *)phba->pci_va + BE2_PCI_ONLINE1;
	sreset = readl((void *)pci_reset_offset);
	sreset |= BE2_SET_RESET;
	writel(sreset, (void *)pci_reset_offset);

	i = 0;
	while (sreset & BE2_SET_RESET) {
		if (i > 64)
			break;
		msleep(100);
		sreset = readl((void *)pci_reset_offset);
		i++;
	}

	if (sreset & BE2_SET_RESET) {
		printk(KERN_ERR DRV_NAME
		       " Soft Reset  did not deassert\n");
		return -EIO;
	}
	pconline1 = BE2_MPU_IRAM_ONLINE;
	writel(pconline0, (void *)pci_online0_offset);
	writel(pconline1, (void *)pci_online1_offset);

	sreset |= BE2_SET_RESET;
	writel(sreset, (void *)pci_reset_offset);

	i = 0;
	while (sreset & BE2_SET_RESET) {
		if (i > 64)
			break;
		msleep(1);
		sreset = readl((void *)pci_reset_offset);
		i++;
	}
	if (sreset & BE2_SET_RESET) {
		printk(KERN_ERR DRV_NAME
		       " MPU Online Soft Reset did not deassert\n");
		return -EIO;
	}
	return 0;
}

int be_chk_reset_complete(struct beiscsi_hba *phba)
{
	unsigned int num_loop;
	u8 *mpu_sem = 0;
	u32 status;

	num_loop = 1000;
	mpu_sem = (u8 *)phba->csr_va + MPU_EP_SEMAPHORE;
	msleep(5000);

	while (num_loop) {
		status = readl((void *)mpu_sem);

		if ((status & 0x80000000) || (status & 0x0000FFFF) == 0xC000)
			break;
		msleep(60);
		num_loop--;
	}

	if ((status & 0x80000000) || (!num_loop)) {
		beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_INIT,
			    "BC_%d : Failed in be_chk_reset_complete"
			    "status = 0x%x\n", status);
		return -EIO;
	}

	return 0;
}

unsigned int alloc_mcc_tag(struct beiscsi_hba *phba)
{
	unsigned int tag = 0;

	spin_lock(&phba->ctrl.mcc_lock);
	if (phba->ctrl.mcc_tag_available) {
		tag = phba->ctrl.mcc_tag[phba->ctrl.mcc_alloc_index];
		phba->ctrl.mcc_tag[phba->ctrl.mcc_alloc_index] = 0;
		phba->ctrl.mcc_tag_status[tag] = 0;
		phba->ctrl.ptag_state[tag].tag_state = 0;
	}
	if (tag) {
		phba->ctrl.mcc_tag_available--;
		if (phba->ctrl.mcc_alloc_index == (MAX_MCC_CMD - 1))
			phba->ctrl.mcc_alloc_index = 0;
		else
			phba->ctrl.mcc_alloc_index++;
	}
	spin_unlock(&phba->ctrl.mcc_lock);
	return tag;
}

struct be_mcc_wrb *alloc_mcc_wrb(struct beiscsi_hba *phba,
				 unsigned int *ref_tag)
{
	struct be_queue_info *mccq = &phba->ctrl.mcc_obj.q;
	struct be_mcc_wrb *wrb = NULL;
	unsigned int tag;

	spin_lock_bh(&phba->ctrl.mcc_lock);
	if (mccq->used == mccq->len) {
		beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_INIT |
			    BEISCSI_LOG_CONFIG | BEISCSI_LOG_MBOX,
			    "BC_%d : MCC queue full: WRB used %u tag avail %u\n",
			    mccq->used, phba->ctrl.mcc_tag_available);
		goto alloc_failed;
	}

	if (!phba->ctrl.mcc_tag_available)
		goto alloc_failed;

	tag = phba->ctrl.mcc_tag[phba->ctrl.mcc_alloc_index];
	if (!tag) {
		beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_INIT |
			    BEISCSI_LOG_CONFIG | BEISCSI_LOG_MBOX,
			    "BC_%d : MCC tag 0 allocated: tag avail %u alloc index %u\n",
			    phba->ctrl.mcc_tag_available,
			    phba->ctrl.mcc_alloc_index);
		goto alloc_failed;
	}

	/* return this tag for further reference */
	*ref_tag = tag;
	phba->ctrl.mcc_tag[phba->ctrl.mcc_alloc_index] = 0;
	phba->ctrl.mcc_tag_status[tag] = 0;
	phba->ctrl.ptag_state[tag].tag_state = 0;
	phba->ctrl.mcc_tag_available--;
	if (phba->ctrl.mcc_alloc_index == (MAX_MCC_CMD - 1))
		phba->ctrl.mcc_alloc_index = 0;
	else
		phba->ctrl.mcc_alloc_index++;

	wrb = queue_head_node(mccq);
	memset(wrb, 0, sizeof(*wrb));
	wrb->tag0 = tag;
	wrb->tag0 |= (mccq->head << MCC_Q_WRB_IDX_SHIFT) & MCC_Q_WRB_IDX_MASK;
	queue_head_inc(mccq);
	mccq->used++;

alloc_failed:
	spin_unlock_bh(&phba->ctrl.mcc_lock);
	return wrb;
}

void free_mcc_wrb(struct be_ctrl_info *ctrl, unsigned int tag)
{
	struct be_queue_info *mccq = &ctrl->mcc_obj.q;

	spin_lock_bh(&ctrl->mcc_lock);
	tag = tag & MCC_Q_CMD_TAG_MASK;
	ctrl->mcc_tag[ctrl->mcc_free_index] = tag;
	if (ctrl->mcc_free_index == (MAX_MCC_CMD - 1))
		ctrl->mcc_free_index = 0;
	else
		ctrl->mcc_free_index++;
	ctrl->mcc_tag_available++;
	mccq->used--;
	spin_unlock_bh(&ctrl->mcc_lock);
}

/**
 * beiscsi_fail_session(): Closing session with appropriate error
 * @cls_session: ptr to session
 **/
void beiscsi_fail_session(struct iscsi_cls_session *cls_session)
{
	iscsi_session_failure(cls_session->dd_data, ISCSI_ERR_CONN_FAILED);
}

/*
 * beiscsi_mccq_compl_wait()- Process completion in MCC CQ
 * @phba: Driver private structure
 * @tag: Tag for the MBX Command
 * @wrb: the WRB used for the MBX Command
 * @mbx_cmd_mem: ptr to memory allocated for MBX Cmd
 *
 * Waits for MBX completion with the passed TAG.
 *
 * return
 * Success: 0
 * Failure: Non-Zero
 **/
int beiscsi_mccq_compl_wait(struct beiscsi_hba *phba,
			    uint32_t tag, struct be_mcc_wrb **wrb,
			    struct be_dma_mem *mbx_cmd_mem)
{
	int rc = 0;
	uint32_t mcc_tag_status;
	uint16_t status = 0, addl_status = 0, wrb_num = 0;
	struct be_mcc_wrb *temp_wrb;
	struct be_cmd_req_hdr *mbx_hdr;
	struct be_cmd_resp_hdr *mbx_resp_hdr;
	struct be_queue_info *mccq = &phba->ctrl.mcc_obj.q;

	if (beiscsi_error(phba))
		return -EPERM;

	/* wait for the mccq completion */
	rc = wait_event_interruptible_timeout(
				phba->ctrl.mcc_wait[tag],
				phba->ctrl.mcc_tag_status[tag],
				msecs_to_jiffies(
				BEISCSI_HOST_MBX_TIMEOUT));
	/**
	 * If MBOX cmd timeout expired, tag and resource allocated
	 * for cmd is not freed until FW returns completion.
	 */
	if (rc <= 0) {
		struct be_dma_mem *tag_mem;

		/**
		 * PCI/DMA memory allocated and posted in non-embedded mode
		 * will have mbx_cmd_mem != NULL.
		 * Save virtual and bus addresses for the command so that it
		 * can be freed later.
		 **/
		tag_mem = &phba->ctrl.ptag_state[tag].tag_mem_state;
		if (mbx_cmd_mem) {
			tag_mem->size = mbx_cmd_mem->size;
			tag_mem->va = mbx_cmd_mem->va;
			tag_mem->dma = mbx_cmd_mem->dma;
		} else
			tag_mem->size = 0;

		/* first make tag_mem_state visible to all */
		wmb();
		set_bit(MCC_TAG_STATE_TIMEOUT,
				&phba->ctrl.ptag_state[tag].tag_state);

		beiscsi_log(phba, KERN_ERR,
			    BEISCSI_LOG_INIT | BEISCSI_LOG_EH |
			    BEISCSI_LOG_CONFIG,
			    "BC_%d : MBX Cmd Completion timed out\n");
		return -EBUSY;
	}

	rc = 0;
	mcc_tag_status = phba->ctrl.mcc_tag_status[tag];
	status = (mcc_tag_status & CQE_STATUS_MASK);
	addl_status = ((mcc_tag_status & CQE_STATUS_ADDL_MASK) >>
			CQE_STATUS_ADDL_SHIFT);

	if (mbx_cmd_mem) {
		mbx_hdr = (struct be_cmd_req_hdr *)mbx_cmd_mem->va;
	} else {
		wrb_num = (mcc_tag_status & CQE_STATUS_WRB_MASK) >>
			   CQE_STATUS_WRB_SHIFT;
		temp_wrb = (struct be_mcc_wrb *)queue_get_wrb(mccq, wrb_num);
		mbx_hdr = embedded_payload(temp_wrb);

		if (wrb)
			*wrb = temp_wrb;
	}

	if (status || addl_status) {
		beiscsi_log(phba, KERN_WARNING,
			    BEISCSI_LOG_INIT | BEISCSI_LOG_EH |
			    BEISCSI_LOG_CONFIG,
			    "BC_%d : MBX Cmd Failed for "
			    "Subsys : %d Opcode : %d with "
			    "Status : %d and Extd_Status : %d\n",
			    mbx_hdr->subsystem,
			    mbx_hdr->opcode,
			    status, addl_status);
		rc = -EIO;
		if (status == MCC_STATUS_INSUFFICIENT_BUFFER) {
			mbx_resp_hdr = (struct be_cmd_resp_hdr *) mbx_hdr;
			beiscsi_log(phba, KERN_WARNING,
				    BEISCSI_LOG_INIT | BEISCSI_LOG_EH |
				    BEISCSI_LOG_CONFIG,
				    "BC_%d : Insufficient Buffer Error "
				    "Resp_Len : %d Actual_Resp_Len : %d\n",
				    mbx_resp_hdr->response_length,
				    mbx_resp_hdr->actual_resp_len);
			rc = -EAGAIN;
		}
	}

	free_mcc_wrb(&phba->ctrl, tag);
	return rc;
}

/*
 * beiscsi_process_mbox_compl()- Check the MBX completion status
 * @ctrl: Function specific MBX data structure
 * @compl: Completion status of MBX Command
 *
 * Check for the MBX completion status when BMBX method used
 *
 * return
 * Success: Zero
 * Failure: Non-Zero
 **/
static int beiscsi_process_mbox_compl(struct be_ctrl_info *ctrl,
				      struct be_mcc_compl *compl)
{
	u16 compl_status, extd_status;
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	struct be_cmd_req_hdr *hdr = embedded_payload(wrb);
	struct be_cmd_resp_hdr *resp_hdr;

	/**
	 * To check if valid bit is set, check the entire word as we don't know
	 * the endianness of the data (old entry is host endian while a new
	 * entry is little endian)
	 */
	if (!compl->flags) {
		beiscsi_log(phba, KERN_ERR,
				BEISCSI_LOG_CONFIG | BEISCSI_LOG_MBOX,
				"BC_%d : BMBX busy, no completion\n");
		return -EBUSY;
	}
	compl->flags = le32_to_cpu(compl->flags);
	WARN_ON((compl->flags & CQE_FLAGS_VALID_MASK) == 0);

	/**
	 * Just swap the status to host endian;
	 * mcc tag is opaquely copied from mcc_wrb.
	 */
	be_dws_le_to_cpu(compl, 4);
	compl_status = (compl->status >> CQE_STATUS_COMPL_SHIFT) &
		CQE_STATUS_COMPL_MASK;
	extd_status = (compl->status >> CQE_STATUS_EXTD_SHIFT) &
		CQE_STATUS_EXTD_MASK;
	/* Need to reset the entire word that houses the valid bit */
	compl->flags = 0;

	if (compl_status == MCC_STATUS_SUCCESS)
		return 0;

	beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_CONFIG | BEISCSI_LOG_MBOX,
		    "BC_%d : error in cmd completion: Subsystem : %d Opcode : %d status(compl/extd)=%d/%d\n",
		    hdr->subsystem, hdr->opcode, compl_status, extd_status);

	if (compl_status == MCC_STATUS_INSUFFICIENT_BUFFER) {
		/* if status is insufficient buffer, check the length */
		resp_hdr = (struct be_cmd_resp_hdr *) hdr;
		if (resp_hdr->response_length)
			return 0;
	}
	return -EINVAL;
}

static void beiscsi_process_async_link(struct beiscsi_hba *phba,
				       struct be_mcc_compl *compl)
{
	struct be_async_event_link_state *evt;

	evt = (struct be_async_event_link_state *)compl;

	phba->port_speed = evt->port_speed;
	/**
	 * Check logical link status in ASYNC event.
	 * This has been newly introduced in SKH-R Firmware 10.0.338.45.
	 **/
	if (evt->port_link_status & BE_ASYNC_LINK_UP_MASK) {
		phba->state = BE_ADAPTER_LINK_UP | BE_ADAPTER_CHECK_BOOT;
		phba->get_boot = BE_GET_BOOT_RETRIES;
		__beiscsi_log(phba, KERN_ERR,
			      "BC_%d : Link Up on Port %d tag 0x%x\n",
			      evt->physical_port, evt->event_tag);
	} else {
		phba->state = BE_ADAPTER_LINK_DOWN;
		__beiscsi_log(phba, KERN_ERR,
			      "BC_%d : Link Down on Port %d tag 0x%x\n",
			      evt->physical_port, evt->event_tag);
		iscsi_host_for_each_session(phba->shost,
					    beiscsi_fail_session);
	}
}

static char *beiscsi_port_misconf_event_msg[] = {
	"Physical Link is functional.",
	"Optics faulted/incorrectly installed/not installed - Reseat optics, if issue not resolved, replace.",
	"Optics of two types installed - Remove one optic or install matching pair of optics.",
	"Incompatible optics - Replace with compatible optics for card to function.",
	"Unqualified optics - Replace with Avago optics for Warranty and Technical Support.",
	"Uncertified optics - Replace with Avago Certified optics to enable link operation."
};

static void beiscsi_process_async_sli(struct beiscsi_hba *phba,
				      struct be_mcc_compl *compl)
{
	struct be_async_event_sli *async_sli;
	u8 evt_type, state, old_state, le;
	char *sev = KERN_WARNING;
	char *msg = NULL;

	evt_type = compl->flags >> ASYNC_TRAILER_EVENT_TYPE_SHIFT;
	evt_type &= ASYNC_TRAILER_EVENT_TYPE_MASK;

	/* processing only MISCONFIGURED physical port event */
	if (evt_type != ASYNC_SLI_EVENT_TYPE_MISCONFIGURED)
		return;

	async_sli = (struct be_async_event_sli *)compl;
	state = async_sli->event_data1 >>
		 (phba->fw_config.phys_port * 8) & 0xff;
	le = async_sli->event_data2 >>
		 (phba->fw_config.phys_port * 8) & 0xff;

	old_state = phba->optic_state;
	phba->optic_state = state;

	if (state >= ARRAY_SIZE(beiscsi_port_misconf_event_msg)) {
		/* fw is reporting a state we don't know, log and return */
		__beiscsi_log(phba, KERN_ERR,
			    "BC_%d : Port %c: Unrecognized optic state 0x%x\n",
			    phba->port_name, async_sli->event_data1);
		return;
	}

	if (ASYNC_SLI_LINK_EFFECT_VALID(le)) {
		/* log link effect for unqualified-4, uncertified-5 optics */
		if (state > 3)
			msg = (ASYNC_SLI_LINK_EFFECT_STATE(le)) ?
				" Link is non-operational." :
				" Link is operational.";
		/* 1 - info */
		if (ASYNC_SLI_LINK_EFFECT_SEV(le) == 1)
			sev = KERN_INFO;
		/* 2 - error */
		if (ASYNC_SLI_LINK_EFFECT_SEV(le) == 2)
			sev = KERN_ERR;
	}

	if (old_state != phba->optic_state)
		__beiscsi_log(phba, sev, "BC_%d : Port %c: %s%s\n",
			      phba->port_name,
			      beiscsi_port_misconf_event_msg[state],
			      !msg ? "" : msg);
}

void beiscsi_process_async_event(struct beiscsi_hba *phba,
				struct be_mcc_compl *compl)
{
	char *sev = KERN_INFO;
	u8 evt_code;

	/* interpret flags as an async trailer */
	evt_code = compl->flags >> ASYNC_TRAILER_EVENT_CODE_SHIFT;
	evt_code &= ASYNC_TRAILER_EVENT_CODE_MASK;
	switch (evt_code) {
	case ASYNC_EVENT_CODE_LINK_STATE:
		beiscsi_process_async_link(phba, compl);
		break;
	case ASYNC_EVENT_CODE_ISCSI:
		phba->state |= BE_ADAPTER_CHECK_BOOT;
		phba->get_boot = BE_GET_BOOT_RETRIES;
		sev = KERN_ERR;
		break;
	case ASYNC_EVENT_CODE_SLI:
		beiscsi_process_async_sli(phba, compl);
		break;
	default:
		/* event not registered */
		sev = KERN_ERR;
	}

	beiscsi_log(phba, sev, BEISCSI_LOG_CONFIG | BEISCSI_LOG_MBOX,
		    "BC_%d : ASYNC Event %x: status 0x%08x flags 0x%08x\n",
		    evt_code, compl->status, compl->flags);
}

int beiscsi_process_mcc_compl(struct be_ctrl_info *ctrl,
			      struct be_mcc_compl *compl)
{
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	u16 compl_status, extd_status;
	struct be_dma_mem *tag_mem;
	unsigned int tag, wrb_idx;

	be_dws_le_to_cpu(compl, 4);
	tag = (compl->tag0 & MCC_Q_CMD_TAG_MASK);
	wrb_idx = (compl->tag0 & CQE_STATUS_WRB_MASK) >> CQE_STATUS_WRB_SHIFT;

	if (!test_bit(MCC_TAG_STATE_RUNNING,
		      &ctrl->ptag_state[tag].tag_state)) {
		beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_MBOX |
			    BEISCSI_LOG_INIT | BEISCSI_LOG_CONFIG,
			    "BC_%d : MBX cmd completed but not posted\n");
		return 0;
	}

	if (test_bit(MCC_TAG_STATE_TIMEOUT, &ctrl->ptag_state[tag].tag_state)) {
		beiscsi_log(phba, KERN_WARNING,
			    BEISCSI_LOG_MBOX | BEISCSI_LOG_INIT |
			    BEISCSI_LOG_CONFIG,
			    "BC_%d : MBX Completion for timeout Command from FW\n");
		/**
		 * Check for the size before freeing resource.
		 * Only for non-embedded cmd, PCI resource is allocated.
		 **/
		tag_mem = &ctrl->ptag_state[tag].tag_mem_state;
		if (tag_mem->size)
			pci_free_consistent(ctrl->pdev, tag_mem->size,
					tag_mem->va, tag_mem->dma);
		free_mcc_wrb(ctrl, tag);
		return 0;
	}

	compl_status = (compl->status >> CQE_STATUS_COMPL_SHIFT) &
		       CQE_STATUS_COMPL_MASK;
	extd_status = (compl->status >> CQE_STATUS_EXTD_SHIFT) &
		      CQE_STATUS_EXTD_MASK;
	/* The ctrl.mcc_tag_status[tag] is filled with
	 * [31] = valid, [30:24] = Rsvd, [23:16] = wrb, [15:8] = extd_status,
	 * [7:0] = compl_status
	 */
	ctrl->mcc_tag_status[tag] = CQE_VALID_MASK;
	ctrl->mcc_tag_status[tag] |= (wrb_idx << CQE_STATUS_WRB_SHIFT);
	ctrl->mcc_tag_status[tag] |= (extd_status << CQE_STATUS_ADDL_SHIFT) &
				     CQE_STATUS_ADDL_MASK;
	ctrl->mcc_tag_status[tag] |= (compl_status & CQE_STATUS_MASK);

	/* write ordering forced in wake_up_interruptible */
	clear_bit(MCC_TAG_STATE_RUNNING, &ctrl->ptag_state[tag].tag_state);
	wake_up_interruptible(&ctrl->mcc_wait[tag]);
	return 0;
}

/*
 * be_mcc_compl_poll()- Wait for MBX completion
 * @phba: driver private structure
 *
 * Wait till no more pending mcc requests are present
 *
 * return
 * Success: 0
 * Failure: Non-Zero
 *
 **/
int be_mcc_compl_poll(struct beiscsi_hba *phba, unsigned int tag)
{
	struct be_ctrl_info *ctrl = &phba->ctrl;
	int i;

	if (!test_bit(MCC_TAG_STATE_RUNNING,
		      &ctrl->ptag_state[tag].tag_state)) {
		beiscsi_log(phba, KERN_ERR,
			    BEISCSI_LOG_CONFIG | BEISCSI_LOG_MBOX,
			    "BC_%d: tag %u state not running\n", tag);
		return 0;
	}
	for (i = 0; i < mcc_timeout; i++) {
		if (beiscsi_error(phba))
			return -EIO;

		beiscsi_process_mcc_cq(phba);
		/* after polling, wrb and tag need to be released */
		if (!test_bit(MCC_TAG_STATE_RUNNING,
			      &ctrl->ptag_state[tag].tag_state)) {
			free_mcc_wrb(ctrl, tag);
			break;
		}
		udelay(100);
	}

	if (i < mcc_timeout)
		return 0;

	beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_CONFIG | BEISCSI_LOG_MBOX,
		    "BC_%d : FW Timed Out\n");
	phba->fw_timeout = true;
	beiscsi_ue_detect(phba);
	return -EBUSY;
}

void be_mcc_notify(struct beiscsi_hba *phba, unsigned int tag)
{
	struct be_queue_info *mccq = &phba->ctrl.mcc_obj.q;
	u32 val = 0;

	set_bit(MCC_TAG_STATE_RUNNING, &phba->ctrl.ptag_state[tag].tag_state);
	val |= mccq->id & DB_MCCQ_RING_ID_MASK;
	val |= 1 << DB_MCCQ_NUM_POSTED_SHIFT;
	/* make request available for DMA */
	wmb();
	iowrite32(val, phba->db_va + DB_MCCQ_OFFSET);
}

/*
 * be_mbox_db_ready_poll()- Check ready status
 * @ctrl: Function specific MBX data structure
 *
 * Check for the ready status of FW to send BMBX
 * commands to adapter.
 *
 * return
 * Success: 0
 * Failure: Non-Zero
 **/
static int be_mbox_db_ready_poll(struct be_ctrl_info *ctrl)
{
	/* wait 30s for generic non-flash MBOX operation */
#define BEISCSI_MBX_RDY_BIT_TIMEOUT	30000
	void __iomem *db = ctrl->db + MPU_MAILBOX_DB_OFFSET;
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	unsigned long timeout;
	u32 ready;

	/*
	 * This BMBX busy wait path is used during init only.
	 * For the commands executed during init, 5s should suffice.
	 */
	timeout = jiffies + msecs_to_jiffies(BEISCSI_MBX_RDY_BIT_TIMEOUT);
	do {
		if (beiscsi_error(phba))
			return -EIO;

		ready = ioread32(db);
		if (ready == 0xffffffff)
			return -EIO;

		ready &= MPU_MAILBOX_DB_RDY_MASK;
		if (ready)
			return 0;

		if (time_after(jiffies, timeout))
			break;
		msleep(20);
	} while (!ready);

	beiscsi_log(phba, KERN_ERR,
			BEISCSI_LOG_CONFIG | BEISCSI_LOG_MBOX,
			"BC_%d : FW Timed Out\n");

	phba->fw_timeout = true;
	beiscsi_ue_detect(phba);

	return -EBUSY;
}

/*
 * be_mbox_notify: Notify adapter of new BMBX command
 * @ctrl: Function specific MBX data structure
 *
 * Ring doorbell to inform adapter of a BMBX command
 * to process
 *
 * return
 * Success: 0
 * Failure: Non-Zero
 **/
int be_mbox_notify(struct be_ctrl_info *ctrl)
{
	int status;
	u32 val = 0;
	void __iomem *db = ctrl->db + MPU_MAILBOX_DB_OFFSET;
	struct be_dma_mem *mbox_mem = &ctrl->mbox_mem;
	struct be_mcc_mailbox *mbox = mbox_mem->va;

	status = be_mbox_db_ready_poll(ctrl);
	if (status)
		return status;

	val &= ~MPU_MAILBOX_DB_RDY_MASK;
	val |= MPU_MAILBOX_DB_HI_MASK;
	val |= (upper_32_bits(mbox_mem->dma) >> 2) << 2;
	iowrite32(val, db);

	status = be_mbox_db_ready_poll(ctrl);
	if (status)
		return status;

	val = 0;
	val &= ~MPU_MAILBOX_DB_RDY_MASK;
	val &= ~MPU_MAILBOX_DB_HI_MASK;
	val |= (u32) (mbox_mem->dma >> 4) << 2;
	iowrite32(val, db);

	status = be_mbox_db_ready_poll(ctrl);
	if (status)
		return status;

	/* RDY is set; small delay before CQE read. */
	udelay(1);

	status = beiscsi_process_mbox_compl(ctrl, &mbox->compl);
	return status;
}

void be_wrb_hdr_prepare(struct be_mcc_wrb *wrb, int payload_len,
				bool embedded, u8 sge_cnt)
{
	if (embedded)
		wrb->embedded |= MCC_WRB_EMBEDDED_MASK;
	else
		wrb->embedded |= (sge_cnt & MCC_WRB_SGE_CNT_MASK) <<
						MCC_WRB_SGE_CNT_SHIFT;
	wrb->payload_length = payload_len;
	be_dws_cpu_to_le(wrb, 8);
}

void be_cmd_hdr_prepare(struct be_cmd_req_hdr *req_hdr,
			u8 subsystem, u8 opcode, int cmd_len)
{
	req_hdr->opcode = opcode;
	req_hdr->subsystem = subsystem;
	req_hdr->request_length = cpu_to_le32(cmd_len - sizeof(*req_hdr));
	req_hdr->timeout = BEISCSI_FW_MBX_TIMEOUT;
}

static void be_cmd_page_addrs_prepare(struct phys_addr *pages, u32 max_pages,
							struct be_dma_mem *mem)
{
	int i, buf_pages;
	u64 dma = (u64) mem->dma;

	buf_pages = min(PAGES_4K_SPANNED(mem->va, mem->size), max_pages);
	for (i = 0; i < buf_pages; i++) {
		pages[i].lo = cpu_to_le32(dma & 0xFFFFFFFF);
		pages[i].hi = cpu_to_le32(upper_32_bits(dma));
		dma += PAGE_SIZE_4K;
	}
}

static u32 eq_delay_to_mult(u32 usec_delay)
{
#define MAX_INTR_RATE 651042
	const u32 round = 10;
	u32 multiplier;

	if (usec_delay == 0)
		multiplier = 0;
	else {
		u32 interrupt_rate = 1000000 / usec_delay;
		if (interrupt_rate == 0)
			multiplier = 1023;
		else {
			multiplier = (MAX_INTR_RATE - interrupt_rate) * round;
			multiplier /= interrupt_rate;
			multiplier = (multiplier + round / 2) / round;
			multiplier = min(multiplier, (u32) 1023);
		}
	}
	return multiplier;
}

struct be_mcc_wrb *wrb_from_mbox(struct be_dma_mem *mbox_mem)
{
	return &((struct be_mcc_mailbox *)(mbox_mem->va))->wrb;
}

int beiscsi_cmd_eq_create(struct be_ctrl_info *ctrl,
			  struct be_queue_info *eq, int eq_delay)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct be_cmd_req_eq_create *req = embedded_payload(wrb);
	struct be_cmd_resp_eq_create *resp = embedded_payload(wrb);
	struct be_dma_mem *q_mem = &eq->dma_mem;
	int status;

	mutex_lock(&ctrl->mbox_lock);
	memset(wrb, 0, sizeof(*wrb));

	be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);

	be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_COMMON,
			OPCODE_COMMON_EQ_CREATE, sizeof(*req));

	req->num_pages = cpu_to_le16(PAGES_4K_SPANNED(q_mem->va, q_mem->size));

	AMAP_SET_BITS(struct amap_eq_context, func, req->context,
						PCI_FUNC(ctrl->pdev->devfn));
	AMAP_SET_BITS(struct amap_eq_context, valid, req->context, 1);
	AMAP_SET_BITS(struct amap_eq_context, size, req->context, 0);
	AMAP_SET_BITS(struct amap_eq_context, count, req->context,
					__ilog2_u32(eq->len / 256));
	AMAP_SET_BITS(struct amap_eq_context, delaymult, req->context,
					eq_delay_to_mult(eq_delay));
	be_dws_cpu_to_le(req->context, sizeof(req->context));

	be_cmd_page_addrs_prepare(req->pages, ARRAY_SIZE(req->pages), q_mem);

	status = be_mbox_notify(ctrl);
	if (!status) {
		eq->id = le16_to_cpu(resp->eq_id);
		eq->created = true;
	}
	mutex_unlock(&ctrl->mbox_lock);
	return status;
}

/**
 * be_cmd_fw_initialize()- Initialize FW
 * @ctrl: Pointer to function control structure
 *
 * Send FW initialize pattern for the function.
 *
 * return
 * Success: 0
 * Failure: Non-Zero value
 **/
int be_cmd_fw_initialize(struct be_ctrl_info *ctrl)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	int status;
	u8 *endian_check;

	mutex_lock(&ctrl->mbox_lock);
	memset(wrb, 0, sizeof(*wrb));

	endian_check = (u8 *) wrb;
	*endian_check++ = 0xFF;
	*endian_check++ = 0x12;
	*endian_check++ = 0x34;
	*endian_check++ = 0xFF;
	*endian_check++ = 0xFF;
	*endian_check++ = 0x56;
	*endian_check++ = 0x78;
	*endian_check++ = 0xFF;
	be_dws_cpu_to_le(wrb, sizeof(*wrb));

	status = be_mbox_notify(ctrl);
	if (status)
		beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_INIT,
			    "BC_%d : be_cmd_fw_initialize Failed\n");

	mutex_unlock(&ctrl->mbox_lock);
	return status;
}

/**
 * be_cmd_fw_uninit()- Uinitialize FW
 * @ctrl: Pointer to function control structure
 *
 * Send FW uninitialize pattern for the function
 *
 * return
 * Success: 0
 * Failure: Non-Zero value
 **/
int be_cmd_fw_uninit(struct be_ctrl_info *ctrl)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	int status;
	u8 *endian_check;

	mutex_lock(&ctrl->mbox_lock);
	memset(wrb, 0, sizeof(*wrb));

	endian_check = (u8 *) wrb;
	*endian_check++ = 0xFF;
	*endian_check++ = 0xAA;
	*endian_check++ = 0xBB;
	*endian_check++ = 0xFF;
	*endian_check++ = 0xFF;
	*endian_check++ = 0xCC;
	*endian_check++ = 0xDD;
	*endian_check = 0xFF;

	be_dws_cpu_to_le(wrb, sizeof(*wrb));

	status = be_mbox_notify(ctrl);
	if (status)
		beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_INIT,
			    "BC_%d : be_cmd_fw_uninit Failed\n");

	mutex_unlock(&ctrl->mbox_lock);
	return status;
}

int beiscsi_cmd_cq_create(struct be_ctrl_info *ctrl,
			  struct be_queue_info *cq, struct be_queue_info *eq,
			  bool sol_evts, bool no_delay, int coalesce_wm)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct be_cmd_req_cq_create *req = embedded_payload(wrb);
	struct be_cmd_resp_cq_create *resp = embedded_payload(wrb);
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	struct be_dma_mem *q_mem = &cq->dma_mem;
	void *ctxt = &req->context;
	int status;

	mutex_lock(&ctrl->mbox_lock);
	memset(wrb, 0, sizeof(*wrb));

	be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);

	be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_COMMON,
			OPCODE_COMMON_CQ_CREATE, sizeof(*req));

	req->num_pages = cpu_to_le16(PAGES_4K_SPANNED(q_mem->va, q_mem->size));
	if (is_chip_be2_be3r(phba)) {
		AMAP_SET_BITS(struct amap_cq_context, coalescwm,
			      ctxt, coalesce_wm);
		AMAP_SET_BITS(struct amap_cq_context, nodelay, ctxt, no_delay);
		AMAP_SET_BITS(struct amap_cq_context, count, ctxt,
			      __ilog2_u32(cq->len / 256));
		AMAP_SET_BITS(struct amap_cq_context, valid, ctxt, 1);
		AMAP_SET_BITS(struct amap_cq_context, solevent, ctxt, sol_evts);
		AMAP_SET_BITS(struct amap_cq_context, eventable, ctxt, 1);
		AMAP_SET_BITS(struct amap_cq_context, eqid, ctxt, eq->id);
		AMAP_SET_BITS(struct amap_cq_context, armed, ctxt, 1);
		AMAP_SET_BITS(struct amap_cq_context, func, ctxt,
			      PCI_FUNC(ctrl->pdev->devfn));
	} else {
		req->hdr.version = MBX_CMD_VER2;
		req->page_size = 1;
		AMAP_SET_BITS(struct amap_cq_context_v2, coalescwm,
			      ctxt, coalesce_wm);
		AMAP_SET_BITS(struct amap_cq_context_v2, nodelay,
			      ctxt, no_delay);
		AMAP_SET_BITS(struct amap_cq_context_v2, count, ctxt,
			      __ilog2_u32(cq->len / 256));
		AMAP_SET_BITS(struct amap_cq_context_v2, valid, ctxt, 1);
		AMAP_SET_BITS(struct amap_cq_context_v2, eventable, ctxt, 1);
		AMAP_SET_BITS(struct amap_cq_context_v2, eqid, ctxt, eq->id);
		AMAP_SET_BITS(struct amap_cq_context_v2, armed, ctxt, 1);
	}

	be_dws_cpu_to_le(ctxt, sizeof(req->context));

	be_cmd_page_addrs_prepare(req->pages, ARRAY_SIZE(req->pages), q_mem);

	status = be_mbox_notify(ctrl);
	if (!status) {
		cq->id = le16_to_cpu(resp->cq_id);
		cq->created = true;
	} else
		beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_INIT,
			    "BC_%d : In be_cmd_cq_create, status=ox%08x\n",
			    status);

	mutex_unlock(&ctrl->mbox_lock);

	return status;
}

static u32 be_encoded_q_len(int q_len)
{
	u32 len_encoded = fls(q_len);	/* log2(len) + 1 */
	if (len_encoded == 16)
		len_encoded = 0;
	return len_encoded;
}

int beiscsi_cmd_mccq_create(struct beiscsi_hba *phba,
			struct be_queue_info *mccq,
			struct be_queue_info *cq)
{
	struct be_mcc_wrb *wrb;
	struct be_cmd_req_mcc_create_ext *req;
	struct be_dma_mem *q_mem = &mccq->dma_mem;
	struct be_ctrl_info *ctrl;
	void *ctxt;
	int status;

	mutex_lock(&phba->ctrl.mbox_lock);
	ctrl = &phba->ctrl;
	wrb = wrb_from_mbox(&ctrl->mbox_mem);
	memset(wrb, 0, sizeof(*wrb));
	req = embedded_payload(wrb);
	ctxt = &req->context;

	be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);

	be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_COMMON,
			OPCODE_COMMON_MCC_CREATE_EXT, sizeof(*req));

	req->num_pages = PAGES_4K_SPANNED(q_mem->va, q_mem->size);
	req->async_evt_bitmap = 1 << ASYNC_EVENT_CODE_LINK_STATE;
	req->async_evt_bitmap |= 1 << ASYNC_EVENT_CODE_ISCSI;
	req->async_evt_bitmap |= 1 << ASYNC_EVENT_CODE_SLI;

	AMAP_SET_BITS(struct amap_mcc_context, fid, ctxt,
		      PCI_FUNC(phba->pcidev->devfn));
	AMAP_SET_BITS(struct amap_mcc_context, valid, ctxt, 1);
	AMAP_SET_BITS(struct amap_mcc_context, ring_size, ctxt,
		be_encoded_q_len(mccq->len));
	AMAP_SET_BITS(struct amap_mcc_context, cq_id, ctxt, cq->id);

	be_dws_cpu_to_le(ctxt, sizeof(req->context));

	be_cmd_page_addrs_prepare(req->pages, ARRAY_SIZE(req->pages), q_mem);

	status = be_mbox_notify(ctrl);
	if (!status) {
		struct be_cmd_resp_mcc_create *resp = embedded_payload(wrb);
		mccq->id = le16_to_cpu(resp->id);
		mccq->created = true;
	}
	mutex_unlock(&phba->ctrl.mbox_lock);

	return status;
}

int beiscsi_cmd_q_destroy(struct be_ctrl_info *ctrl, struct be_queue_info *q,
			  int queue_type)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct be_cmd_req_q_destroy *req = embedded_payload(wrb);
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	u8 subsys = 0, opcode = 0;
	int status;

	beiscsi_log(phba, KERN_INFO, BEISCSI_LOG_INIT,
		    "BC_%d : In beiscsi_cmd_q_destroy "
		    "queue_type : %d\n", queue_type);

	mutex_lock(&ctrl->mbox_lock);
	memset(wrb, 0, sizeof(*wrb));
	be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);

	switch (queue_type) {
	case QTYPE_EQ:
		subsys = CMD_SUBSYSTEM_COMMON;
		opcode = OPCODE_COMMON_EQ_DESTROY;
		break;
	case QTYPE_CQ:
		subsys = CMD_SUBSYSTEM_COMMON;
		opcode = OPCODE_COMMON_CQ_DESTROY;
		break;
	case QTYPE_MCCQ:
		subsys = CMD_SUBSYSTEM_COMMON;
		opcode = OPCODE_COMMON_MCC_DESTROY;
		break;
	case QTYPE_WRBQ:
		subsys = CMD_SUBSYSTEM_ISCSI;
		opcode = OPCODE_COMMON_ISCSI_WRBQ_DESTROY;
		break;
	case QTYPE_DPDUQ:
		subsys = CMD_SUBSYSTEM_ISCSI;
		opcode = OPCODE_COMMON_ISCSI_DEFQ_DESTROY;
		break;
	case QTYPE_SGL:
		subsys = CMD_SUBSYSTEM_ISCSI;
		opcode = OPCODE_COMMON_ISCSI_CFG_REMOVE_SGL_PAGES;
		break;
	default:
		mutex_unlock(&ctrl->mbox_lock);
		BUG();
		return -ENXIO;
	}
	be_cmd_hdr_prepare(&req->hdr, subsys, opcode, sizeof(*req));
	if (queue_type != QTYPE_SGL)
		req->id = cpu_to_le16(q->id);

	status = be_mbox_notify(ctrl);

	mutex_unlock(&ctrl->mbox_lock);
	return status;
}

/**
 * be_cmd_create_default_pdu_queue()- Create DEFQ for the adapter
 * @ctrl: ptr to ctrl_info
 * @cq: Completion Queue
 * @dq: Default Queue
 * @lenght: ring size
 * @entry_size: size of each entry in DEFQ
 * @is_header: Header or Data DEFQ
 * @ulp_num: Bind to which ULP
 *
 * Create HDR/Data DEFQ for the passed ULP. Unsol PDU are posted
 * on this queue by the FW
 *
 * return
 *	Success: 0
 *	Failure: Non-Zero Value
 *
 **/
int be_cmd_create_default_pdu_queue(struct be_ctrl_info *ctrl,
				    struct be_queue_info *cq,
				    struct be_queue_info *dq, int length,
				    int entry_size, uint8_t is_header,
				    uint8_t ulp_num)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct be_defq_create_req *req = embedded_payload(wrb);
	struct be_dma_mem *q_mem = &dq->dma_mem;
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	void *ctxt = &req->context;
	int status;

	mutex_lock(&ctrl->mbox_lock);
	memset(wrb, 0, sizeof(*wrb));

	be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);

	be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_ISCSI,
			   OPCODE_COMMON_ISCSI_DEFQ_CREATE, sizeof(*req));

	req->num_pages = PAGES_4K_SPANNED(q_mem->va, q_mem->size);
	if (phba->fw_config.dual_ulp_aware) {
		req->ulp_num = ulp_num;
		req->dua_feature |= (1 << BEISCSI_DUAL_ULP_AWARE_BIT);
		req->dua_feature |= (1 << BEISCSI_BIND_Q_TO_ULP_BIT);
	}

	if (is_chip_be2_be3r(phba)) {
		AMAP_SET_BITS(struct amap_be_default_pdu_context,
			      rx_pdid, ctxt, 0);
		AMAP_SET_BITS(struct amap_be_default_pdu_context,
			      rx_pdid_valid, ctxt, 1);
		AMAP_SET_BITS(struct amap_be_default_pdu_context,
			      pci_func_id, ctxt, PCI_FUNC(ctrl->pdev->devfn));
		AMAP_SET_BITS(struct amap_be_default_pdu_context,
			      ring_size, ctxt,
			      be_encoded_q_len(length /
			      sizeof(struct phys_addr)));
		AMAP_SET_BITS(struct amap_be_default_pdu_context,
			      default_buffer_size, ctxt, entry_size);
		AMAP_SET_BITS(struct amap_be_default_pdu_context,
			      cq_id_recv, ctxt,	cq->id);
	} else {
		AMAP_SET_BITS(struct amap_default_pdu_context_ext,
			      rx_pdid, ctxt, 0);
		AMAP_SET_BITS(struct amap_default_pdu_context_ext,
			      rx_pdid_valid, ctxt, 1);
		AMAP_SET_BITS(struct amap_default_pdu_context_ext,
			      ring_size, ctxt,
			      be_encoded_q_len(length /
			      sizeof(struct phys_addr)));
		AMAP_SET_BITS(struct amap_default_pdu_context_ext,
			      default_buffer_size, ctxt, entry_size);
		AMAP_SET_BITS(struct amap_default_pdu_context_ext,
			      cq_id_recv, ctxt, cq->id);
	}

	be_dws_cpu_to_le(ctxt, sizeof(req->context));

	be_cmd_page_addrs_prepare(req->pages, ARRAY_SIZE(req->pages), q_mem);

	status = be_mbox_notify(ctrl);
	if (!status) {
		struct be_ring *defq_ring;
		struct be_defq_create_resp *resp = embedded_payload(wrb);

		dq->id = le16_to_cpu(resp->id);
		dq->created = true;
		if (is_header)
			defq_ring = &phba->phwi_ctrlr->default_pdu_hdr[ulp_num];
		else
			defq_ring = &phba->phwi_ctrlr->
				    default_pdu_data[ulp_num];

		defq_ring->id = dq->id;

		if (!phba->fw_config.dual_ulp_aware) {
			defq_ring->ulp_num = BEISCSI_ULP0;
			defq_ring->doorbell_offset = DB_RXULP0_OFFSET;
		} else {
			defq_ring->ulp_num = resp->ulp_num;
			defq_ring->doorbell_offset = resp->doorbell_offset;
		}
	}
	mutex_unlock(&ctrl->mbox_lock);

	return status;
}

/**
 * be_cmd_wrbq_create()- Create WRBQ
 * @ctrl: ptr to ctrl_info
 * @q_mem: memory details for the queue
 * @wrbq: queue info
 * @pwrb_context: ptr to wrb_context
 * @ulp_num: ULP on which the WRBQ is to be created
 *
 * Create WRBQ on the passed ULP_NUM.
 *
 **/
int be_cmd_wrbq_create(struct be_ctrl_info *ctrl,
			struct be_dma_mem *q_mem,
			struct be_queue_info *wrbq,
			struct hwi_wrb_context *pwrb_context,
			uint8_t ulp_num)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct be_wrbq_create_req *req = embedded_payload(wrb);
	struct be_wrbq_create_resp *resp = embedded_payload(wrb);
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	int status;

	mutex_lock(&ctrl->mbox_lock);
	memset(wrb, 0, sizeof(*wrb));

	be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);

	be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_ISCSI,
		OPCODE_COMMON_ISCSI_WRBQ_CREATE, sizeof(*req));
	req->num_pages = PAGES_4K_SPANNED(q_mem->va, q_mem->size);

	if (phba->fw_config.dual_ulp_aware) {
		req->ulp_num = ulp_num;
		req->dua_feature |= (1 << BEISCSI_DUAL_ULP_AWARE_BIT);
		req->dua_feature |= (1 << BEISCSI_BIND_Q_TO_ULP_BIT);
	}

	be_cmd_page_addrs_prepare(req->pages, ARRAY_SIZE(req->pages), q_mem);

	status = be_mbox_notify(ctrl);
	if (!status) {
		wrbq->id = le16_to_cpu(resp->cid);
		wrbq->created = true;

		pwrb_context->cid = wrbq->id;
		if (!phba->fw_config.dual_ulp_aware) {
			pwrb_context->doorbell_offset = DB_TXULP0_OFFSET;
			pwrb_context->ulp_num = BEISCSI_ULP0;
		} else {
			pwrb_context->ulp_num = resp->ulp_num;
			pwrb_context->doorbell_offset = resp->doorbell_offset;
		}
	}
	mutex_unlock(&ctrl->mbox_lock);
	return status;
}

int be_cmd_iscsi_post_template_hdr(struct be_ctrl_info *ctrl,
				    struct be_dma_mem *q_mem)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct be_post_template_pages_req *req = embedded_payload(wrb);
	int status;

	mutex_lock(&ctrl->mbox_lock);

	memset(wrb, 0, sizeof(*wrb));
	be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);
	be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_COMMON,
			   OPCODE_COMMON_ADD_TEMPLATE_HEADER_BUFFERS,
			   sizeof(*req));

	req->num_pages = PAGES_4K_SPANNED(q_mem->va, q_mem->size);
	req->type = BEISCSI_TEMPLATE_HDR_TYPE_ISCSI;
	be_cmd_page_addrs_prepare(req->pages, ARRAY_SIZE(req->pages), q_mem);

	status = be_mbox_notify(ctrl);
	mutex_unlock(&ctrl->mbox_lock);
	return status;
}

int be_cmd_iscsi_remove_template_hdr(struct be_ctrl_info *ctrl)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct be_remove_template_pages_req *req = embedded_payload(wrb);
	int status;

	mutex_lock(&ctrl->mbox_lock);

	memset(wrb, 0, sizeof(*wrb));
	be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);
	be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_COMMON,
			   OPCODE_COMMON_REMOVE_TEMPLATE_HEADER_BUFFERS,
			   sizeof(*req));

	req->type = BEISCSI_TEMPLATE_HDR_TYPE_ISCSI;

	status = be_mbox_notify(ctrl);
	mutex_unlock(&ctrl->mbox_lock);
	return status;
}

int be_cmd_iscsi_post_sgl_pages(struct be_ctrl_info *ctrl,
				struct be_dma_mem *q_mem,
				u32 page_offset, u32 num_pages)
{
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct be_post_sgl_pages_req *req = embedded_payload(wrb);
	struct beiscsi_hba *phba = pci_get_drvdata(ctrl->pdev);
	int status;
	unsigned int curr_pages;
	u32 internal_page_offset = 0;
	u32 temp_num_pages = num_pages;

	if (num_pages == 0xff)
		num_pages = 1;

	mutex_lock(&ctrl->mbox_lock);
	do {
		memset(wrb, 0, sizeof(*wrb));
		be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);
		be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_ISCSI,
				   OPCODE_COMMON_ISCSI_CFG_POST_SGL_PAGES,
				   sizeof(*req));
		curr_pages = BE_NUMBER_OF_FIELD(struct be_post_sgl_pages_req,
						pages);
		req->num_pages = min(num_pages, curr_pages);
		req->page_offset = page_offset;
		be_cmd_page_addrs_prepare(req->pages, req->num_pages, q_mem);
		q_mem->dma = q_mem->dma + (req->num_pages * PAGE_SIZE);
		internal_page_offset += req->num_pages;
		page_offset += req->num_pages;
		num_pages -= req->num_pages;

		if (temp_num_pages == 0xff)
			req->num_pages = temp_num_pages;

		status = be_mbox_notify(ctrl);
		if (status) {
			beiscsi_log(phba, KERN_ERR, BEISCSI_LOG_INIT,
				    "BC_%d : FW CMD to map iscsi frags failed.\n");

			goto error;
		}
	} while (num_pages > 0);
error:
	mutex_unlock(&ctrl->mbox_lock);
	if (status != 0)
		beiscsi_cmd_q_destroy(ctrl, NULL, QTYPE_SGL);
	return status;
}

int beiscsi_cmd_reset_function(struct beiscsi_hba  *phba)
{
	struct be_ctrl_info *ctrl = &phba->ctrl;
	struct be_mcc_wrb *wrb = wrb_from_mbox(&ctrl->mbox_mem);
	struct be_post_sgl_pages_req *req = embedded_payload(wrb);
	int status;

	mutex_lock(&ctrl->mbox_lock);

	req = embedded_payload(wrb);
	be_wrb_hdr_prepare(wrb, sizeof(*req), true, 0);
	be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_COMMON,
			   OPCODE_COMMON_FUNCTION_RESET, sizeof(*req));
	status = be_mbox_notify(ctrl);

	mutex_unlock(&ctrl->mbox_lock);
	return status;
}

/**
 * be_cmd_set_vlan()- Configure VLAN paramters on the adapter
 * @phba: device priv structure instance
 * @vlan_tag: TAG to be set
 *
 * Set the VLAN_TAG for the adapter or Disable VLAN on adapter
 *
 * returns
 *	TAG for the MBX Cmd
 * **/
int be_cmd_set_vlan(struct beiscsi_hba *phba,
		     uint16_t vlan_tag)
{
	unsigned int tag;
	struct be_mcc_wrb *wrb;
	struct be_cmd_set_vlan_req *req;
	struct be_ctrl_info *ctrl = &phba->ctrl;

	if (mutex_lock_interruptible(&ctrl->mbox_lock))
		return 0;
	wrb = alloc_mcc_wrb(phba, &tag);
	if (!wrb) {
		mutex_unlock(&ctrl->mbox_lock);
		return 0;
	}

	req = embedded_payload(wrb);
	be_wrb_hdr_prepare(wrb, sizeof(*wrb), true, 0);
	be_cmd_hdr_prepare(&req->hdr, CMD_SUBSYSTEM_ISCSI,
			   OPCODE_COMMON_ISCSI_NTWK_SET_VLAN,
			   sizeof(*req));

	req->interface_hndl = phba->interface_handle;
	req->vlan_priority = vlan_tag;

	be_mcc_notify(phba, tag);
	mutex_unlock(&ctrl->mbox_lock);

	return tag;
}
