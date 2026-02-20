/*
 * PCI demo driver (MEM based) for CH368 board
 * 
 * # insmod ch368_mem.ko
 * # ./mem_test /dev/ch368_mem0
 *
 * # echo -n "ab" > /dev/ch368_mem0
 *
 */

#include <linux/kernel.h>	/* printk() */
#include <linux/module.h>	/* modules */
#include <linux/init.h>		/* module_{init,exit}() */
#include <linux/slab.h>		/* kmalloc()/kfree() */
#include <linux/pci.h>		/* pci_*() */
#include <linux/pci_ids.h>	/* pci idents */
#include <linux/list.h>		/* list_*() */
#include <asm/uaccess.h>	/* copy_{from,to}_user() */
#include <linux/fs.h>		/* file_operations */
#include <linux/interrupt.h>	/* request_irq etc */
#include <linux/version.h>
#include <linux/device.h>	/* class */

MODULE_DESCRIPTION("ch368_mem");
MODULE_AUTHOR("Pierre Ficheux (pierre.ficheux@gmail.com)");
MODULE_LICENSE("GPL");

/*
 * Arguments
 */
static int major = 0; /* Major number */
module_param(major, int, 0660);
MODULE_PARM_DESC(major, "Static major number (none = dynamic)");

// Driver class in /sys
static struct class *ch368_mem_class;

/*
 * Supported devices
 */

#define VENDOR_ID    0x1c00
#define DEVICE_ID    0x5834

static struct pci_device_id ch368_mem_id_table[] = {
  {VENDOR_ID, DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
  {0,}	/* 0 terminated list */
};
MODULE_DEVICE_TABLE(pci, ch368_mem_id_table);

/*
 * Global variables
 */
static LIST_HEAD(ch368_mem_list);

struct ch368_mem_struct {
  struct list_head	link; /* Double linked list */
  struct pci_dev	*dev; /* PCI device */
  int			minor; /* Minor number */
  unsigned int		*mmio;
  u32			mmio_len;
};

/*
 * Event handler
 */
static irqreturn_t ch368_mem_irq_handler(int irq, void *dev_id)
{
  struct ch368_mem_struct *data = (struct ch368_mem_struct *)dev_id;

  printk(KERN_INFO "ch368_mem: interrupt from device %d\n", data->minor);

  return IRQ_HANDLED;
}

/*
 * File operations
 */
static ssize_t ch368_mem_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
  int real, i;
  struct ch368_mem_struct *data = file->private_data;
  unsigned char *p = (unsigned char *)buf;

  /* Check for overflow */
  real = min(data->mmio_len - (u64)*ppos, (u64) count);
 
  // Read data 
  for (i = 0 ; i < real ; i++) {
    *(p+i) = ioread8(data->mmio+ i + *ppos); 
    pr_info ("%s: read %x @ %p (ppos= %lld)\n", __FUNCTION__, *(p+i), data->mmio+ i + *ppos, *ppos);
  }

  *ppos += real;

  return real;
}

static ssize_t ch368_mem_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
  int i, real;
  struct ch368_mem_struct *data = file->private_data;
  unsigned char *p = (unsigned char *)buf, val;
  
  /* Check for overflow */
  real = min(data->mmio_len - (u64)*ppos, (u64) count);

  // Write data
  for (i = 0 ; i < real ; i++) {
    pr_info ("%s: write %x @ ppos= %d\n", __FUNCTION__, *(p+i), (int)*ppos);
    iowrite8(*(p+i), data->mmio + *ppos + i);

    val = ioread8(data->mmio + i + *ppos); 
    pr_info ("%s: written %x @ ppos %d\n", __FUNCTION__, val, (int)*ppos);
  }
  
  *ppos += real;

  return real;
}

static int ch368_mem_open(struct inode *inode, struct file *file)
{
  int minor = MINOR(inode->i_rdev);
  struct list_head *cur;
  struct ch368_mem_struct *data;

  printk("ch368_mem_open()\n");

  list_for_each(cur, &ch368_mem_list) {
    data = list_entry(cur, struct ch368_mem_struct, link);

    if (data->minor == minor) {
      file->private_data = data;

      return 0;
    }
  }

  pr_warn("ch368_mem: minor %d not found\n", minor);

  return -ENODEV;
}

static int ch368_mem_release(struct inode *inode, struct file *file)
{
  printk("ch368_mem_release()\n");

  file->private_data = NULL;

  return 0;
}

static struct file_operations ch368_mem_fops = {
  .owner =	THIS_MODULE,
  .read =	ch368_mem_read,
  .write =	ch368_mem_write,
  .open =	ch368_mem_open,
  .release =	ch368_mem_release,
};

/*
 * PCI handling
 */
static int ch368_mem_probe(struct pci_dev *dev, const struct pci_device_id *ent)
{
  int ret = 0, i;
  struct ch368_mem_struct *data;
  static int minor = 0;
  struct device *device = NULL;

  pr_info("ch368_mem: found %04x:%04x\n", ent->vendor, ent->device);
  pr_info("ch368_mem: using major %d and minor %d for this device\n", major, minor);

  /* Allocate a private structure and reference it as driver's data */
  data = (struct ch368_mem_struct *)kmalloc(sizeof(struct ch368_mem_struct), GFP_KERNEL);
  if (data == NULL) {
    pr_warn("ch368_mem: unable to allocate private structure\n");

    ret = -ENOMEM;
    goto cleanup_kmalloc;
  }

  pci_set_drvdata(dev, data);

  /* Init private field */
  data->dev = dev;
  data->minor = minor++;

  /* Initialize device before it's used by the driver */
  ret = pci_enable_device(dev);
  if (ret < 0) {
    pr_warn("ch368_mem: unable to initialize PCI device\n");

    goto cleanup_pci_enable;
  }

  /* Reserve PCI I/O and memory resources */
  ret = pci_request_regions(dev, "ch368_mem");
  if (ret < 0) {
    pr_warn("ch368_mem: unable to reserve PCI resources\n");

    goto cleanup_regions;
  }

  for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
    if (pci_resource_len(dev, i) == 0)
      continue;

    if (pci_resource_start(dev, i) == 0)
      continue;

    pr_info("ch368_mem: BAR %d (%#08x-%#08x), len=%d, flags=%#08x\n", i, (u32) pci_resource_start(dev, i), (u32) pci_resource_end(dev, i), (u32) pci_resource_len(dev, i), (u32) pci_resource_flags(dev, i));

    if (pci_resource_flags(dev, i) & IORESOURCE_MEM) {
      data->mmio = pci_iomap(dev, i, pci_resource_len(dev, i));
      if (data->mmio == NULL) {
	pr_warn("ch368_mem: unable to remap BAR %d\n", i);
	goto cleanup_pci_iomap;
      }

      data->mmio_len = pci_resource_len(dev, i);

      pr_info("ch368_mem: BAR %d has been remaped at 0x%p\n", i, data->mmio);

      break; // we use the first MEM
    }
  }

  if (i == DEVICE_COUNT_RESOURCE) {
    pr_warn("ch368_io: can't fin IO memory !\n");
    ret = -ENXIO;
    goto cleanup_regions;
  }

  /* Install the irq handler */
  if (dev->pin) {
    if (pci_enable_msi (dev))
      pr_warn("ch368_mem: unable to init MSI !\n");
    else 
      ret = request_irq(dev->irq, ch368_mem_irq_handler, 0, "ch368_mem", data);
    if (ret < 0) {
      pr_warn("ch368_mem: unable to register irq handler\n");
      goto cleanup_irq;
    }
    else
      pr_warn("ch368_mem: IRQ %d registered !\n", dev->irq);
  }
  else
    pr_info("ch368_mem: no IRQ!\n");

  /* Link the new data structure with others */
  list_add_tail(&data->link, &ch368_mem_list);

  // add device to class
  device = device_create(ch368_mem_class, NULL, MKDEV(major, data->minor), NULL, "ch368_mem" "%d", data->minor);

  if (IS_ERR(device)) {
    ret = PTR_ERR(device);
    pr_warn("ch368_mem: can't create device %d\n", data->minor);
    goto cleanup_irq;
  }

  return 0;

cleanup_irq:
  pci_iounmap(dev, data->mmio);
cleanup_pci_iomap:
  pci_release_regions(dev);
cleanup_regions:
  pci_disable_device(dev);
cleanup_pci_enable:
  kfree(data);
cleanup_kmalloc:
  return ret;
}

static void ch368_mem_remove(struct pci_dev *dev)
{
  struct ch368_mem_struct *data = pci_get_drvdata(dev);

  // Unmap BAR0
  pci_iounmap(dev, data->mmio);

  pci_release_regions(dev);

  if (dev->pin) {
    free_irq(dev->irq, data);
    pci_disable_msi(dev);
  }

  pci_disable_device(dev);

  list_del(&data->link);

  device_destroy(ch368_mem_class, MKDEV(major, data->minor));

  kfree(data);

  pr_info("ch368_mem: device removed\n");
}

static struct pci_driver ch368_mem_driver = {
  .name =	"ch368_mem",
  .id_table =	ch368_mem_id_table,
  .probe =	ch368_mem_probe,		/* Init one device */
  .remove =	ch368_mem_remove,		/* Remove one device */
};

/*
 * Init and Exit
 */
static int __init ch368_mem_init(void)
{
  int ret;

  /* Register the device driver major */
  ret = register_chrdev(major, "ch368_mem", &ch368_mem_fops);
  if (ret < 0) {
    pr_warn("ch368_mem: unable to get a major\n");

    return ret;
  }

  if (major == 0)
    major = ret; /* dynamic value */

  // Class creation in /sys/class
  #if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 4, 0)
  ch368_mem_class = class_create (THIS_MODULE, "ch368_mem");
#else
  ch368_mem_class = class_create ("ch368_mem");
#endif  
  if (IS_ERR(ch368_mem_class)) {
    ret = PTR_ERR(ch368_mem_class);
    pr_warn("ch368_mem: can't create class !\n");

    return ret;
  }

  /* Register PCI driver */
  ret = pci_register_driver(&ch368_mem_driver);
  if (ret < 0) {
    pr_warn("ch368_mem: unable to register PCI driver\n");

    unregister_chrdev(major, "ch368_mem");

    return ret;
  }

  return 0;
}

static void __exit ch368_mem_exit(void)
{
  pci_unregister_driver(&ch368_mem_driver);
  class_destroy(ch368_mem_class);
  unregister_chrdev(major, "ch368_mem");
}

/*
 * Module entry points
 */
module_init(ch368_mem_init);
module_exit(ch368_mem_exit);
