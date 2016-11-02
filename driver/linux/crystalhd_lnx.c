/***************************************************************************
  BCM70010 Linux driver
  Copyright (c) 2005-2009, Broadcom Corporation.

  This driver is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, version 2 of the License.

  This driver is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this driver.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************/

#include <linux/version.h>

#include "crystalhd_lnx.h"

static struct class *crystalhd_class;

static struct crystalhd_adp *g_adp_info;

extern int bc_get_userhandle_count(struct crystalhd_cmd *ctx);

struct device *chddev(void)
{
	return &g_adp_info->pdev->dev;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
loff_t noop_llseek(struct file *file, loff_t offset, int origin)
{
	return file->f_pos;
}
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 18)
static irqreturn_t chd_dec_isr(int irq, void *arg)
#else
static irqreturn_t chd_dec_isr(int irq, void *arg, struct pt_regs *r)
#endif
{
	struct crystalhd_adp *adp = (struct crystalhd_adp *) arg;
	int rc = 0;
	if (adp)
		rc = crystalhd_cmd_interrupt(&adp->cmds);

	return IRQ_RETVAL(rc);
}

static int chd_dec_enable_int(struct crystalhd_adp *adp)
{
	int rc = 0;

	if (!adp || !adp->pdev) {
		printk(KERN_ERR "%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	rc = pci_enable_msi(adp->pdev);
	if(rc != 0)
		dev_err(&adp->pdev->dev, "MSI request failed..\n");
	else
		adp->msi = 1;

	rc = request_irq(adp->pdev->irq, chd_dec_isr, IRQF_SHARED,
			 adp->name, (void *)adp);

	if (rc != 0) {
		dev_err(&adp->pdev->dev, "Interrupt request failed..\n");
		if(adp->msi) {
			pci_disable_msi(adp->pdev);
			adp->msi = 0;
		}
	}

	return rc;
}

static int chd_dec_disable_int(struct crystalhd_adp *adp)
{
	if (!adp || !adp->pdev) {
		printk(KERN_ERR "%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	free_irq(adp->pdev->irq, adp);

	if (adp->msi) {
		pci_disable_msi(adp->pdev);
		adp->msi = 0;
	}

	return 0;
}

crystalhd_ioctl_data *chd_dec_alloc_iodata(struct crystalhd_adp *adp, bool isr)
{
	unsigned long flags = 0;
	crystalhd_ioctl_data *temp;

	if (!adp)
		return NULL;

	spin_lock_irqsave(&adp->lock, flags);

	temp = adp->idata_free_head;
	if (temp) {
		adp->idata_free_head = adp->idata_free_head->next;
		memset(temp, 0, sizeof(*temp));
	}

	spin_unlock_irqrestore(&adp->lock, flags);
	return temp;
}

void chd_dec_free_iodata(struct crystalhd_adp *adp, crystalhd_ioctl_data *iodata,
			 bool isr)
{
	unsigned long flags = 0;

	if (!adp || !iodata)
		return;

	spin_lock_irqsave(&adp->lock, flags);
	iodata->next = adp->idata_free_head;
	adp->idata_free_head = iodata;
	spin_unlock_irqrestore(&adp->lock, flags);
}

static inline int crystalhd_user_data(unsigned long ud, void *dr, int size, int set)
{
	int rc;

	if (!ud || !dr) {
		dev_err(chddev(), "%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	if (set)
		rc = copy_to_user((void *)ud, dr, size);
	else
		rc = copy_from_user(dr, (void *)ud, size);

	if (rc) {
		dev_err(chddev(), "Invalid args for command\n");
		rc = -EFAULT;
	}

	return rc;
}

static int chd_dec_fetch_cdata(struct crystalhd_adp *adp, crystalhd_ioctl_data *io,
			       uint32_t m_sz, unsigned long ua)
{
	unsigned long ua_off;
	int rc = 0;

	if (!adp || !io || !ua || !m_sz) {
		dev_err(chddev(), "Invalid Arg!!\n");
		return -EINVAL;
	}

	io->add_cdata = vmalloc(m_sz);
	if (!io->add_cdata) {
		dev_err(chddev(), "kalloc fail for sz:%x\n", m_sz);
		return -ENOMEM;
	}

	io->add_cdata_sz = m_sz;
	ua_off = ua + sizeof(io->udata);
	rc = crystalhd_user_data(ua_off, io->add_cdata, io->add_cdata_sz, 0);
	if (rc) {
		dev_err(chddev(), "failed to pull add_cdata sz:%x "
			"ua_off:%x\n", io->add_cdata_sz,
			(unsigned int)ua_off);
		kfree(io->add_cdata);
		io->add_cdata = NULL;
		return -ENODATA;
	}

	return rc;
}

static int chd_dec_release_cdata(struct crystalhd_adp *adp,
				 crystalhd_ioctl_data *io, unsigned long ua)
{
	unsigned long ua_off;
	int rc;

	if (!adp || !io || !ua) {
		dev_err(chddev(), "Invalid Arg!!\n");
		return -EINVAL;
	}

	if (io->cmd != BCM_IOC_FW_DOWNLOAD) {
		ua_off = ua + sizeof(io->udata);
		rc = crystalhd_user_data(ua_off, io->add_cdata,
					io->add_cdata_sz, 1);
		if (rc) {
			dev_err(chddev(), "failed to push add_cdata sz:%x "
				"ua_off:%x\n", io->add_cdata_sz,
				(unsigned int)ua_off);
			return -ENODATA;
		}
	}

	if (io->add_cdata) {
		vfree(io->add_cdata);
		io->add_cdata = NULL;
	}

	return 0;
}

static int chd_dec_proc_user_data(struct crystalhd_adp *adp,
				  crystalhd_ioctl_data *io,
				  unsigned long ua, int set)
{
	int rc;
	uint32_t m_sz = 0;

	if (!adp || !io || !ua) {
		dev_err(chddev(), "Invalid Arg!!\n");
		return -EINVAL;
	}

	rc = crystalhd_user_data(ua, &io->udata, sizeof(io->udata), set);
	if (rc) {
		dev_err(chddev(), "failed to %s iodata\n",
			(set ? "set" : "get"));
		return rc;
	}

	switch (io->cmd) {
	case BCM_IOC_MEM_RD:
	case BCM_IOC_MEM_WR:
	case BCM_IOC_FW_DOWNLOAD:
		m_sz = io->udata.u.devMem.NumDwords * 4;
		if (set)
			rc = chd_dec_release_cdata(adp, io, ua);
		else
			rc = chd_dec_fetch_cdata(adp, io, m_sz, ua);
		break;
	default:
		break;
	}

	return rc;
}

static int chd_dec_api_cmd(struct crystalhd_adp *adp, unsigned long ua,
			   uint32_t uid, uint32_t cmd, crystalhd_cmd_proc func)
{
	int rc;
	crystalhd_ioctl_data *temp;
	BC_STATUS sts = BC_STS_SUCCESS;

	temp = chd_dec_alloc_iodata(adp, 0);
	if (!temp) {
		dev_err(chddev(), "Failed to get iodata..\n");
		return -EINVAL;
	}

	temp->u_id = uid;
	temp->cmd  = cmd;

	rc = chd_dec_proc_user_data(adp, temp, ua, 0);
	if (!rc) {
		if(func == NULL)
			sts = BC_STS_PWR_MGMT; /* Can only happen when we are in suspend state */
		else
			sts = func(&adp->cmds, temp);
		if (sts == BC_STS_PENDING)
			sts = BC_STS_NOT_IMPL;
		temp->udata.RetSts = sts;
		rc = chd_dec_proc_user_data(adp, temp, ua, 1);
	}

	if (temp) {
		chd_dec_free_iodata(adp, temp, 0);
		temp = NULL;
	}

	return rc;
}

/* API interfaces */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static int chd_dec_ioctl(struct inode *in, struct file *fd,
			 unsigned int cmd, unsigned long ua)
#else
static long chd_dec_ioctl(struct file *fd,
			  unsigned int cmd, unsigned long ua)
#endif
{
	struct crystalhd_adp *adp = chd_get_adp();
	struct device *dev = &adp->pdev->dev;
	crystalhd_cmd_proc cproc;
	struct crystalhd_user *uc;

	dev_dbg(dev, "Entering %s\n", __func__);

	if (!adp || !fd) {
		dev_err(chddev(), "Invalid adp\n");
		return -EINVAL;
	}

	uc = fd->private_data;
	if (!uc) {
		dev_err(chddev(), "Failed to get uc\n");
		return -ENODATA;
	}

	cproc = crystalhd_get_cmd_proc(&adp->cmds, cmd, uc);
	if (!cproc && !(adp->cmds.state & BC_LINK_SUSPEND)) {
		dev_err(chddev(), "Unhandled command: %d\n", cmd);
		return -EINVAL;
	}

	return chd_dec_api_cmd(adp, ua, uc->uid, cmd, cproc);
}

static int chd_dec_open(struct inode *in, struct file *fd)
{
	struct crystalhd_adp *adp = chd_get_adp();
	struct device *dev = &adp->pdev->dev;
	int rc = 0;
	BC_STATUS sts = BC_STS_SUCCESS;
	struct crystalhd_user *uc = NULL;

	dev_dbg(dev, "Entering %s\n", __func__);
	if (!adp) {
		dev_err(dev, "Invalid adp\n");
		return -EINVAL;
	}

	if (adp->cfg_users >= BC_LINK_MAX_OPENS) {
		dev_info(dev, "Already in use.%d\n", adp->cfg_users);
		return -EBUSY;
	}

	sts = crystalhd_user_open(&adp->cmds, &uc);
	if (sts != BC_STS_SUCCESS) {
		dev_err(dev, "cmd_user_open - %d\n", sts);
		rc = -EBUSY;
	}
	else {
		adp->cfg_users++;
		fd->private_data = uc;
	}

	return rc;
}

static int chd_dec_close(struct inode *in, struct file *fd)
{
	struct crystalhd_adp *adp = chd_get_adp();
	struct device *dev = &adp->pdev->dev;
	struct crystalhd_cmd *ctx = &adp->cmds;
	struct crystalhd_user *uc;
	uint32_t mode;

	dev_dbg(dev, "Entering %s\n", __func__);
	if (!adp) {
		dev_err(dev, "Invalid adp\n");
		return -EINVAL;
	}

	uc = fd->private_data;
	if (!uc) {
		dev_err(dev, "Failed to get uc\n");
		return -ENODATA;
	}

	/* Check and close only if we have not flush/closed before */
	/* This is needed because release is not guarenteed to be called immediately on close,
	 * if duplicate file handles exist due to fork etc. This causes problems with close and re-open
	 of the device immediately */

	if(uc->in_use) {
		mode = uc->mode;

		ctx->user[uc->uid].mode = DTS_MODE_INV;
		ctx->user[uc->uid].in_use = 0;

		dev_info(chddev(), "Closing user[%x] handle with mode %x\n", uc->uid, mode);

		if (((mode & 0xFF) == DTS_DIAG_MODE) ||
			((mode & 0xFF) == DTS_PLAYBACK_MODE) ||
			((bc_get_userhandle_count(ctx) == 0) && (ctx->hw_ctx != NULL))) {
			ctx->cin_wait_exit = 1;
			ctx->pwr_state_change = BC_HW_RUNNING;
			/* Stop the HW Capture just in case flush did not get called before stop */
			/* And only if we had actually started it */
			if(ctx->hw_ctx->rx_freeq != NULL) {
				crystalhd_hw_stop_capture(ctx->hw_ctx, true);
				crystalhd_hw_free_dma_rings(ctx->hw_ctx);
			}
			if(ctx->adp->fill_byte_pool)
				crystalhd_destroy_dio_pool(ctx->adp);
			if(ctx->adp->elem_pool_head)
				crystalhd_delete_elem_pool(ctx->adp);
			ctx->state = BC_LINK_INVALID;
			crystalhd_hw_close(ctx->hw_ctx, ctx->adp);
			kfree(ctx->hw_ctx);
			ctx->hw_ctx = NULL;
		}

		uc->in_use = 0;

		if(adp->cfg_users > 0)
			adp->cfg_users--;
	}

	return 0;
}

static const struct file_operations chd_dec_fops = {
	.owner		= THIS_MODULE,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
	.ioctl		= chd_dec_ioctl,
#else
	.unlocked_ioctl	= chd_dec_ioctl,
#endif
	.open		= chd_dec_open,
	.release	= chd_dec_close,
	.llseek		= noop_llseek,
};

static int __init chd_dec_init_chdev(struct crystalhd_adp *adp)
{
	struct device *xdev = &adp->pdev->dev;
	struct device *dev;
	crystalhd_ioctl_data *temp;
	int rc = -ENODEV, i = 0;

	if (!adp)
		goto fail;

	adp->chd_dec_major = register_chrdev(0, CRYSTALHD_API_NAME,
					     &chd_dec_fops);
	if (adp->chd_dec_major < 0) {
		dev_err(xdev, "Failed to create config dev\n");
		rc = adp->chd_dec_major;
		goto fail;
	}

	/* register crystalhd class */
	crystalhd_class = class_create(THIS_MODULE, "crystalhd");
	if (IS_ERR(crystalhd_class)) {
		dev_err(xdev, "failed to create class\n");
		goto fail;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
	dev = device_create(crystalhd_class, NULL, MKDEV(adp->chd_dec_major, 0),
			    NULL, "crystalhd");
#else
	dev = device_create(crystalhd_class, NULL, MKDEV(adp->chd_dec_major, 0),
			    "crystalhd");
#endif
	if (IS_ERR(dev)) {
		dev_err(xdev, "failed to create device\n");
		goto device_create_fail;
	}

/*	rc = crystalhd_create_elem_pool(adp, BC_LINK_ELEM_POOL_SZ); */
/*	if (rc) { */
/*		dev_err(xdev, "failed to create device\n"); */
/*		goto elem_pool_fail; */
/*	} */

	/* Allocate general purpose ioctl pool. */
	for (i = 0; i < CHD_IODATA_POOL_SZ; i++) {
		temp = kzalloc(sizeof(crystalhd_ioctl_data), GFP_KERNEL);
		if (!temp) {
			dev_err(xdev, "ioctl data pool kzalloc failed\n");
			rc = -ENOMEM;
			goto kzalloc_fail;
		}
		/* Add to global pool.. */
		chd_dec_free_iodata(adp, temp, 0);
	}

	return 0;

kzalloc_fail:
	/*crystalhd_delete_elem_pool(adp); */
/*elem_pool_fail: */
	device_destroy(crystalhd_class, MKDEV(adp->chd_dec_major, 0));
device_create_fail:
	class_destroy(crystalhd_class);
fail:
	return rc;
}

static void chd_dec_release_chdev(struct crystalhd_adp *adp)
{
	crystalhd_ioctl_data *temp = NULL;
	if (!adp)
		return;

	if (adp->chd_dec_major > 0) {
		/* unregister crystalhd class */
		device_destroy(crystalhd_class, MKDEV(adp->chd_dec_major, 0));
		unregister_chrdev(adp->chd_dec_major, CRYSTALHD_API_NAME);
		dev_info(chddev(), "released api device - %d\n",
		       adp->chd_dec_major);
		class_destroy(crystalhd_class);
	}
	adp->chd_dec_major = 0;

	/* Clear iodata pool.. */
	do {
		temp = chd_dec_alloc_iodata(adp, 0);
		kfree(temp);
	} while (temp);

	/*crystalhd_delete_elem_pool(adp); */
}

static int __init chd_pci_reserve_mem(struct crystalhd_adp *pinfo)
{
	struct device *dev = &pinfo->pdev->dev;
	int rc;

	uint32_t bar0		= pci_resource_start(pinfo->pdev, 0);
	uint32_t i2o_len	= pci_resource_len(pinfo->pdev, 0);

	uint32_t bar2		= pci_resource_start(pinfo->pdev, 2);
	uint32_t mem_len	= pci_resource_len(pinfo->pdev, 2);

	dev_dbg(dev, "bar0:0x%x-0x%08x  bar2:0x%x-0x%08x\n",
	        bar0, i2o_len, bar2, mem_len);

	/* bar-0 */
	pinfo->pci_i2o_start = bar0;
	pinfo->pci_i2o_len   = i2o_len;

	/* bar-2 */
	pinfo->pci_mem_start = bar2;
	pinfo->pci_mem_len   = mem_len;

	/* pdev */
	rc = pci_request_regions(pinfo->pdev, pinfo->name);
	if (rc < 0) {
		printk(KERN_ERR "Region request failed: %d\n", rc);
		return rc;
	}
	
	pinfo->i2o_addr = pci_ioremap_bar(pinfo->pdev, 0);
	if (!pinfo->i2o_addr) {
		printk(KERN_ERR "Failed to remap i2o region...\n");
		return -ENOMEM;
	}
	
	pinfo->mem_addr = pci_ioremap_bar(pinfo->pdev, 2);
	if (!pinfo->mem_addr) {
		printk(KERN_ERR "Failed to remap mem region...\n");
		return -ENOMEM;
	}

	dev_dbg(dev, "i2o_addr:0x%08lx   Mapped addr:0x%08lx  \n",
	        (unsigned long)pinfo->i2o_addr, (unsigned long)pinfo->mem_addr);

	return 0;
}

static void chd_pci_release_mem(struct crystalhd_adp *pinfo)
{
	if (!pinfo)
		return;

	if (pinfo->mem_addr)
		iounmap(pinfo->mem_addr);

	if (pinfo->i2o_addr)
		iounmap(pinfo->i2o_addr);

	pci_release_regions(pinfo->pdev);
}


static void __exit chd_dec_pci_remove(struct pci_dev *pdev)
{
	struct crystalhd_adp *pinfo;
	BC_STATUS sts = BC_STS_SUCCESS;

	dev_dbg(chddev(), "Entering %s\n", __func__);

	pinfo = (struct crystalhd_adp *) pci_get_drvdata(pdev);
	if (!pinfo) {
		dev_err(chddev(), "could not get adp\n");
		return;
	}

	sts = crystalhd_delete_cmd_context(&pinfo->cmds);
	if (sts != BC_STS_SUCCESS)
		dev_err(chddev(), "cmd delete :%d\n", sts);

	chd_dec_release_chdev(pinfo);

	chd_dec_disable_int(pinfo);

	chd_pci_release_mem(pinfo);
	pci_disable_device(pinfo->pdev);

	kfree(pinfo);
	g_adp_info = NULL;
}

static int __init chd_dec_pci_probe(struct pci_dev *pdev,
			     const struct pci_device_id *entry)
{
	struct device *dev = &pdev->dev;
	struct crystalhd_adp *pinfo;
	int rc;
	BC_STATUS sts = BC_STS_SUCCESS;

	dev_info(dev, "Starting Device:0x%04x\n", pdev->device);

	pinfo = kzalloc(sizeof(struct crystalhd_adp), GFP_KERNEL);
	if (!pinfo) {
		dev_err(dev, "%s: Failed to allocate memory\n", __func__);
		rc = -ENOMEM;
		goto out;
	}

	pinfo->pdev = pdev;

	rc = pci_enable_device(pdev);
	if (rc) {
		dev_err(dev, "%s: Failed to enable PCI device\n", __func__);
		goto free_priv;
	}

	snprintf(pinfo->name, sizeof(pinfo->name), "crystalhd_pci_e:%d:%d:%d",
		 pdev->bus->number, PCI_SLOT(pdev->devfn),
		 PCI_FUNC(pdev->devfn));

	rc = chd_pci_reserve_mem(pinfo);
	if (rc) {
		dev_err(dev, "%s: Failed to set up memory regions.\n",
			__func__);
		goto disable_device;
	}

	pinfo->present	= 1;
	pinfo->drv_data = entry->driver_data;

	/* Setup adapter level lock.. */
	spin_lock_init(&pinfo->lock);

	/* setup api stuff.. */
	rc = chd_dec_init_chdev(pinfo);
	if (rc)
		goto release_mem;

	rc = chd_dec_enable_int(pinfo);
	if (rc) {
		dev_err(dev, "%s: _enable_int err:%d\n", __func__, rc);
		goto cleanup_chdev;
	}

	/* Set dma mask... */
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
		pinfo->dmabits = 64;
	} else if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		pinfo->dmabits = 32;
	} else {
		dev_err(dev, "%s: Unabled to setup DMA %d\n", __func__, rc);
		rc = -ENODEV;
		goto cleanup_int;
	}

	sts = crystalhd_setup_cmd_context(&pinfo->cmds, pinfo);
	if (sts != BC_STS_SUCCESS) {
		dev_err(dev, "%s: cmd setup :%d\n", __func__, sts);
		rc = -ENODEV;
		goto cleanup_int;
	}

	pci_set_master(pdev);

	pci_set_drvdata(pdev, pinfo);

	g_adp_info = pinfo;

out:
	return rc;
cleanup_int:
	chd_dec_disable_int(pinfo);
cleanup_chdev:
	chd_dec_release_chdev(pinfo);
release_mem:
	chd_pci_release_mem(pinfo);
disable_device:
	pci_disable_device(pdev);
free_priv:
	kfree(pdev);
	goto out;
}

int chd_dec_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct crystalhd_adp *adp;
	struct device *dev = &pdev->dev;
	crystalhd_ioctl_data *temp;
	BC_STATUS sts = BC_STS_SUCCESS;

	adp = (struct crystalhd_adp *)pci_get_drvdata(pdev);
	if (!adp) {
		dev_err(dev, "%s: could not get adp\n", __func__);
		return -ENODEV;
	}

	temp = chd_dec_alloc_iodata(adp, false);
	if (!temp) {
		dev_err(dev, "could not get ioctl data\n");
		return -ENODEV;
	}

	sts = crystalhd_suspend(&adp->cmds, temp);
	if (sts != BC_STS_SUCCESS) {
		dev_err(dev, "Crystal HD Suspend %d\n", sts);
		chd_dec_free_iodata(adp, temp, false);
		return -ENODEV;
	}

	chd_dec_free_iodata(adp, temp, false);
	chd_dec_disable_int(adp);
	pci_save_state(pdev);

	/* Disable IO/bus master/irq router */
	pci_disable_device(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));
	return 0;
}

int chd_dec_pci_resume(struct pci_dev *pdev)
{
	struct crystalhd_adp *adp;
	struct device *dev = &pdev->dev;
	BC_STATUS sts = BC_STS_SUCCESS;
	int rc;

	adp = (struct crystalhd_adp *)pci_get_drvdata(pdev);
	if (!adp) {
		dev_err(dev, "%s: could not get adp\n", __func__);
		return -ENODEV;
	}

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	/* device's irq possibly is changed, driver should take care */
	if (pci_enable_device(pdev)) {
		dev_err(dev, "Failed to enable PCI device\n");
		return 1;
	}

	pci_set_master(pdev);

	rc = chd_dec_enable_int(adp);
	if (rc) {
		dev_err(dev, "_enable_int err:%d\n", rc);
		pci_disable_device(pdev);
		return -ENODEV;
	}

	sts = crystalhd_resume(&adp->cmds);
	if (sts != BC_STS_SUCCESS) {
		dev_err(dev, "Crystal HD Resume %d\n", sts);
		pci_disable_device(pdev);
		return -ENODEV;
	}

	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 24)
static struct pci_device_id chd_dec_pci_id_table[] = {
	{ PCI_VDEVICE(BROADCOM, 0x1612), 8 },
	{ PCI_VDEVICE(BROADCOM, 0x1615), 8 },
	{ 0, },
};
#else
static struct pci_device_id chd_dec_pci_id_table[] = {
/*	vendor, device, subvendor, subdevice, class, classmask, driver_data */
	{ 0x14e4, 0x1612, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 8 },
	{ 0x14e4, 0x1615, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 8 },
	{ 0, },
};
#endif
MODULE_DEVICE_TABLE(pci, chd_dec_pci_id_table);

static struct pci_driver bc_chd_driver __refdata = {
	.name     = "crystalhd",
	.probe    = chd_dec_pci_probe,
	.remove   = __exit_p(chd_dec_pci_remove),
	.id_table = chd_dec_pci_id_table,
	.suspend  = chd_dec_pci_suspend,
	.resume   = chd_dec_pci_resume
};

struct crystalhd_adp *chd_get_adp(void)
{
	return g_adp_info;
}

static int __init chd_dec_module_init(void)
{
	int rc;

	printk(KERN_DEBUG "Loading crystalhd v%d.%d.%d\n",
	       crystalhd_kmod_major, crystalhd_kmod_minor, crystalhd_kmod_rev);

	rc = pci_register_driver(&bc_chd_driver);

	if (rc < 0)
		printk(KERN_ERR "%s: Could not find any devices. err:%d\n",
		       __func__, rc);

	return rc;
}
module_init(chd_dec_module_init);

static void __exit chd_dec_module_cleanup(void)
{
	printk(KERN_DEBUG "Unloading crystalhd %d.%d.%d\n",
	       crystalhd_kmod_major, crystalhd_kmod_minor, crystalhd_kmod_rev);

	pci_unregister_driver(&bc_chd_driver);
}
module_exit(chd_dec_module_cleanup);

MODULE_AUTHOR("Naren Sankar <nsankar@broadcom.com>");
MODULE_AUTHOR("Prasad Bolisetty <prasadb@broadcom.com>");
MODULE_DESCRIPTION(CRYSTAL_HD_NAME);
MODULE_LICENSE("GPL");
MODULE_ALIAS("crystalhd");
