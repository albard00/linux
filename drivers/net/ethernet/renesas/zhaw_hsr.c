/*
 * ZHAW HSR driver.
 * Copyright (C) 2017 Renesas Electronics Europe Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_net.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sysctrl-rzn1.h>

//#include "hsr_table.h"
#include "zhaw_hsr_registers.h"
#include "zhaw_hsr.h"

struct zhaw_hsr {
	struct device *dev;
	struct net_device * ndev;
	void __iomem *regs_cpu;
	void __iomem *regs;
	void __iomem *regs_ptp;
	struct clk *clk_100mhz;
	struct clk *clk_50mhz;
	struct clk *clk_pclk;
};


/******************************************************************************
 * Global parameters
 ******************************************************************************/
static DEFINE_SPINLOCK(ioread_lock);

struct hsr_reg_attribute {
	struct device_attribute dev_attr;
	int reg;
	u32 counter_bias; /* for counters only: the value that reads as zero (software counter reset) */
};

#define to_dev(obj) container_of(obj, struct device, kobj)
#define to_dev_attr(_attr) container_of(_attr, struct device_attribute, attr)
#define to_hsr_attr(_attr) container_of(_attr, struct hsr_reg_attribute, dev_attr)

/******************************************************************************
 * Private functions implementations
 ******************************************************************************/
/**
 * @fn ssize_t XXXX_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
 * @brief Function to show the register's content
 * 
 * These functions gets called if you read a file in the virtual file system. \n
 * Each file has its own read function, named after the filename + suffix _show.
 * @param   *kobj Pointer to the kobject struct
 * @param   *attr Pointer th the kobj_attribute struct 
 * @param   *buf char pointer to the buffer
 * @return   size of the read chars
 */
 
ssize_t hsr_register_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
	struct hsr_reg_attribute *hsr = to_hsr_attr(attr);
    //struct lre_kobj_attribute * lre_attr = (struct lre_kobj_attribute*)attr;
    u32 val;
    val = ioread32(data->regs + 4* hsr->reg);
    return snprintf(buf, PAGE_SIZE, "0x%08x\n", val);
}
/******************************************************************************/
ssize_t lre_counter_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
	struct hsr_reg_attribute *hsr = to_hsr_attr(attr);
    u32 val = ioread32(data->regs + 4* hsr->reg);
    val -= hsr->counter_bias;
    return snprintf(buf, PAGE_SIZE, "%u\n", val);
}
/******************************************************************************/
ssize_t lreNodeType_show(struct device *dev,
                         struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
	u32 val = ioread32(data->regs + (R_VER * 4));                          /* prpmode = 1 */
	val = (val >> 30) & 0x03;                               /* hsr = 2 */
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
/******************************************************************************/
ssize_t lreVersionName_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
	
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val_main,val_sub;
    val_main = (ioread32(data->regs + (R_VER * 4)) & HVM_BIT) >> 8;
    val_sub  = (ioread32(data->regs + (R_VER * 4)) & HVS_BIT);
    return snprintf(buf, PAGE_SIZE, "ZHAW Version %d.%d\n", val_main, val_sub);
}
/******************************************************************************/
ssize_t lreMacAddress_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val_hw,val_lw;
    unsigned char mac_addr[6];
    val_hw = ioread32(data->regs + R_MACADH * 4);
    val_lw = ioread32(data->regs + R_MACADL * 4);
    mac_addr[0] = (val_hw >>  8);
    mac_addr[1] = (val_hw >>  0);
    mac_addr[2] = (val_lw >> 24);
    mac_addr[3] = (val_lw >> 16);
    mac_addr[4] = (val_lw >>  8);
    mac_addr[5] = (val_lw >>  0);
    return snprintf(buf, PAGE_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x\n", 
                                    mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);
}
/******************************************************************************/
ssize_t lrePortAdminStateA_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val = ioread32(data->regs + R_PEN * 4);
    val &= (1<<PORT_A) | ((1<<PORT_A) << 4);                /* rx or tx enabled */
    if (val)                                                /* active = 2 */
        val = 2;
    else                                                    /* notActive = 1 */
        val = 1;
    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
/******************************************************************************/
ssize_t lrePortAdminStateB_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val = ioread32(data->regs + 4 * R_PEN);
    val &= (1<<PORT_B) | ((1<<PORT_B) << 4);                /* rx or tx enabled */
    if (val)                                                /* active = 2 */
        val = 2;
    else                                                    /* notActive = 1 */
        val = 1;
    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
/******************************************************************************/
// ssize_t lreLinkStatusA_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
// {
//     u32 val = 0xdeadbeef;
//     val = lre_ioread32(lreLinkStatusA);
//     return snprintf(buf, PAGE_SIZE, "0x%08x\n", val);
// }
// ssize_t lreLinkStatusB_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
// {
//     u32 val = 0xdeadbeef;
//     val = lre_ioread32(lreLinkStatusB);
//     return snprintf(buf, PAGE_SIZE, "0x%08x\n", val);
// }
/******************************************************************************/
ssize_t lreDuplicateDiscard_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    u32 val = 2; /* we always use a dulplicate discard algorithm */
    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
/******************************************************************************/
ssize_t lreTransparentReception_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    u32 val = 1; /* the lre always removes the RCT */
    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
/******************************************************************************/
ssize_t lreHsrLREMode_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val = ioread32(data->regs + 4 * R_PEN) & (BIT18 | BIT19 | BIT20);
    val >>= 18;
    switch (val) {
        case OPERATION_MODE_H:
            val = 1;
            break;
        case OPERATION_MODE_N:
            val = 2;
            break;
        case OPERATION_MODE_T:
            val = 3;
            break;
        case OPERATION_MODE_U:
            val = 4;
            break;
        case OPERATION_MODE_M:
            val = 5;
            break;
        default:
            return -EINVAL;
    }
    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
/******************************************************************************/
ssize_t lreSwitchingEndNode_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 redbox_mode = (ioread32(data->regs + 4 * R_PEN) & (BIT29 | BIT30 | BIT31)) >> 29;
    u32 node_type   = (ioread32(data->regs + 4 * R_VER) & (BIT30 | BIT31)) >> 30;
    int val = -1;
    if (node_type == NODE_TYPE_PRP1) {
        val = 3;
    } else if (node_type == NODE_TYPE_HSR) {
        if (redbox_mode == REDBOX_MODE_SAN) {
            val = 4;
        } else if (redbox_mode == REDBOX_MODE_PRP_A) {
            val = 7;
        } else if (redbox_mode == REDBOX_MODE_PRP_B) {
            val = 8;
        } else if (redbox_mode == REDBOX_MODE_HSR) {
            val = 6;
        }
    }
    if (val == -1) return -EINVAL;
    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
/******************************************************************************/
ssize_t lreCPURedBoxIdentity_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val = ioread32(data->regs + 4 * R_PEN) & (BIT25 | BIT26 | BIT27 | BIT28);
    val >>= 25;
    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
/******************************************************************************/
ssize_t lreRedBoxIdentity_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val = ioread32(data->regs + 4 * R_PEN) & (BIT21 | BIT22 | BIT23 | BIT24);
    val >>= 21;
    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
/******************************************************************************/
ssize_t lreDuplicateDetectionLength_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 len = 256 * ((ioread32(data->regs + 4 * R_DD_AGT) >> 27) & 0xf);
    return snprintf(buf, PAGE_SIZE, "%d\n", len);
}
/******************************************************************************/
ssize_t lreDuplicateDetectionAging_ms_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 len = 256 * ((ioread32(data->regs + 4 * R_DD_AGT) >> 27) & 0xf);
    u32 agt = ioread32(data->regs + 4 * R_DD_AGT) & ((1<<18)-1);
    // aging time = 20ns * agt * 2^LENGTH_LOG2
    // agt_milliseconds * 1000000ns = 20ns * agt * len
    u32 tmp = 1000000/len;
    u32 agt_milliseconds = (agt * 20 + tmp/2) / tmp;
    return snprintf(buf, PAGE_SIZE, "%d\n", agt_milliseconds);
}
/******************************************************************************/
ssize_t lreProxyNodeTableClear_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    /* this is specified read/write in the MIB, but reading doesn't make much sense */
    return snprintf(buf, PAGE_SIZE, "0\n");
}
/******************************************************************************/
ssize_t lreCntProxyNodes_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u16 i;
    u16 valid_index_cnt = 0;
    u32 table_pointer;
    u32 table_size;
    u32 tmp_hw;
    unsigned long flags;
      
    spin_lock_irqsave(&ioread_lock, flags); 
    table_pointer = ioread32(data->regs + 4 * R_PNT_S) & PNTP_BIT;       /* proxy node table pointer */
    table_size = ioread32(data->regs + 4 * R_PNT_S) & PNTS_BIT;
    table_size >>= 16;
    
    while (table_pointer != 0) {                            /* "set" table pointer to 0 */
        ioread32(data->regs + 4 * R_PNT_D);
        barrier();
        table_pointer = ioread32(data->regs + 4 * R_PNT_S) & PNTP_BIT;
    }
    
    for (i=0;i<table_size;i++) {
        ioread32(data->regs + 4 * R_PNT_D); 
        tmp_hw = ioread32(data->regs + 4 * R_PNT_D);

        if (tmp_hw & VALID_BIT) {                           /* count the valid entries */
            valid_index_cnt++;
        }
    }
    spin_unlock_irqrestore(&ioread_lock, flags);
    return snprintf(buf,PAGE_SIZE,"%d\n",valid_index_cnt);
}
/******************************************************************************/
ssize_t lreProxyNodeMacAddress_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    unsigned char addr[6];
    u16 i;
    u16 len = 0;
    u32 table_pointer;
    u32 table_size;
    u32 tmp_lw,tmp_hw;
    unsigned long flags;
      
    spin_lock_irqsave(&ioread_lock, flags); 
    table_pointer = ioread32(data->regs + 4 * R_PNT_S) & PNTP_BIT;       /* proxy node table pointer */
    table_size = ioread32(data->regs + 4 * R_PNT_S) & PNTS_BIT;
    table_size >>= 16;
    
    while (table_pointer != 0) {                            /* "set" table pointer to 0 */
        ioread32(data->regs + 4 * R_PNT_D);
        barrier();
        table_pointer = ioread32(data->regs + 4 * R_PNT_S) & PNTP_BIT;
    }
    
    for (i=0;i<table_size;i++) {
        tmp_lw = ioread32(data->regs + 4 * R_PNT_D); 
        tmp_hw = ioread32(data->regs + 4 * R_PNT_D);

        if (tmp_hw & VALID_BIT) {
            addr[0] = ((tmp_hw >>  8) & 0x0FF);
            addr[1] = ((tmp_hw >>  0) & 0x0FF);
            addr[2] = ((tmp_lw >> 24) & 0x0FF);
            addr[3] = ((tmp_lw >> 16) & 0x0FF);
            addr[4] = ((tmp_lw >>  8) & 0x0FF);
            addr[5] = ((tmp_lw >>  0) & 0x0FF);
            
            len += snprintf(buf+len,PAGE_SIZE,"%02x:%02x:%02x:%02x:%02x:%02x\n",
                     addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
        }
    }
    spin_unlock_irqrestore(&ioread_lock, flags);
    return len;
}
/******************************************************************************/
/**
 * @fn ssize_t R_XXX_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
 * @brief Functions to store a value to a register
 * 
 * These functions gets called due to a file write to a file stored in sys/lre. \n
 * Each file has its own write function, named after the filename + suffix _store.
 * @param   *kobj Pointer to the kobject struct
 * @param   *attr Pointer th the kobj_attribute struct 
 * @param   *buf char pointer to the buffer
 * @param   count size_t number of the written chars
 * @return  count number of the written chars
 * 
 */
ssize_t hsr_register_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
	struct hsr_reg_attribute *hsr = to_hsr_attr(attr);
    u32 val;
    if (kstrtou32(buf, 0, &val) != 0)
        return -EINVAL;
    iowrite32(val, data->regs + 4 * hsr->reg);
    return count;
}
/******************************************************************************/
ssize_t lre_counter_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
	struct hsr_reg_attribute *hsr = to_hsr_attr(attr);
	
    u32 val, cnt;
    if (kstrtou32(buf, 0, &val) != 0)
        return -EINVAL;
    /* Hardware does not support counter reset, so we implement it in software. */
    cnt = ioread32(data->regs + 4 * hsr->reg);
    hsr->counter_bias = cnt - val;
    return count;
}
/******************************************************************************/
ssize_t lrePortAdminStateA_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val;
    u32 tmp;
    u32 mask = (1<<PORT_A) | ((1<<PORT_A) << 4);           /* tx and rx enable */

    if (kstrtou32(buf, 0, &val) != 0)
        return -EINVAL;
    tmp = ioread32(data->regs + 4 * R_PEN);

    if (val == 1) {                                         /* notActive */
        tmp &= ~mask;
        iowrite32(tmp, data->regs + 4 * R_PEN); 
    } else if (val == 2) {                                  /* active */
        tmp |= mask;
        iowrite32(tmp, data->regs + 4 * R_PEN); 
    } else {
        return -EINVAL;
    }
    return count;
}
/******************************************************************************/
ssize_t lrePortAdminStateB_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val;
    u32 tmp;
    u32 mask = (1<<PORT_B) | ((1<<PORT_B) << 4);           /* tx and rx enable */

    if (kstrtou32(buf, 0, &val) != 0)
        return -EINVAL;
    tmp = ioread32(data->regs + 4 * R_PEN);

    if (val == 1) {                                         /* notActive */
        tmp &= ~mask;
        iowrite32(tmp, data->regs + 4 * R_PEN); 
    } else if (val == 2) {                                  /* active */
        tmp |= mask;
        iowrite32(tmp, data->regs + 4 * R_PEN); 
    } else {
        return -EINVAL;
    }
    return count;
}
/******************************************************************************/
// ssize_t lreLinkStatusA_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
// {
//     u32 val;
//     if (kstrtou32(buf, 0, &val) != 0)
//         return -EINVAL;
//     lre_iowrite32(val, lreLinkStatusA); 
//     return count;
// }
// ssize_t lreLinkStatusB_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
// {
//     u32 val;
//     if (kstrtou32(buf, 0, &val) != 0)
//         return -EINVAL;
//     lre_iowrite32(val, lreLinkStatusB); 
//     return count;
// }
/******************************************************************************/
ssize_t lreHsrLREMode_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val;
    u32 tmp;
    if (kstrtou32(buf, 0, &val) != 0)
        return -EINVAL;
    tmp = ioread32(data->regs + 4 * R_PEN);
    
    switch (val) {
    case 1:
        val = OPERATION_MODE_H;
        break;
    case 2:
        val = OPERATION_MODE_N;
        break;
    case 3:
        val = OPERATION_MODE_T;
        break;
    case 4:
        val = OPERATION_MODE_U;
        break;
    default:
        // mode M (mixed mode) is not supported, other modes neither
        return -EINVAL;
    }

    tmp &= ~(BIT18 | BIT19 | BIT20);
    tmp |= val << 18;
    iowrite32(tmp, data->regs + 4 * R_PEN); 
    return count;
}
/******************************************************************************/
ssize_t lreSwitchingEndNode_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val;
    u32 pen;
    u32 node_type;
    int redbox_mode = -1;

    if (kstrtou32(buf, 0, &val) != 0)
        return -EINVAL;
    pen = ioread32(data->regs + 4 * R_PEN);
    
    node_type   = (ioread32(data->regs + 4 * R_VER) & (BIT30 | BIT31)) >> 30;

    if (node_type == NODE_TYPE_PRP1) {
        /* PRP-1 Hardware. The only valid value is (3): a PRP node/RedBox. */
        /* Actually, the PRP-1 LRE switch ignores this value anyway. */
        if (val == 3) redbox_mode = REDBOX_MODE_SAN;
    } else if (node_type == NODE_TYPE_HSR) {
        if (val == 4) redbox_mode = REDBOX_MODE_SAN;
        if (val == 6) redbox_mode = REDBOX_MODE_HSR;
        if (val == 7) redbox_mode = REDBOX_MODE_PRP_A;
        if (val == 8) redbox_mode = REDBOX_MODE_PRP_B;
        /* other MIB values are not valid or not supported */
    }
    if (redbox_mode == -1) return -EINVAL;

    pen &= ~(BIT29 | BIT30 | BIT31);
    pen |= ((u32)redbox_mode) << 29;
    iowrite32(pen, data->regs + 4 * R_PEN); 
    return count;
}
/******************************************************************************/
ssize_t lreCPURedBoxIdentity_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val;
    u32 tmp;
    if (kstrtou32(buf, 0, &val) != 0)
        return -EINVAL;
    if (val < 0x0 || val > 0xf)
        return -EINVAL;
    tmp = ioread32(data->regs + 4 * R_PEN);
    tmp &= ~(BIT25 | BIT26 | BIT27 | BIT28);
    val = (val << 25) | tmp;

    iowrite32(val, data->regs + 4 * R_PEN);
    return count;
}
/******************************************************************************/
ssize_t lreRedBoxIdentity_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val;
    u32 tmp;
    if (kstrtou32(buf, 0, &val) != 0)
        return -EINVAL;
    if (val < 0x0 || val > 0xf)
        return -EINVAL;
    tmp = ioread32(data->regs + 4 * R_PEN);
    tmp &= ~(BIT21 | BIT22 | BIT23 | BIT24);
    val = (val << 21) | tmp;
    
    iowrite32(val, data->regs + 4 * R_PEN); 
    return count;
}
/******************************************************************************/
ssize_t lreDuplicateDetectionAging_ms_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 agt_milliseconds;
    u32 agt;
    u32 len = 256 * ((ioread32(data->regs + 4 * R_DD_AGT) >> 27) & 0xf);
    if (kstrtou32(buf, 0, &agt_milliseconds) != 0)
        return -EINVAL;
    // aging time = 20ns * agt * 2^LENGTH_LOG2
    // agt = agt_milliseconds * 1000000ns / 20ns / len
    agt = (agt_milliseconds * (1000000/len) + 20/2) / 20;
    if (agt >= 1<<18)
        return -EINVAL;
    iowrite32(agt, data->regs + 4 * R_DD_AGT); 
    return count;
}
/******************************************************************************/
ssize_t lreProxyNodeTableClear_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct zhaw_hsr *data = dev_get_drvdata(dev);
    u32 val;
    int ms_delay = 20;
    u32 tmp;
    
    if (kstrtou32(buf, 0, &val) != 0)
        return -EINVAL;
    tmp = ioread32(data->regs + 4 * R_PNT_AGT);
    
    if (val == 0)
        return count;
    if (val == 1)
        iowrite32(0x01, data->regs + 4 * R_PNT_AGT);                     /* clear table by setting the aging time to 1 */
    
    //init_waitqueue_head(&wait);                             /* wait until the table is cleared */
    //interruptible_sleep_on_timeout(&wait, ms_delay*HZ/1000);
    msleep_interruptible(ms_delay);
    current->state = TASK_INTERRUPTIBLE;

    iowrite32(tmp, data->regs + 4 * R_PNT_AGT);                          /* set the old aging time */
    return count;
}


/******************************************************************************/
/**
 * @brief Struct fot the lre registers (Register Set)
 * 
 * This struct contains the file name of the file, the rights and the names \n
 * of the read and write functions if any.
 */

// filled in __init()
struct hsr_reg_attribute static_register_attrs[R_COUNT];
/*
#define RW(_name) struct kobj_attribute _name##_ATTR = { \
	.attr = {.name = __stringify(_name), .mode = S_IRUGO | S_IWUSR }, \
	.show	= _name##_show, \
	.store	= _name##_store, \
}
#define RO(_name) struct kobj_attribute _name##_ATTR = { \
	.attr = {.name = __stringify(_name), .mode = S_IRUGO }, \
	.show	= _name##_show, \
}
*/
/*
#define COUNTER(_name, _reg) struct lre_kobj_attribute _name##_ATTR = {  \
    .kobj_attr = { \
	    .attr = {.name = __stringify(_name), .mode = S_IRUGO | S_IWUSR }, \
	    .show	= lre_counter_show, \
	    .store	= lre_counter_store, \
    }, \
    .reg = _reg, \
    .counter_bias = 0 \
}
*/
#define RW(_name) DEVICE_ATTR(_name, S_IRUGO | S_IWUSR, _name##_show, _name##_store)
#define RO(_name) DEVICE_ATTR(_name, S_IRUGO, _name##_show, NULL)

#define COUNTER(_name, _reg) struct hsr_reg_attribute _name##_ATTR = {  \
    .dev_attr = { \
	    .attr = {.name = __stringify(_name), .mode = S_IRUGO | S_IWUSR }, \
	    .show	= lre_counter_show, \
	    .store	= lre_counter_store, \
    }, \
    .reg = _reg, \
    .counter_bias = 0 \
}
/*******************************************************************************
 * Initialize MIB registers
 ******************************************************************************/
RO(lreNodeType);
RO(lreVersionName);
RO(lreMacAddress);
RW(lrePortAdminStateA);
RW(lrePortAdminStateB);
// RW(lreLinkStatusA);
// RW(lreLinkStatusB);
RO(lreDuplicateDiscard);
RO(lreTransparentReception);
RW(lreHsrLREMode);
RW(lreSwitchingEndNode);
RW(lreCPURedBoxIdentity);           /* FIXME: not a official MIB register (to be discussed in tisue data base) */
RW(lreRedBoxIdentity);
RW(lreProxyNodeTableClear);
RW(lreDuplicateDetectionAging_ms);
RO(lreDuplicateDetectionLength);

COUNTER(lreCntTxA, R_FRA_TAG_ATX);
COUNTER(lreCntTxB, R_FRA_TAG_BTX);
COUNTER(lreCntTxC, R_FRA_NLL_CTX);
COUNTER(lreCntRxA, R_FRA_TAG_ARX);
COUNTER(lreCntRxB, R_FRA_TAG_BRX);
COUNTER(lreCntRxC, R_FRA_ALL_CRX); /* FIXME: this should count (Interlink+CPU), not only Interlink */
COUNTER(lreCntErrWrongLanA, R_FRA_WRO_ARX);
COUNTER(lreCntErrWrongLanB, R_FRA_WRO_BRX);
COUNTER(lreCntErrWrongLanC, R_FRA_WRO_CRX);
COUNTER(lreCntErrorsA, R_FRA_ERR_ARX);
COUNTER(lreCntErrorsB, R_FRA_ERR_BRX);
COUNTER(lreCntErrorsC, R_FRA_ERR_CRX);
RO(lreCntProxyNodes);
// lreProxyNodeIndex
RO(lreProxyNodeMacAddress);


/******************************************************************************/
/**
 * @brief Struct for the register group's attributes 
 */
// filled in __init()
struct attribute *register_attrs[R_COUNT+1];
/******************************************************************************/
/**
 * @brief Struct for the mib group's attributes 
 */
struct attribute *mib_attrs[] = {
    &dev_attr_lreNodeType.attr,
    &dev_attr_lreVersionName.attr,

    
    &dev_attr_lreMacAddress.attr,
    &dev_attr_lrePortAdminStateA.attr,
    &dev_attr_lrePortAdminStateB.attr,
//     &lreLinkStatusA_ATTR.attr,
//     &lreLinkStatusB.attr,
    &dev_attr_lreDuplicateDiscard.attr,
    &dev_attr_lreTransparentReception.attr,
    &dev_attr_lreHsrLREMode.attr,
    &dev_attr_lreSwitchingEndNode.attr,
    &dev_attr_lreCPURedBoxIdentity.attr,
    &dev_attr_lreRedBoxIdentity.attr,
    &dev_attr_lreProxyNodeTableClear.attr,
    &dev_attr_lreDuplicateDetectionAging_ms.attr,
    &dev_attr_lreDuplicateDetectionLength.attr,

     &lreCntTxA_ATTR.dev_attr.attr,
    &lreCntTxB_ATTR.dev_attr.attr,
    &lreCntTxC_ATTR.dev_attr.attr,
    &lreCntErrWrongLanA_ATTR.dev_attr.attr,
    &lreCntErrWrongLanB_ATTR.dev_attr.attr,
    &lreCntErrWrongLanC_ATTR.dev_attr.attr,
    &lreCntRxA_ATTR.dev_attr.attr,
    &lreCntRxB_ATTR.dev_attr.attr,
    &lreCntRxC_ATTR.dev_attr.attr,
    &lreCntErrorsA_ATTR.dev_attr.attr,
    &lreCntErrorsB_ATTR.dev_attr.attr,
    &lreCntErrorsC_ATTR.dev_attr.attr,

    &dev_attr_lreCntProxyNodes.attr,
    &dev_attr_lreProxyNodeMacAddress.attr,
    
    NULL, /* last element must be NULL to terminate the list */
};

/******************************************************************************/
/**
 * @brief group of attributes for the lre registers
 */
struct attribute_group register_group = {
    .name = "registers",
    .attrs = register_attrs,
};

/******************************************************************************/
/**
 * @brief Group of attributes for the MIB registers
 */
struct attribute_group mib_group = {
    .name = "mib",
    .attrs = mib_attrs,
};

static void lre_registers_init(void)
{
    int i, k;

    /* fill generate register attributes */
    k = 0;
    for (i=0; i<R_COUNT; i++) {
        if (R_NAMES[i]) {
            static_register_attrs[i].dev_attr.attr.name = R_NAMES[i];
            static_register_attrs[i].dev_attr.attr.mode = S_IRUGO | S_IWUSR;
            static_register_attrs[i].dev_attr.show = hsr_register_show;
            static_register_attrs[i].dev_attr.store = hsr_register_store;
            static_register_attrs[i].reg = i;
            register_attrs[k++] = &static_register_attrs[i].dev_attr.attr;
        }
    }
    register_attrs[k] = NULL;
}

typedef uint8_t MAC_ADDR[6];
#define NODES_TABLE_SIZE 511
static nodes_table_node_t nodes_table[NODES_TABLE_SIZE][10];
#define SUPERVISION_MCAST {0x01, 0x15, 0x4E, 0x00, 0x01};

static int zhaw_hsr_add_node_to_table(uint8_t * mac){
	unsigned int index = (mac[0]*mac[1]*mac[2]*mac[3]*mac[4]*mac[5]) % NODES_TABLE_SIZE;
	unsigned int i;
	index = abs(index);
	for(i = 0; i < 10; i++){
		if(nodes_table[index][i].valid){
			if(memcmp(mac, nodes_table[index][i].addr, 6) == 0){ //node already in table
				nodes_table[index][i].age_a_s = 0;
				nodes_table[index][i].age_b_s = 0;
				printk(KERN_INFO "Node table entry updated\n");
				return 0; 
			}
			else
				continue;
		}
		else{
			nodes_table[index][i].valid = true;
			nodes_table[index][i].opmode = DUPLICATE_DISCARD_OPMODE;
			nodes_table[index][i].age_a_s = 0;
			nodes_table[index][i].age_b_s = 0;
			nodes_table[index][i].node_type = node_type_danh;
			
			memcpy(nodes_table[index][i].addr, mac, 6);
			printk(KERN_INFO "Node added to table : %x %x %x %x %x %x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			break;
		}
	}
	if(i == 10) return -1; // Node table full for this entry
	return 0;
}
static bool zhaw_hsr_is_supervision(void * data, uint16_t len){
	union hsr_sup_frame * frame = data;
	uint8_t SV_MCAST[] = SUPERVISION_MCAST;
	if(len != 60) return false;
	
	if(memcmp(SV_MCAST, frame->dest_mac, sizeof(SV_MCAST)))
		return false;
	if(frame->ethertype != 0x892f) return false;
	
	
	printk(KERN_INFO "Adding node to table\n");
	zhaw_hsr_add_node_to_table(frame->mac_addr);
	return true;
}

static bool zhaw_hsr_is_node_in_table(uint8_t * mac){
	unsigned int i;
	unsigned int index = (mac[0]*mac[1]*mac[2]*mac[3]*mac[4]*mac[5]) % NODES_TABLE_SIZE;
	index = abs(index);
	for(i = 0; i < 10; i++)
		if(nodes_table[index][i].valid && memcmp(mac, &nodes_table[index][i].addr, 6) == 0)
			return true;

	return false;
}
static irqreturn_t zhaw_hsr_irq(int a, void* dev){
	struct zhaw_hsr *hsr_data = dev_get_drvdata(dev);
	struct net_device * ndev = hsr_data->ndev;
	uint32_t __iomem * p_rci_rconfig = hsr_data->regs_cpu + 4;
	uint32_t __iomem * p_rci_int = hsr_data->regs_cpu + 8;
	uint32_t __iomem * rci_txrx_data = hsr_data->regs_cpu + 0xC;
	uint32_t rci_int = *p_rci_int;
	struct sk_buff *skb;
	unsigned long irqflags;
	//spin_lock_irqsave(&ioread_lock, irqflags); 

	if(rci_int & 0x1){ // RX interrupt
		union REG_RCI_RCONFIG rci_rconfig;
		rci_rconfig.LONG = *p_rci_rconfig;
		if(rci_rconfig.BIT.FP){
			int i;
			//rci_rconfig.BIT.END = 0;
			// TODO : Check receiving port and store in the correct table
			int tag = rci_rconfig.BIT.TAG; //recevied with tag ?
			
	
			uint16_t pkt_len = rci_rconfig.BIT.RFS;
			uint32_t * data = kmalloc(pkt_len + pkt_len % 4, GFP_ATOMIC);
			//printk(KERN_INFO "HSR RX interrupt : %d\n", pkt_len);
			for(i = 0; i < (pkt_len + pkt_len % 4) / 4; i++){
				data[i] = be32_to_cpu(rci_txrx_data[i]);
				//printk(KERN_INFO "WORD : %x\n", data[i]);
			}
			if(zhaw_hsr_is_supervision(data, pkt_len)){
				printk(KERN_INFO "Supervision frame received\n");
				kfree(data);
				//spin_unlock_irqrestore(&ioread_lock, irqflags); 
				return IRQ_HANDLED;
			}
			if(rci_rconfig.BIT.TAG)
				zhaw_hsr_add_node_to_table(((uint8_t *) data) + 6);
			skb = netdev_alloc_skb_ip_align(ndev, pkt_len);
			skb_copy_to_linear_data(skb, data, pkt_len);
			skb_put(skb, pkt_len);
			skb->protocol = eth_type_trans(skb, ndev);
			netif_rx(skb);
			
			rci_rconfig.BIT.RFD = 1;
			*p_rci_rconfig = rci_rconfig.LONG;
			rci_rconfig.LONG = *p_rci_rconfig;
			kfree(data);
		}
		
	}
	if(rci_int & 0x2){ // TX interrupt, nothing to do
	}
		//printk(KERN_INFO "IRQ Handled\n");
	
	//spin_unlock_irqrestore(&ioread_lock, irqflags); 
	return IRQ_HANDLED;
}
static int zhaw_hsr_switch_port_open(struct net_device *dev)
{
	printk(KERN_INFO "zhaw_hsr_switch_port_open\n");
	//Set MAC address
	dev->dev_addr = "\x02\x00\x00\x00\x00\x02";

	return 0;
}

static int zhaw_hsr_switch_port_stop(struct net_device *dev)
{
	printk(KERN_INFO "zhaw_hsr_switch_port_stop\n");
	return 0;
}

static int zhaw_hsr_switch_port_ioctl(struct net_device *dev, struct ifreq *ifrq, int cmd)
{
	printk(KERN_INFO "zhaw_hsr_switch_port_ioctl %s %d\n", ifrq-> ifr_name, cmd);
	return 0;
}

static netdev_tx_t zhaw_hsr_switch_port_start_xmit(struct sk_buff *skb,
						struct net_device *ndev)
{
	struct zhaw_hsr *data = dev_get_drvdata(&ndev->dev);
	
	uint32_t __iomem * p_rci_wconfig = data->regs_cpu;
	uint32_t __iomem * rci_txrx_data = data->regs_cpu + 0xC;
	union REG_RCI_WCONFIG rci_wconfig;
	int i;
	
	//spin_lock(&ioread_lock);
	//printk(KERN_INFO "XMIT LEN: %d\n", skb->len);
	for(i = 0; i < skb->len;i += 4){
		uint32_t word = *(uint32_t*) (skb->data + i);
		//printk(KERN_INFO "XMIT: %x\n", cpu_to_be32(word));
		rci_txrx_data[i / 4] = cpu_to_be32(word);
	}
	rci_wconfig.BIT.SB = 0; //start sending
	rci_wconfig.BIT.END = 0; //Big endian
	rci_wconfig.BIT.PTH = 0; //path field of hsr tag
	if(zhaw_hsr_is_node_in_table(skb->data)){
		rci_wconfig.BIT.TAG = 1; //Send with tag
	}
	else
		rci_wconfig.BIT.TAG = 0; //Send with tag
	rci_wconfig.BIT.A = 1; //send to port A
	rci_wconfig.BIT.B = 1; //Send to port B
	rci_wconfig.BIT.WFS = skb->len > 60 ? skb->len : 60; //frame length
	rci_wconfig.BIT.I = 0; //do not send to interlink

	iowrite32(rci_wconfig.LONG, p_rci_wconfig);
	
	rci_wconfig.BIT.SB = 1; //start sending
	iowrite32(rci_wconfig.LONG, p_rci_wconfig);
	
	//spin_unlock(&ioread_lock);
	return NETDEV_TX_OK;
}

static int zhaw_hsr_switch_port_change_mtu(struct net_device *dev, int new_mtu)
{
	printk(KERN_INFO "zhaw_hsr_switch_port_change_mtu\n");
	return 0;
}

static const struct net_device_ops zhaw_hsr_switch_netdev_ops = {
	/* When using an NFS rootfs, we can only bring up the GMAC behind the
	 * upstream port. So we need ndo_init to enable the Switch's downstream
	 * ports and kick the PHY.
	 */
	.ndo_init		= zhaw_hsr_switch_port_open,
	.ndo_open		= zhaw_hsr_switch_port_open,
	.ndo_stop		= zhaw_hsr_switch_port_stop,
	.ndo_do_ioctl		= NULL,//zhaw_hsr_switch_port_ioctl,
	.ndo_start_xmit		= zhaw_hsr_switch_port_start_xmit,
	.ndo_change_mtu		= zhaw_hsr_switch_port_change_mtu,
};

static int register_eth_device(struct device * dev, struct zhaw_hsr * data){
	data->ndev = alloc_etherdev(sizeof(struct zhaw_hsr));
	data->ndev->netdev_ops = &zhaw_hsr_switch_netdev_ops;
	printk(KERN_INFO "register_eth_device");

	if(register_netdev(data->ndev) != 0){
		dev_err(dev, "Could not register ethernet device\n");
		return -1;
	}
	
	dev_set_drvdata(&data->ndev->dev, data);
	printk(KERN_INFO "register_eth_device : success\n");
	return 0;
}
static int zhaw_hsr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zhaw_hsr *data;
	struct resource *res;
	int err;
	uint32_t val;

	data = devm_kzalloc(dev, sizeof(struct zhaw_hsr), GFP_KERNEL);
	data->dev = dev;
	/* Get memory regions */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->regs_cpu = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->regs_cpu))
		return PTR_ERR(data->regs_cpu);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	data->regs = devm_ioremap_resource(dev, res);
	dev_info(data->dev, "HSR main regs : %p\n", data->regs);
	if (IS_ERR(data->regs))
		return PTR_ERR(data->regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	data->regs_ptp = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->regs_ptp))
		return PTR_ERR(data->regs_ptp);

	/* Get clocks */
	data->clk_100mhz = devm_clk_get(dev, "100MHz");
	data->clk_50mhz = devm_clk_get(dev, "50MHz");
	data->clk_pclk = devm_clk_get(dev, "apb_pclk");

	if (IS_ERR(data->clk_100mhz) ||
	    IS_ERR(data->clk_50mhz) ||
	    IS_ERR(data->clk_pclk)) {
		goto err_clk;
	}

	/* Enable clocks */
	err = clk_prepare_enable(data->clk_100mhz);
	if (err) {
		dev_err(dev, "could not enable 100MHz clk\n");
		goto err_clk;
	}

	err = clk_prepare_enable(data->clk_50mhz);
	if (err) {
		dev_err(dev, "could not enable 50MHz clk\n");
		goto err_clk;
	}

	err = clk_prepare_enable(data->clk_pclk);
	if (err) {
		dev_err(dev, "could not enable apb_pclk\n");
		goto err_clk;
	}

	/* Reset C domain */
	val = rzn1_sysctrl_readl(RZN1_SYSCTRL_REG_PWRCTRL_HSR);
	val &= ~(1 << RZN1_SYSCTRL_REG_PWRCTRL_HSR_RSTN_C);
	rzn1_sysctrl_writel(val, RZN1_SYSCTRL_REG_PWRCTRL_HSR);
	udelay(1);
	val |= (1 << RZN1_SYSCTRL_REG_PWRCTRL_HSR_RSTN_C);
	rzn1_sysctrl_writel(val, RZN1_SYSCTRL_REG_PWRCTRL_HSR);

	/* Reset A domain */
	val = rzn1_sysctrl_readl(RZN1_SYSCTRL_REG_PWRCTRL_HSR);
	val &= ~(1 << RZN1_SYSCTRL_REG_PWRCTRL_HSR_RSTN_A);
	rzn1_sysctrl_writel(val, RZN1_SYSCTRL_REG_PWRCTRL_HSR);
	udelay(1);
	val |= (1 << RZN1_SYSCTRL_REG_PWRCTRL_HSR_RSTN_A);
	rzn1_sysctrl_writel(val, RZN1_SYSCTRL_REG_PWRCTRL_HSR);

	mdelay(10);
	
	/* create a main kobject structure sys/lre */
	/*
	dev->kobj = kobject_create_and_add("zhaw-hsr", NULL);
	if (!data->lre_kobj) {
		printk(KERN_INFO "lre create failed\n");
		return -ENOMEM;
	}
	*/
	lre_registers_init();
	/* create the lre group with its attributes */
	err = sysfs_create_group(&dev->kobj, &register_group);
	if (err) {
		kobject_put(&dev->kobj);
		return err;
	}
	
	/* create the mib group with its attribues */
	err = sysfs_create_group(&dev->kobj, &mib_group);
	if (err) {
		kobject_put(&dev->kobj);
		return err;
	}
	
	/* Register interrupt */
	err = platform_get_irq_byname(pdev, "cpu_irq");
	if(err < 0){
		printk(KERN_INFO "Reading hsr irq failed: %d\n", err);
		return err;
	}
	//Register Interurpt handler
	err = request_irq(err, zhaw_hsr_irq, IRQF_SHARED, "zhaw-hsr", dev);
	if(err < 0){
		printk(KERN_INFO "HSR IRQ  request failed : %d\n", err);
		return err;
	}
	
	/* Configure ethernet device */
	register_eth_device(dev, data);
	dev_set_drvdata(dev, data);
	return 0;

err_clk:
	if (!IS_ERR(data->clk_pclk))
		clk_disable_unprepare(data->clk_pclk);
	if (!IS_ERR(data->clk_50mhz))
		clk_disable_unprepare(data->clk_50mhz);
	if (!IS_ERR(data->clk_100mhz))
		clk_disable_unprepare(data->clk_100mhz);

	return 0;
}

static int zhaw_hsr_remove(struct platform_device *pdev)
{
	struct zhaw_hsr *data;
	uint32_t val;
	
	data = dev_get_drvdata(&pdev->dev);
	
	//Remove netdevice
	unregister_netdev(data->ndev);
	//Remove nodes in sysfs
	sysfs_remove_group(&pdev->dev.kobj, &mib_group);
	sysfs_remove_group(&pdev->dev.kobj, &register_group);
	
	//kobject_del(&pdev->dev.kobj);

	/* Reset A domain */
	val = rzn1_sysctrl_readl(RZN1_SYSCTRL_REG_PWRCTRL_HSR);
	val &= ~(1 << RZN1_SYSCTRL_REG_PWRCTRL_HSR_RSTN_A);
	rzn1_sysctrl_writel(val, RZN1_SYSCTRL_REG_PWRCTRL_HSR);

	/* Reset C domain */
	val = rzn1_sysctrl_readl(RZN1_SYSCTRL_REG_PWRCTRL_HSR);
	val &= ~(1 << RZN1_SYSCTRL_REG_PWRCTRL_HSR_RSTN_C);
	rzn1_sysctrl_writel(val, RZN1_SYSCTRL_REG_PWRCTRL_HSR);

	
	/* Disable clocks */
	clk_disable_unprepare(data->clk_pclk);
	clk_disable_unprepare(data->clk_50mhz);
	clk_disable_unprepare(data->clk_100mhz);
	
	
	mdelay(10);
	return 0;
}


static const struct of_device_id zhaw_hsr_match[] = {
	{ .compatible = "zhaw,hsr" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, zhaw_hsr_match);

static struct platform_driver zhaw_hsr_driver = {
	.probe  = zhaw_hsr_probe,
	.remove = zhaw_hsr_remove,
	.driver = {
		.name	   = "zhaw-hsr",
		.of_match_table = zhaw_hsr_match,
	},
	
};
module_platform_driver(zhaw_hsr_driver);

MODULE_AUTHOR("Phil Edworthy <phil.edworthy@renesas.com>");
MODULE_DESCRIPTION("ZHAW HSR");
MODULE_LICENSE("GPL");
