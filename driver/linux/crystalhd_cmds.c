/***************************************************************************
 * Copyright (c) 2005-2009, Broadcom Corporation.
 *
 *  Name: crystalhd_cmds . c
 *
 *  Description:
 *		BCM70012/BCM70015 Linux driver user command interfaces.
 *
 *  HISTORY:
 *
 **********************************************************************
 * This file is part of the crystalhd device driver.
 *
 * This driver is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this driver.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include "crystalhd_lnx.h"

static struct crystalhd_user *bc_cproc_get_uid(struct crystalhd_cmd *ctx)
{
	struct crystalhd_user *user = NULL;
	int i;

	for (i = 0; i < BC_LINK_MAX_OPENS; i++) {
		if (!ctx->user[i].in_use) {
			user = &ctx->user[i];
			break;
		}
	}

	return user;
}

static int bc_cproc_get_user_count(struct crystalhd_cmd *ctx)
{
	int i, count = 0;

	for (i = 0; i < BC_LINK_MAX_OPENS; i++) {
		if (ctx->user[i].in_use)
			count++;
	}

	return count;
}

static void bc_cproc_mark_pwr_state(struct crystalhd_cmd *ctx)
{
	int i;

	for (i = 0; i < BC_LINK_MAX_OPENS; i++) {
		if (!ctx->user[i].in_use)
			continue;
		if ((ctx->user[i].mode & 0xFF) == DTS_DIAG_MODE ||
		    (ctx->user[i].mode & 0xFF) == DTS_PLAYBACK_MODE) {
			ctx->pwr_state_change = 1;
			break;
		}
	}
}

static BC_STATUS bc_cproc_notify_mode(struct crystalhd_cmd *ctx,
				      crystalhd_ioctl_data *idata)
{
	struct device *dev = chddev();
	int rc = 0, i = 0;

	if (!ctx || !idata) {
		dev_err(dev, "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	if (ctx->user[idata->u_id].mode != DTS_MODE_INV) {
		dev_err(dev, "Close the handle first..\n");
		return BC_STS_ERR_USAGE;
	}

	if ((idata->udata.u.NotifyMode.Mode && 0xFF) == DTS_MONITOR_MODE) {
		ctx->user[idata->u_id].mode = idata->udata.u.NotifyMode.Mode;
		return BC_STS_SUCCESS;
	}

	if (ctx->state != BC_LINK_INVALID) {
		dev_err(dev, "Link invalid state notify mode %d \n", ctx->state);
		return BC_STS_ERR_USAGE;
	}
	/* Check for duplicate playback sessions..*/
	for (i = 0; i < BC_LINK_MAX_OPENS; i++) {
		if ((ctx->user[i].mode & 0xFF) == DTS_DIAG_MODE ||
		    (ctx->user[i].mode & 0xFF) == DTS_PLAYBACK_MODE) {
			dev_err(dev, "multiple playback sessions are not "
				"supported..\n");
			return BC_STS_ERR_USAGE;
		}
	}
	ctx->cin_wait_exit = 0;

	ctx->user[idata->u_id].mode = idata->udata.u.NotifyMode.Mode;
	/* Create list pools */
	rc = crystalhd_create_elem_pool(ctx->adp, BC_LINK_ELEM_POOL_SZ);
	if (rc)
		return BC_STS_ERROR;
	/* Setup mmap pool for uaddr sgl mapping..*/
	rc = crystalhd_create_dio_pool(ctx->adp, BC_LINK_MAX_SGLS);
	if (rc)
		return BC_STS_ERROR;

	/* Setup Hardware DMA rings */
	return crystalhd_hw_setup_dma_rings(ctx->hw_ctx);
}

static BC_STATUS bc_cproc_get_version(struct crystalhd_cmd *ctx,
				      crystalhd_ioctl_data *idata)
{
	if (!ctx || !idata) {
		dev_err(chddev(), "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}
	idata->udata.u.VerInfo.DriverMajor = crystalhd_kmod_major;
	idata->udata.u.VerInfo.DriverMinor = crystalhd_kmod_minor;
	idata->udata.u.VerInfo.DriverRevision	= crystalhd_kmod_rev;
	return BC_STS_SUCCESS;
}


static BC_STATUS bc_cproc_get_hwtype(struct crystalhd_cmd *ctx, crystalhd_ioctl_data *idata)
{
	if (!ctx || !idata) {
		dev_err(chddev(), "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	crystalhd_pci_cfg_rd(ctx->adp, 0, 2,
			   (uint32_t *)&idata->udata.u.hwType.PciVenId);
	crystalhd_pci_cfg_rd(ctx->adp, 2, 2,
			   (uint32_t *)&idata->udata.u.hwType.PciDevId);
	crystalhd_pci_cfg_rd(ctx->adp, 8, 1,
			   (uint32_t *)&idata->udata.u.hwType.HwRev);

	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_reg_rd(struct crystalhd_cmd *ctx,
				 crystalhd_ioctl_data *idata)
{
	if (!ctx || !idata)
		return BC_STS_INV_ARG;
	idata->udata.u.regAcc.Value = ctx->hw_ctx->pfnReadDevRegister(ctx->adp,
					idata->udata.u.regAcc.Offset);
	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_reg_wr(struct crystalhd_cmd *ctx,
				 crystalhd_ioctl_data *idata)
{
	if (!ctx || !idata)
		return BC_STS_INV_ARG;

	ctx->hw_ctx->pfnWriteDevRegister(ctx->adp, idata->udata.u.regAcc.Offset,
		      idata->udata.u.regAcc.Value);

	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_link_reg_rd(struct crystalhd_cmd *ctx,
				      crystalhd_ioctl_data *idata)
{
	if (!ctx || !idata)
		return BC_STS_INV_ARG;

	idata->udata.u.regAcc.Value = ctx->hw_ctx->pfnReadFPGARegister(ctx->adp,
					idata->udata.u.regAcc.Offset);
	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_link_reg_wr(struct crystalhd_cmd *ctx,
				      crystalhd_ioctl_data *idata)
{
	if (!ctx || !idata)
		return BC_STS_INV_ARG;

	ctx->hw_ctx->pfnWriteFPGARegister(ctx->adp, idata->udata.u.regAcc.Offset,
		       idata->udata.u.regAcc.Value);

	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_mem_rd(struct crystalhd_cmd *ctx,
				 crystalhd_ioctl_data *idata)
{
	BC_STATUS sts = BC_STS_SUCCESS;

	if (!ctx || !idata || !idata->add_cdata)
		return BC_STS_INV_ARG;

	if (idata->udata.u.devMem.NumDwords > (idata->add_cdata_sz / 4)) {
		dev_err(chddev(), "insufficient buffer\n");
		return BC_STS_INV_ARG;
	}
	sts = ctx->hw_ctx->pfnDevDRAMRead(ctx->hw_ctx, idata->udata.u.devMem.StartOff,
			     idata->udata.u.devMem.NumDwords,
			     (uint32_t *)idata->add_cdata);
	return sts;

}

static BC_STATUS bc_cproc_mem_wr(struct crystalhd_cmd *ctx,
				 crystalhd_ioctl_data *idata)
{
	BC_STATUS sts = BC_STS_SUCCESS;

	if (!ctx || !idata || !idata->add_cdata)
		return BC_STS_INV_ARG;

	if (idata->udata.u.devMem.NumDwords > (idata->add_cdata_sz / 4)) {
		dev_err(chddev(), "insufficient buffer\n");
		return BC_STS_INV_ARG;
	}

	sts = ctx->hw_ctx->pfnDevDRAMWrite(ctx->hw_ctx, idata->udata.u.devMem.StartOff,
			     idata->udata.u.devMem.NumDwords,
			     (uint32_t *)idata->add_cdata);
	return sts;
}

static BC_STATUS bc_cproc_cfg_rd(struct crystalhd_cmd *ctx,
				 crystalhd_ioctl_data *idata)
{
	uint32_t ix, cnt, off, len;
	BC_STATUS sts = BC_STS_SUCCESS;
	uint32_t *temp;

	if (!ctx || !idata)
		return BC_STS_INV_ARG;

	temp = (uint32_t *) idata->udata.u.pciCfg.pci_cfg_space;
	off = idata->udata.u.pciCfg.Offset;
	len = idata->udata.u.pciCfg.Size;

	if (len <= 4) {
		sts = crystalhd_pci_cfg_rd(ctx->adp, off, len, temp);
		return sts;
	}

	/* Truncate to dword alignment..*/
	len = 4;
	cnt = idata->udata.u.pciCfg.Size / len;
	for (ix = 0; ix < cnt; ix++) {
		sts = crystalhd_pci_cfg_rd(ctx->adp, off, len, &temp[ix]);
		if (sts != BC_STS_SUCCESS) {
			dev_err(chddev(), "config read : %d\n", sts);
			return sts;
		}
		off += len;
	}

	return sts;
}

static BC_STATUS bc_cproc_cfg_wr(struct crystalhd_cmd *ctx,
				 crystalhd_ioctl_data *idata)
{
	uint32_t ix, cnt, off, len;
	BC_STATUS sts = BC_STS_SUCCESS;
	uint32_t *temp;

	if (!ctx || !idata)
		return BC_STS_INV_ARG;

	temp = (uint32_t *) idata->udata.u.pciCfg.pci_cfg_space;
	off = idata->udata.u.pciCfg.Offset;
	len = idata->udata.u.pciCfg.Size;

	if (len <= 4)
		return crystalhd_pci_cfg_wr(ctx->adp, off, len, temp[0]);

	/* Truncate to dword alignment..*/
	len = 4;
	cnt = idata->udata.u.pciCfg.Size / len;
	for (ix = 0; ix < cnt; ix++) {
		sts = crystalhd_pci_cfg_wr(ctx->adp, off, len, temp[ix]);
		if (sts != BC_STS_SUCCESS) {
			dev_err(chddev(), "config write : %d\n", sts);
			return sts;
		}
		off += len;
	}

	return sts;
}

static BC_STATUS bc_cproc_download_fw(struct crystalhd_cmd *ctx,
				      crystalhd_ioctl_data *idata)
{
	BC_STATUS sts = BC_STS_SUCCESS;

	dev_dbg(chddev(), "Downloading FW\n");

	if (!ctx || !idata || !idata->add_cdata || !idata->add_cdata_sz) {
		dev_err(chddev(), "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	if (ctx->state != BC_LINK_INVALID) {
		dev_err(chddev(), "Link invalid state download fw %d \n", ctx->state);
		return BC_STS_ERR_USAGE;
	}

	sts = ctx->hw_ctx->pfnFWDwnld(ctx->hw_ctx, (uint8_t *)idata->add_cdata,
				  idata->add_cdata_sz);

	if (sts != BC_STS_SUCCESS) {
		dev_info(chddev(), "Firmware Download Failure!! - %d\n", sts);
	} else
		ctx->state |= BC_LINK_INIT;

	ctx->hw_ctx->FwCmdCnt = 0;
	return sts;
}

/*
 * We use the FW_CMD interface to sync up playback state with application
 * and  firmware. This function will perform the required pre and post
 * processing of the Firmware commands.
 *
 * Pause -
 *	Disable capture after decoder pause.
 * Resume -
 *	First enable capture and issue decoder resume command.
 * Flush -
 *	Abort pending input transfers and issue decoder flush command.
 *
 */
static BC_STATUS bc_cproc_do_fw_cmd(struct crystalhd_cmd *ctx, crystalhd_ioctl_data *idata)
{
	struct device *dev = chddev();
	BC_STATUS sts;
	uint32_t *cmd;

	if (!(ctx->state & BC_LINK_INIT)) {
		dev_err(dev, "Link invalid state do fw cmd %d \n", ctx->state);
		return BC_STS_ERR_USAGE;
	}

	cmd = idata->udata.u.fwCmd.cmd;

	/* Pre-Process */
	if (cmd[0] == eCMD_C011_DEC_CHAN_PAUSE) {
		if (!cmd[3]) {
			ctx->state &= ~BC_LINK_PAUSED;
			ctx->hw_ctx->pfnIssuePause(ctx->hw_ctx, false);
		}
	} else if (cmd[0] == eCMD_C011_DEC_CHAN_FLUSH) {
		dev_dbg(dev, "Flush issued\n");
		if (cmd[3])
			ctx->cin_wait_exit = 1;
	}

	sts = ctx->hw_ctx->pfnDoFirmwareCmd(ctx->hw_ctx, &idata->udata.u.fwCmd);

	if (sts != BC_STS_SUCCESS) {
		dev_info(dev, "fw cmd %x failed\n", cmd[0]);
		return sts;
	}

	/* Post-Process */
	if (cmd[0] == eCMD_C011_DEC_CHAN_PAUSE) {
		if (cmd[3]) {
			ctx->state |= BC_LINK_PAUSED;
			ctx->hw_ctx->pfnIssuePause(ctx->hw_ctx, true);
		}
	}

	return sts;
}

static void bc_proc_in_completion(crystalhd_dio_req *dio_hnd,
				  wait_queue_head_t *event, BC_STATUS sts)
{
	if (!dio_hnd || !event) {
		dev_err(chddev(), "%s: Invalid Arg\n", __func__);
		return;
	}
	if (sts == BC_STS_IO_USER_ABORT)
		 return;

	dio_hnd->uinfo.comp_sts = sts;
	dio_hnd->uinfo.ev_sts = 1;
	crystalhd_set_event(event);
}

static BC_STATUS bc_cproc_codein_sleep(struct crystalhd_cmd *ctx)
{
	wait_queue_head_t sleep_ev;
	int rc = 0;

	if (ctx->state & BC_LINK_SUSPEND)
		return BC_STS_IO_USER_ABORT;

	if (ctx->cin_wait_exit) {
		ctx->cin_wait_exit = 0;
		return BC_STS_CMD_CANCELLED;
	}
	crystalhd_create_event(&sleep_ev);
	crystalhd_wait_on_event(&sleep_ev, 0, 100, rc, false);
	if (rc == -EINTR)
		return BC_STS_IO_USER_ABORT;

	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_hw_txdma(struct crystalhd_cmd *ctx,
				   crystalhd_ioctl_data *idata,
				   crystalhd_dio_req *dio)
{
	struct device *dev = chddev();
	uint32_t tx_listid = 0;
	BC_STATUS sts = BC_STS_SUCCESS;
	wait_queue_head_t event;
	int rc = 0;

	if (!ctx || !idata || !dio) {
		dev_err(dev, "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	crystalhd_create_event(&event);

	ctx->tx_list_id = 0;
	/* msleep_interruptible(2000); */
	sts = crystalhd_hw_post_tx(ctx->hw_ctx, dio, bc_proc_in_completion,
				 &event, &tx_listid,
				 idata->udata.u.ProcInput.Encrypted);

	while (sts == BC_STS_BUSY) {
		sts = bc_cproc_codein_sleep(ctx);
		if (sts != BC_STS_SUCCESS)
			break;
		sts = crystalhd_hw_post_tx(ctx->hw_ctx, dio,
					 bc_proc_in_completion,
					 &event, &tx_listid,
					 idata->udata.u.ProcInput.Encrypted);
	}
	if (sts != BC_STS_SUCCESS) {
		dev_dbg(dev, "_hw_txdma returning sts:%d\n", sts);
		return sts;
	}
	if (ctx->cin_wait_exit)
		ctx->cin_wait_exit = 0;

	ctx->tx_list_id = tx_listid;

	/* _post() succeeded.. wait for the completion. */
	crystalhd_wait_on_event(&event, (dio->uinfo.ev_sts), 3000, rc, false);
	ctx->tx_list_id = 0;
	if (!rc) {
		return dio->uinfo.comp_sts;
	} else if (rc == -EBUSY) {
		dev_dbg(dev, "_tx_post() T/O \n");
		sts = BC_STS_TIMEOUT;
	} else if (rc == -EINTR) {
		dev_dbg(dev, "Tx Wait Signal int.\n");
		sts = BC_STS_IO_USER_ABORT;
	} else {
		sts = BC_STS_IO_ERROR;
	}

	/* We are cancelling the IO from the same context as the _post().
	 * so no need to wait on the event again.. the return itself
	 * ensures the release of our resources.
	 */
	crystalhd_hw_cancel_tx(ctx->hw_ctx, tx_listid);

	return sts;
}

/* Helper function to check on user buffers */
static BC_STATUS bc_cproc_check_inbuffs(bool pin, void *ubuff, uint32_t ub_sz,
					uint32_t uv_off, bool en_422)
{
	struct device *dev = chddev();
	if (!ubuff || !ub_sz) {
		dev_err(dev, "%s->Invalid Arg %p %x\n",
			((pin) ? "TX" : "RX"), ubuff, ub_sz);
		return BC_STS_INV_ARG;
	}

	/* Check for alignment */
	if (((uintptr_t)ubuff) & 0x03) {
		dev_err(dev, "%s-->Un-aligned address not implemented yet.. %p \n",
				((pin) ? "TX" : "RX"), ubuff);
		return BC_STS_NOT_IMPL;
	}
	if (pin)
		return BC_STS_SUCCESS;

	if (!en_422 && !uv_off) {
		dev_err(dev, "Need UV offset for 420 mode.\n");
		return BC_STS_INV_ARG;
	}

	if (en_422 && uv_off) {
		dev_err(dev, "UV offset in 422 mode ??\n");
		return BC_STS_INV_ARG;
	}

	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_proc_input(struct crystalhd_cmd *ctx, crystalhd_ioctl_data *idata)
{
	struct device *dev = chddev();
	void *ubuff;
	uint32_t ub_sz;
	crystalhd_dio_req *dio_hnd = NULL;
	BC_STATUS sts = BC_STS_SUCCESS;

	if (!ctx || !idata) {
		dev_err(dev, "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	ubuff = idata->udata.u.ProcInput.pDmaBuff;
	ub_sz = idata->udata.u.ProcInput.BuffSz;

	sts = bc_cproc_check_inbuffs(1, ubuff, ub_sz, 0, 0);
	if (sts != BC_STS_SUCCESS)
		return sts;

	sts = crystalhd_map_dio(ctx->adp, ubuff, ub_sz, 0, 0, 1, &dio_hnd);
	if (sts != BC_STS_SUCCESS) {
		dev_err(dev, "dio map - %d \n", sts);
		return sts;
	}

	if (!dio_hnd)
		return BC_STS_ERROR;

	sts = bc_cproc_hw_txdma(ctx, idata, dio_hnd);

	crystalhd_unmap_dio(ctx->adp, dio_hnd);

	return sts;
}

static BC_STATUS bc_cproc_add_cap_buff(struct crystalhd_cmd *ctx,
				       crystalhd_ioctl_data *idata)
{
	struct device *dev = chddev();
	void *ubuff;
	uint32_t ub_sz, uv_off;
	bool en_422;
	crystalhd_dio_req *dio_hnd = NULL;
	BC_STATUS sts = BC_STS_SUCCESS;

	if (!ctx || !idata) {
		dev_err(dev, "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	ubuff = idata->udata.u.RxBuffs.YuvBuff;
	ub_sz = idata->udata.u.RxBuffs.YuvBuffSz;
	uv_off = idata->udata.u.RxBuffs.UVbuffOffset;
	en_422 = idata->udata.u.RxBuffs.b422Mode;

	sts = bc_cproc_check_inbuffs(0, ubuff, ub_sz, uv_off, en_422);

	if (sts != BC_STS_SUCCESS)
		return sts;

	sts = crystalhd_map_dio(ctx->adp, ubuff, ub_sz, uv_off,
			      en_422, 0, &dio_hnd);
	if (sts != BC_STS_SUCCESS) {
		dev_err(dev, "dio map - %d \n", sts);
		return sts;
	}

	if (!dio_hnd)
		return BC_STS_ERROR;

	sts = crystalhd_hw_add_cap_buffer(ctx->hw_ctx, dio_hnd, (ctx->state == BC_LINK_READY));
	if ((sts != BC_STS_SUCCESS) && (sts != BC_STS_BUSY)) {
		crystalhd_unmap_dio(ctx->adp, dio_hnd);
		return sts;
	}

	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_fmt_change(struct crystalhd_cmd *ctx,
				     crystalhd_dio_req *dio)
{
	BC_STATUS sts = BC_STS_SUCCESS;

	sts = crystalhd_hw_add_cap_buffer(ctx->hw_ctx, dio, 0);
	if (sts != BC_STS_SUCCESS)
		return sts;

	ctx->state |= BC_LINK_FMT_CHG;
	if (ctx->state == BC_LINK_READY)
		sts = crystalhd_hw_start_capture(ctx->hw_ctx);

	return sts;
}

static BC_STATUS bc_cproc_fetch_frame(struct crystalhd_cmd *ctx,
				      crystalhd_ioctl_data *idata)
{
	struct device *dev = chddev();
	crystalhd_dio_req *dio = NULL;
	BC_STATUS sts = BC_STS_SUCCESS;
	BC_DEC_OUT_BUFF *frame;

	if (!ctx || !idata) {
		dev_err(dev, "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	if (!(ctx->state & BC_LINK_CAP_EN)) {
		dev_dbg(dev, "Capture not enabled..%x\n", ctx->state);
		return BC_STS_ERR_USAGE;
	}

	frame = &idata->udata.u.DecOutData;

	sts = crystalhd_hw_get_cap_buffer(ctx->hw_ctx, &frame->PibInfo, &dio);
	if (sts != BC_STS_SUCCESS)
		return (ctx->state & BC_LINK_SUSPEND) ? BC_STS_IO_USER_ABORT : sts;

	frame->Flags = dio->uinfo.comp_flags;

	if (frame->Flags & COMP_FLAG_FMT_CHANGE)
		return bc_cproc_fmt_change(ctx, dio);

	frame->OutPutBuffs.YuvBuff = dio->uinfo.xfr_buff;
	frame->OutPutBuffs.YuvBuffSz = dio->uinfo.xfr_len;
	frame->OutPutBuffs.UVbuffOffset = dio->uinfo.uv_offset;
	frame->OutPutBuffs.b422Mode = dio->uinfo.b422mode;

	frame->OutPutBuffs.YBuffDoneSz = dio->uinfo.y_done_sz;
	frame->OutPutBuffs.UVBuffDoneSz = dio->uinfo.uv_done_sz;

	crystalhd_unmap_dio(ctx->adp, dio);

	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_start_capture(struct crystalhd_cmd *ctx,
					crystalhd_ioctl_data *idata)
{
	ctx->state |= BC_LINK_CAP_EN;

	if( idata->udata.u.RxCap.PauseThsh )
		ctx->hw_ctx->PauseThreshold = idata->udata.u.RxCap.PauseThsh;
	else
		ctx->hw_ctx->PauseThreshold = HW_PAUSE_THRESHOLD;

	if( idata->udata.u.RxCap.ResumeThsh )
		ctx->hw_ctx->ResumeThreshold = idata->udata.u.RxCap.ResumeThsh;
	else
		ctx->hw_ctx->ResumeThreshold = HW_RESUME_THRESHOLD;

	printk(KERN_DEBUG "start_capture: pause_th:%d, resume_th:%d\n", ctx->hw_ctx->PauseThreshold, ctx->hw_ctx->ResumeThreshold);

	ctx->hw_ctx->DrvTotalFrmCaptured = 0;

	ctx->hw_ctx->DefaultPauseThreshold = ctx->hw_ctx->PauseThreshold; // used to restore on FMTCH

	ctx->hw_ctx->pfnNotifyHardware(ctx->hw_ctx, BC_EVENT_START_CAPTURE);

	if (ctx->state == BC_LINK_READY)
		return crystalhd_hw_start_capture(ctx->hw_ctx);

	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_flush_cap_buffs(struct crystalhd_cmd *ctx,
					  crystalhd_ioctl_data *idata)
{
	struct device *dev = chddev();
	crystalhd_rx_dma_pkt *rpkt;

	if (!ctx || !idata) {
		dev_err(dev, "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	if (!(ctx->state & BC_LINK_CAP_EN))
		return BC_STS_ERR_USAGE;

	/* We should ack flush even when we are in paused/suspend state */
// 	if (!(ctx->state & BC_LINK_READY))
// 		return crystalhd_hw_stop_capture(&ctx->hw_ctx);

	dev_dbg(dev, "number of rx success %u and failure %u\n", ctx->hw_ctx->stats.rx_success, ctx->hw_ctx->stats.rx_errors);
	if(idata->udata.u.FlushRxCap.bDiscardOnly) {
		// just flush without unmapping and then resume
		crystalhd_hw_stop_capture(ctx->hw_ctx, false);
		while((rpkt = crystalhd_dioq_fetch(ctx->hw_ctx->rx_actq)) != NULL)
			crystalhd_dioq_add(ctx->hw_ctx->rx_freeq, rpkt, false, rpkt->pkt_tag);

		while((rpkt = crystalhd_dioq_fetch(ctx->hw_ctx->rx_rdyq)) != NULL)
			crystalhd_dioq_add(ctx->hw_ctx->rx_freeq, rpkt, false, rpkt->pkt_tag);
		crystalhd_hw_start_capture(ctx->hw_ctx);
	} else {
		ctx->state &= ~(BC_LINK_CAP_EN|BC_LINK_FMT_CHG);
		crystalhd_hw_stop_capture(ctx->hw_ctx, true);
	}

	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_get_stats(struct crystalhd_cmd *ctx,
				    crystalhd_ioctl_data *idata)
{
	BC_DTS_STATS *stats;
	struct crystalhd_hw_stats	hw_stats;
	uint32_t pic_width;
	uint8_t flags = 0;
	bool readTxOnly = false;
	unsigned long irqflags;

	if (!ctx || !idata) {
		dev_err(chddev(), "%s: Invalid Arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	crystalhd_hw_stats(ctx->hw_ctx, &hw_stats);

	stats = &idata->udata.u.drvStat;
	stats->drvRLL = hw_stats.rdyq_count;
	stats->drvFLL = hw_stats.freeq_count;
	stats->DrvTotalFrmDropped = hw_stats.rx_errors;
	stats->DrvTotalHWErrs = hw_stats.rx_errors + hw_stats.tx_errors;
	stats->intCount = hw_stats.num_interrupts;
	stats->DrvIgnIntrCnt = hw_stats.num_interrupts -
				hw_stats.dev_interrupts;
	stats->TxFifoBsyCnt = hw_stats.cin_busy;
	stats->pauseCount = hw_stats.pause_cnt;

	/* Indicate that we are checking stats on the input buffer for a single threaded application */
	/* this will prevent the HW from going to low power because we assume that once we have told the application */
	/* that we have space in the HW, the app is going to try to DMA. And if we block that DMA, a single threaded application */
	/* will deadlock */
	if(stats->DrvNextMDataPLD & BC_BIT(31))
	{
		flags |= 0x08;
		// Also for single threaded applications, check to see if we have reduced the power down
		// pause threshold to too low and increase it if the RLL is close to the threshold
/*		if(pDrvStat->drvRLL >= pDevExt->pHwExten->PauseThreshold)
			pDevExt->pHwExten->PauseThreshold++;
		PeekNextTS = TRUE;*/
	}

	/* also indicate that we are just checking stats and not posting */
	/* This allows multi-threaded applications to be placed into low power state */
	/* because eveentually the RX thread will wake up the HW when needed */
	flags |= 0x04;

	if (ctx->pwr_state_change)
		stats->pwr_state_change = 1;
	if (ctx->state & BC_LINK_PAUSED)
		stats->DrvPauseTime = 1;

	// use bit 29 of the input status to indicate that we are trying to read VC1 status
	// This is important for the BCM70012 which uses a different input queue for VC1
	if(stats->DrvcpbEmptySize & BC_BIT(29))
		flags = 0x2;
	// Bit 30 is used to indicate that we are reading only the TX stats and to not touch the Ready list
	if(stats->DrvcpbEmptySize & BC_BIT(30))
		readTxOnly = true;

	spin_lock_irqsave(&ctx->hw_ctx->lock, irqflags);
	ctx->hw_ctx->pfnCheckInputFIFO(ctx->hw_ctx, 0, &stats->DrvcpbEmptySize,
				      false, &flags);
	spin_unlock_irqrestore(&ctx->hw_ctx->lock, irqflags);

	/* status peek ahead to retreive the next decoded frame timestamp */
//	if (!readTxOnly && stats->drvRLL && (stats->DrvNextMDataPLD & BC_BIT(31))) {
	if (!readTxOnly && stats->drvRLL) {
		pic_width = stats->DrvNextMDataPLD & 0xffff;
		stats->DrvNextMDataPLD = 0;
		if (pic_width <= 1920) {
			// get fetch lock to make sure that fetch is not in progress as wel peek
 			if(down_interruptible(&ctx->hw_ctx->fetch_sem))
				goto get_out;
			if(ctx->hw_ctx->pfnPeekNextDeodedFr(ctx->hw_ctx,&stats->DrvNextMDataPLD, &stats->picNumFlags, pic_width)) {
				// Check in case we dropped a picture here
				crystalhd_hw_stats(ctx->hw_ctx, &hw_stats);
				stats->drvRLL = hw_stats.rdyq_count;
				stats->drvFLL = hw_stats.freeq_count;
			}
			up(&ctx->hw_ctx->fetch_sem);
		}
	}

get_out:
	return BC_STS_SUCCESS;
}

static BC_STATUS bc_cproc_reset_stats(struct crystalhd_cmd *ctx,
				      crystalhd_ioctl_data *idata)
{
	crystalhd_hw_stats(ctx->hw_ctx, NULL);

	return BC_STS_SUCCESS;
}

/*=============== Cmd Proc Table.. ======================================*/
static const crystalhd_cmd_tbl_t	g_crystalhd_cproc_tbl[] = {
	{ BCM_IOC_GET_VERSION,		bc_cproc_get_version,	0},
	{ BCM_IOC_GET_HWTYPE,		bc_cproc_get_hwtype,	0},
	{ BCM_IOC_REG_RD,			bc_cproc_reg_rd,	0},
	{ BCM_IOC_REG_WR,			bc_cproc_reg_wr,	0},
	{ BCM_IOC_FPGA_RD,			bc_cproc_link_reg_rd,	0},
	{ BCM_IOC_FPGA_WR,			bc_cproc_link_reg_wr,	0},
	{ BCM_IOC_MEM_RD,			bc_cproc_mem_rd,	0},
	{ BCM_IOC_MEM_WR,			bc_cproc_mem_wr,	0},
	{ BCM_IOC_RD_PCI_CFG,		bc_cproc_cfg_rd,	0},
	{ BCM_IOC_WR_PCI_CFG,		bc_cproc_cfg_wr,	1},
	{ BCM_IOC_FW_DOWNLOAD,		bc_cproc_download_fw,	1},
	{ BCM_IOC_FW_CMD,			bc_cproc_do_fw_cmd,	1},
	{ BCM_IOC_PROC_INPUT,		bc_cproc_proc_input,	1},
	{ BCM_IOC_ADD_RXBUFFS,		bc_cproc_add_cap_buff,	1},
	{ BCM_IOC_FETCH_RXBUFF,		bc_cproc_fetch_frame,	1},
	{ BCM_IOC_START_RX_CAP,		bc_cproc_start_capture,	1},
	{ BCM_IOC_FLUSH_RX_CAP,		bc_cproc_flush_cap_buffs, 1},
	{ BCM_IOC_GET_DRV_STAT,		bc_cproc_get_stats,	0},
	{ BCM_IOC_RST_DRV_STAT,		bc_cproc_reset_stats,	0},
	{ BCM_IOC_NOTIFY_MODE,		bc_cproc_notify_mode,	0},
	{ BCM_IOC_END,				NULL},
};

/*=============== Cmd Proc Functions.. ===================================*/

/**
 * crystalhd_suspend - Power management suspend request.
 * @ctx: Command layer context.
 * @idata: Iodata - required for internal use.
 *
 * Return:
 *	status
 *
 * 1. Set the state to Suspend.
 * 2. Flush the Rx Buffers it will unmap all the buffers and
 *    stop the RxDMA engine.
 * 3. Cancel The TX Io and Stop Dma Engine.
 * 4. Put the DDR in to deep sleep.
 * 5. Stop the hardware putting it in to Reset State.
 *
 * Current gstreamer frame work does not provide any power management
 * related notification to user mode decoder plug-in. As a work-around
 * we pass on the power mangement notification to our plug-in by completing
 * all outstanding requests with BC_STS_IO_USER_ABORT return code.
 */
BC_STATUS crystalhd_suspend(struct crystalhd_cmd *ctx, crystalhd_ioctl_data *idata)
{
	struct device *dev = chddev();
	BC_STATUS sts = BC_STS_SUCCESS;

	if (!ctx || !idata) {
		dev_err(dev, "Invalid Parameters\n");
		return BC_STS_ERROR;
	}

	if (ctx->state & BC_LINK_SUSPEND)
		return BC_STS_SUCCESS;

	if (ctx->state == BC_LINK_INVALID) {
		dev_dbg(dev, "Nothing To Do Suspend Success\n");
		return BC_STS_SUCCESS;
	}

	ctx->state |= BC_LINK_SUSPEND;

	bc_cproc_mark_pwr_state(ctx);

	if (ctx->state & BC_LINK_CAP_EN) {
		idata->udata.u.FlushRxCap.bDiscardOnly = true;
		sts = bc_cproc_flush_cap_buffs(ctx, idata);
		if (sts != BC_STS_SUCCESS)
			return sts;
	}

	if (ctx->tx_list_id) {
		sts = crystalhd_hw_cancel_tx(ctx->hw_ctx, ctx->tx_list_id);
		if (sts != BC_STS_SUCCESS)
			return sts;
	}

	sts = crystalhd_hw_suspend(ctx->hw_ctx);
	if (sts != BC_STS_SUCCESS)
		return sts;

	dev_info(dev, "Crystal HD suspend success\n");

	return BC_STS_SUCCESS;
}

/**
 * crystalhd_resume - Resume frame capture.
 * @ctx: Command layer contextx.
 *
 * Return:
 *	status
 *
 *
 * Resume frame capture.
 *
 * PM_Resume can't resume the playback state back to pre-suspend state
 * because we don't keep video clip related information within driver.
 * To get back to the pre-suspend state App will re-open the device and
 * start a new playback session from the pre-suspend clip position.
 *
 */
BC_STATUS crystalhd_resume(struct crystalhd_cmd *ctx)
{
	dev_info(chddev(), "crystalhd_resume Success %x\n", ctx->state);

	bc_cproc_mark_pwr_state(ctx);

	return BC_STS_SUCCESS;
}

/**
 * crystalhd_user_open - Create application handle.
 * @ctx: Command layer contextx.
 * @user_ctx: User ID context.
 *
 * Return:
 *	status
 *
 * Creates an application specific UID and allocates
 * application specific resources. HW layer initialization
 * is done for the first open request.
 */
BC_STATUS crystalhd_user_open(struct crystalhd_cmd *ctx,
			      struct crystalhd_user **user_ctx)
{
	struct device *dev = chddev();
	struct crystalhd_user *uc;

	if (!ctx || !user_ctx) {
		dev_err(dev, "Invalid arg..\n");
		return BC_STS_INV_ARG;
	}

	uc = bc_cproc_get_uid(ctx);
	if (!uc) {
		dev_info(dev, "No free user context...\n");
		return BC_STS_BUSY;
	}

	dev_info(dev, "Opening new user[%x] handle\n", uc->uid);

	uc->mode = DTS_MODE_INV;
	uc->in_use = 0;

	if(ctx->hw_ctx == NULL) {
		ctx->hw_ctx = (struct crystalhd_hw*)kmalloc(sizeof(struct crystalhd_hw), GFP_KERNEL);
		if(ctx->hw_ctx != NULL)
			memset(ctx->hw_ctx, 0, sizeof(struct crystalhd_hw));
		else
			return BC_STS_ERROR;

		crystalhd_hw_open(ctx->hw_ctx, ctx->adp);
	}

	uc->in_use = 1;

	*user_ctx = uc;

	return BC_STS_SUCCESS;
}

/**
 * crystalhd_user_close - Close application handle.
 * @ctx: Command layer contextx.
 * @uc: User ID context.
 *
 * Return:
 *	status
 *
 * Closer aplication handle and release app specific
 * resources.
 */
BC_STATUS crystalhd_user_close(struct crystalhd_cmd *ctx, struct crystalhd_user *uc)
{
	uint32_t mode = uc->mode;

	ctx->user[uc->uid].mode = DTS_MODE_INV;
	ctx->user[uc->uid].in_use = 0;

	dev_info(chddev(), "Closing user[%x] handle with mode %x\n", uc->uid, mode);

	if (((mode & 0xFF) == DTS_DIAG_MODE) ||
		((mode & 0xFF) == DTS_PLAYBACK_MODE) ||
		((bc_cproc_get_user_count(ctx) == 0) && (ctx->hw_ctx != NULL))) {
		ctx->cin_wait_exit = 1;
		ctx->pwr_state_change = 0;
		// Stop the HW Capture just in case flush did not get called before stop
		crystalhd_hw_stop_capture(ctx->hw_ctx, true);
		crystalhd_hw_free_dma_rings(ctx->hw_ctx);
		crystalhd_destroy_dio_pool(ctx->adp);
		crystalhd_delete_elem_pool(ctx->adp);
		ctx->state = BC_LINK_INVALID;
		crystalhd_hw_close(ctx->hw_ctx, ctx->adp);
		kfree(ctx->hw_ctx);
		ctx->hw_ctx = NULL;
	} /*else if (bc_cproc_get_user_count(ctx)) {
		return BC_STS_SUCCESS;
	}*/

	return BC_STS_SUCCESS;
}

/**
 * crystalhd_setup_cmd_context - Setup Command layer resources.
 * @ctx: Command layer contextx.
 * @adp: Adapter context
 *
 * Return:
 *	status
 *
 * Called at the time of driver load.
 */
BC_STATUS __devinit crystalhd_setup_cmd_context(struct crystalhd_cmd *ctx,
				    struct crystalhd_adp *adp)
{
	struct device *dev = &adp->pdev->dev;
	int i = 0;

	if (!ctx || !adp) {
		dev_err(dev, "%s: Invalid arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	if (ctx->adp)
		dev_dbg(dev, "Resetting Cmd context delete missing..\n");

	ctx->adp = adp;
	for (i = 0; i < BC_LINK_MAX_OPENS; i++) {
		ctx->user[i].uid = i;
		ctx->user[i].in_use = 0;
		ctx->user[i].mode = DTS_MODE_INV;
	}

	ctx->hw_ctx = (struct crystalhd_hw*)kmalloc(sizeof(struct crystalhd_hw), GFP_KERNEL);

	memset(ctx->hw_ctx, 0, sizeof(struct crystalhd_hw));

	/*Open and Close the Hardware to put it in to sleep state*/
	crystalhd_hw_open(ctx->hw_ctx, ctx->adp);
	crystalhd_hw_close(ctx->hw_ctx, ctx->adp);
	kfree(ctx->hw_ctx);
	ctx->hw_ctx = NULL;

	return BC_STS_SUCCESS;
}

/**
 * crystalhd_delete_cmd_context - Release Command layer resources.
 * @ctx: Command layer contextx.
 *
 * Return:
 *	status
 *
 * Called at the time of driver un-load.
 */
BC_STATUS __devexit crystalhd_delete_cmd_context(struct crystalhd_cmd *ctx)
{
	dev_dbg(chddev(), "Deleting Command context..\n");

	ctx->adp = NULL;

	return BC_STS_SUCCESS;
}

/**
 * crystalhd_get_cmd_proc  - Cproc table lookup.
 * @ctx: Command layer contextx.
 * @cmd: IOCTL command code.
 * @uc: User ID context.
 *
 * Return:
 *	command proc function pointer
 *
 * This function checks the process context, application's
 * mode of operation and returns the function pointer
 * from the cproc table.
 */
crystalhd_cmd_proc crystalhd_get_cmd_proc(struct crystalhd_cmd *ctx, uint32_t cmd,
				      struct crystalhd_user *uc)
{
	struct device *dev = chddev();
	crystalhd_cmd_proc cproc = NULL;
	unsigned int i, tbl_sz;

	if (!ctx) {
		dev_err(dev, "Invalid arg.. Cmd[%d]\n", cmd);
		return NULL;
	}

	if ((cmd != BCM_IOC_GET_DRV_STAT) && (ctx->state & BC_LINK_SUSPEND)) {
		dev_err(dev, "Invalid State [suspend Set].. Cmd[%d]\n", cmd);
		return NULL;
	}

	tbl_sz = sizeof(g_crystalhd_cproc_tbl) / sizeof(crystalhd_cmd_tbl_t);
	for (i = 0; i < tbl_sz; i++) {
		if (g_crystalhd_cproc_tbl[i].cmd_id == cmd) {
			if ((uc->mode == DTS_MONITOR_MODE) &&
			    (g_crystalhd_cproc_tbl[i].block_mon)) {
				dev_info(dev, "Blocking cmd %d \n", cmd);
				break;
			}
			cproc = g_crystalhd_cproc_tbl[i].cmd_proc;
			break;
		}
	}

	return cproc;
}

/**
 * crystalhd_cmd_interrupt - ISR entry point
 * @ctx: Command layer contextx.
 *
 * Return:
 *	TRUE: If interrupt from CrystalHD device.
 *
 *
 * ISR entry point from OS layer.
 */
bool crystalhd_cmd_interrupt(struct crystalhd_cmd *ctx)
{
	if (!ctx) {
		printk(KERN_ERR "%s: Invalid arg..\n", __func__);
		return false;
	}

	// If HW has not been initialized then all interrupts are spurious
	if ((ctx->hw_ctx == NULL) || (ctx->hw_ctx->pfnFindAndClearIntr == NULL))
		return false;

	return ctx->hw_ctx->pfnFindAndClearIntr(ctx->adp, ctx->hw_ctx);
}
