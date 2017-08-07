/*
 * Copyright(C),2014-2015, All rights reserved.
 * Developed using:  gcc version 4.7.3 20130226 
 * Author: Chang.Qing<chang.qing@advantech.com.cn>
 * Description:
 *  Write value to any physical address of any device.
 * Version: 1.00
 * Last Change: 2016/07/30
 * -------------------------------------------------------------------------------
 * Version history:
 * 1.00  16/07/30  Qing   the first version.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <asm/io.h>

#define PHY_SIZE_LEN	(4)

static struct class *phy2virt_class;
static struct cdev phy2virt_cdev;
static struct device *pdev;
static dev_t dev;
static struct {
u32  value;
u32  phy_addr;
u32 __iomem * virt_addr; 
} Phy2virt_Obj;

static DEFINE_MUTEX(sysfs_lock);

/* string to hex */
static inline unsigned char str2hexnum(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0; /* foo */
}
static inline unsigned long str2hex(const char *str)
{
	unsigned long value = 0;
	unsigned char idx = 0;
	const char * ptmp;

	ptmp = strstr(str, "0x");
	if(!ptmp){
		ptmp =	strstr(str, "0X");
	}
	
	ptmp = !ptmp ? str : ptmp+2;
 
	while (*ptmp && ( idx < 8)) {
		value = value << 4;
		value |= str2hexnum(*ptmp++);
		idx++;
	}

	return value;
}

/******************* The implement for access these  attributes ******************/
static ssize_t phy2virt_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t			status;

	mutex_lock(&sysfs_lock);
	status = sprintf(buf, "0x%x\n", Phy2virt_Obj.phy_addr);
	mutex_unlock(&sysfs_lock);
	return status;
}

static ssize_t phy2virt_addr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t			status;

	mutex_lock(&sysfs_lock);

	Phy2virt_Obj.phy_addr = str2hex(buf);
	Phy2virt_Obj.virt_addr = ioremap(Phy2virt_Obj.phy_addr, PHY_SIZE_LEN);

	status = size;
	mutex_unlock(&sysfs_lock);
	
	return status;
}
static DEVICE_ATTR(addr, 0644,
		phy2virt_addr_show, phy2virt_addr_store);


static ssize_t phy2virt_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t			status;

	mutex_lock(&sysfs_lock);
	if(!Phy2virt_Obj.virt_addr){
		status = sprintf(buf, "invalid phy address!\n");
		goto out;
	}			
	
	Phy2virt_Obj.value =*(volatile u32 *)(Phy2virt_Obj.virt_addr); //readl(Phy2virt_Obj.virt_addr);
	
	status = sprintf(buf, "0x%x\n", Phy2virt_Obj.value);	

out:
	mutex_unlock(&sysfs_lock);
	return status;
}

static ssize_t phy2virt_value_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t			status = 1;

	mutex_lock(&sysfs_lock);
	Phy2virt_Obj.value = str2hex(buf);
	if(!Phy2virt_Obj.virt_addr)
		goto out;

	//writel(Phy2virt_Obj.value, Phy2virt_Obj.virt_addr);
	*(volatile u32 *)(Phy2virt_Obj.virt_addr) = Phy2virt_Obj.value;
	status = 1;
out:
	mutex_unlock(&sysfs_lock);
	return status;
}
static DEVICE_ATTR(value, 0644,
		phy2virt_value_show, phy2virt_value_store);

static struct attribute *phy2virt_attrs[] = {
	&dev_attr_addr.attr,
	&dev_attr_value.attr,
	NULL,
};


static const struct attribute_group phy2virt_group = {
	.attrs = phy2virt_attrs,
	//.is_visible = phy2virt_is_visible,   		/* No  need .*/
};

/* Phy2virt group */
static const struct attribute_group *phy2virt_groups[] = {
	&phy2virt_group,
	NULL
};

static const struct file_operations phy2virt_fops = {
	.owner		= THIS_MODULE,
};

static int __init phy2virt_init(void)
{
	int res;
	
	printk(KERN_INFO "phy2virt driver\n");

	/* dymaic alloc & register a devid */
	res = alloc_chrdev_region(&dev, 0, 1, "phy2virt");
	if (res){
		printk(KERN_ERR "phy2virt: unable to get major %d\n", MAJOR(dev));
		goto out;
	}

	/* Init the cdev. */
	cdev_init(&phy2virt_cdev, &phy2virt_fops);

	/* Add a char device driver */
	if ((res = cdev_add(&phy2virt_cdev, dev, 1)) != 0) {
		printk(KERN_ERR "phy2virt: unable register character device\n");
		goto out_unregister_region;
	}

	/* Create a class */
	phy2virt_class = class_create(THIS_MODULE, "phy2virt");
	if (IS_ERR(phy2virt_class)) {
		res = PTR_ERR(phy2virt_class);
		goto out_unreg_chrdev;
	}

	/* Create a device to sysfs */	
	pdev = device_create_with_groups(phy2virt_class, NULL,
					dev, NULL , phy2virt_groups,
					"phy2virt");
	if (IS_ERR(pdev)) {
		res = PTR_ERR(pdev);
		goto out_unregister_calss;
	}

	Phy2virt_Obj.value = 0;
	Phy2virt_Obj.virt_addr = NULL;
	Phy2virt_Obj.phy_addr = 0;
	return 0;

out_unregister_calss:
	class_destroy(phy2virt_class);
out_unreg_chrdev:
	cdev_del(&phy2virt_cdev);
out_unregister_region:
	unregister_chrdev_region(dev, 1);
out:
	return res;
}
module_init(phy2virt_init);

static void __exit phy2virt_exit(void)
{
	if (pdev) {
		device_unregister(pdev);
		put_device(pdev);
		pdev = NULL;
	}
	class_destroy(phy2virt_class);
	cdev_del(&phy2virt_cdev);
	unregister_chrdev_region(dev, 1);	
}
module_exit(phy2virt_exit);

MODULE_AUTHOR("Qing(chang.qing@advantech.com.cn) ");
MODULE_DESCRIPTION("phy2virt-address test");
MODULE_VERSION("1.0.0");
MODULE_LICENSE("GPL");
