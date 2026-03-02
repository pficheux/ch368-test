/*
 * PCI demo driver (IO based) for CH368 board
 * 
 * # insmod ch368_io.ko
 * # ./led_blink /dev/ch368_io0
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

MODULE_DESCRIPTION("ch368_io");
MODULE_AUTHOR("Pierre Ficheux (pierre.ficheux@gmail.com)");
MODULE_LICENSE("GPL");

#define BUF_SIZE 256

/*
 * Arguments
 */
static int major = 0; /* Major number */
module_param(major, int, 0660);
MODULE_PARM_DESC(major, "Static major number (none = dynamic)");

static int debug = 0;
module_param(debug, int, 0660);
MODULE_PARM_DESC(debug, "Debug mode 0/1");

/*
 * Supported devices
*/

#define VENDOR_ID	0x1c00
#define DEVICE_ID	0x5834

static struct pci_device_id ch368_io_id_table[] = {
  {VENDOR_ID, DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
  {0,}	/* 0 terminated list */
};
MODULE_DEVICE_TABLE(pci, ch368_io_id_table);

/*
 * Global variables
 */
static LIST_HEAD(ch368_io_list);

struct ch368_io_struct {
  struct list_head	link; /* Double linked list */
  struct pci_dev		*dev; /* PCI device */
  int			minor; /* Minor number */
  unsigned int		iobase;
  u32			iolen;
};

// Driver class in /sys
static struct class *ch368_io_class;

/*
 * Event handlers
 */
static irqreturn_t ch368_io_irq_handler(int irq, void *dev_id)
{
  struct ch368_io_struct *data = (struct ch368_io_struct *)dev_id;

  pr_info("ch368_io: interrupt from device %d\n", data->minor);

  return IRQ_HANDLED;
}

/*
 * File operations
 */
static ssize_t ch368_io_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
  int i, real;
  struct ch368_io_struct *data = file->private_data;
  unsigned char kbuf[BUF_SIZE];
  unsigned int port;

  /* Check for overflow */
  real = min((int)data->iolen - (int)*ppos, (int)count);

  if (debug)
    pr_info ("read offset= %x\n", (int)*ppos);
  port = data->iobase + *ppos;

  /* Copy data from board */
  if (real) {
    for (i = 0; i < real; i += sizeof(char)) {
      pr_info ("port[%d] = %x\n", i, inb(port+i));
      *(kbuf + i) = inb(port + i);
    }

    if (copy_to_user(buf, kbuf, real))
      return -EFAULT;
  }

  if (debug)
    pr_info("ch368_io: read %d/%d chars at offset %d from I/O memory\n", real, (int)count, (int)*ppos);

  return real;
}

static ssize_t ch368_io_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
  int i, real;
  struct ch368_io_struct *data = file->private_data;
  unsigned char kbuf[BUF_SIZE];
  unsigned int port;
  
  /* Check for overflow */
  real = min((int)data->iolen - (int)*ppos, (int)count);
  if (debug)
    pr_info ("real= %d\n", real);
  
  // set base offset from ppos
  if (debug)
    pr_info ("write offset= %x\n", (int)*ppos);
  port = data->iobase + *ppos;
    
  /* Copy data to board */
  if (copy_from_user(kbuf, buf, real))
    return -EFAULT;

  for (i = 0; i < real; i += sizeof(char)) {
    if (debug)
      pr_info ("writing 0x%x @ offset %d\n", *(kbuf+i), i);
    outb(*(kbuf+i), port + i);
  }

  if (debug)
    pr_info("ch368_io: write %d/%d chars at offset %d from I/O memory\n", real, (int)count, (int)*ppos);

  return real;
}

loff_t ch368_io_llseek(struct file *file, loff_t off, int whence)
{
  loff_t newpos = 0;

  switch(whence) {
  case 0: /* SEEK_SET */
    newpos = off;
    break;

  case 1: /* SEEK_CUR */
    newpos = file->f_pos + off;
    break;

  case 2: /* SEEK_END */
    newpos = BUF_SIZE + off;
    break;

  default: /* can't happen */
    return -EINVAL;
  }
  if (newpos < 0 || newpos > BUF_SIZE)
    return -EINVAL;

  file->f_pos = newpos;
  
  return newpos;
}

static int ch368_io_open(struct inode *inode, struct file *file)
{
  int minor = MINOR(inode->i_rdev);
  struct list_head *cur;
  struct ch368_io_struct *data;

  printk("ch368_io_open()\n");

  list_for_each(cur, &ch368_io_list) {
    data = list_entry(cur, struct ch368_io_struct, link);

    if (data->minor == minor) {
      file->private_data = data;

      return 0;
    }
  }

  pr_warn("ch368_io: minor %d not found\n", minor);

  return -ENODEV;
}

static int ch368_io_release(struct inode *inode, struct file *file)
{
  printk("ch368_io_release()\n");

  file->private_data = NULL;

  return 0;
}

static struct file_operations ch368_io_fops = {
  .owner =	THIS_MODULE,
  .read =	ch368_io_read,
  .write =	ch368_io_write,
  .open =	ch368_io_open,
  .llseek =	ch368_io_llseek,
  .release =	ch368_io_release,
};

/*
 * PCI handling
 */
static int ch368_io_probe(struct pci_dev *dev, const struct pci_device_id *ent)
{
  int i, ret = 0;
  struct ch368_io_struct *data;
  static int minor = 0;
  struct device *device;

  pr_info("ch368_io: found %x:%x\n", ent->vendor, ent->device);
  pr_info("ch368_io: using major %d and minor %d for this device\n", major, minor);

  /* Allocate a private structure and reference it as driver's data */
  data = (struct ch368_io_struct *)kmalloc(sizeof(struct ch368_io_struct), GFP_KERNEL);
  if (data == NULL) {
    pr_info("ch368_io: unable to allocate private structure\n");
    ret = -ENOMEM;
    goto cleanup_kmalloc;
  }

  // Add device to the class -> create entry in /dev
  if (ch368_io_class) {
    device = device_create(ch368_io_class, NULL, MKDEV (major, minor),  NULL, "ch368_io%d", minor);
    if (IS_ERR(device))
      dev_err(&(dev->dev), "ch368_io: can't create device %d !\n", minor);
  }

  pci_set_drvdata(dev, data);

  /* Init private field */
  data->dev = dev;
  data->minor = minor++;

  /* Initialize device before it's used by the driver */
  ret = pci_enable_device(dev);
  if (ret < 0) {
    pr_warn("ch368_io: unable to initialize PCI device\n");

    goto cleanup_pci_enable;
  }

  /* Reserve PCI I/O and memory resources */
  ret = pci_request_regions(dev, "ch368_io");
  if (ret < 0) {
    pr_warn("ch368_io: unable to reserve PCI resources\n");

    goto cleanup_regions;
  }

  data->iobase = 0;
  /* Inspect PCI BARs and search IORESOURCE_IO */
  for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
    if (pci_resource_len(dev, i) == 0)
      continue;

    if (pci_resource_start(dev, i) == 0)
      continue;

    pr_info("ch368_io: BAR %d (%#08x-%#08x), len=%d, flags=%#08x\n", i, (u32) pci_resource_start(dev, i), (u32) pci_resource_end(dev, i), (u32) pci_resource_len(dev, i), (u32) pci_resource_flags(dev, i));

    if (pci_resource_flags(dev, i) & IORESOURCE_IO) {
      data->iobase = pci_resource_start(dev, i);
      data->iolen = pci_resource_len(dev, i);
      pr_info("ch368_io: BAR %d is IO_RESOURCE_IO @ %x!\n", i, data->iobase);

      break; // we use the first IO
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
      ret = request_irq(dev->irq, ch368_io_irq_handler, 0, "ch368_io", data);
    if (ret < 0) {
      pr_warn("ch368_io: unable to register irq handler\n");
      goto cleanup_irq;
    }
  }
  else
    pr_info("ch368_io: no IRQ!\n");

  /* Link the new data structure with others */
  list_add_tail(&data->link, &ch368_io_list);

  return 0;

 cleanup_irq:
  pci_release_regions(dev);
 cleanup_regions:
  pci_disable_device(dev);
 cleanup_pci_enable:
  kfree(data);
 cleanup_kmalloc:
  return ret;
}

static void ch368_io_remove(struct pci_dev *dev)
{
  struct ch368_io_struct *data = pci_get_drvdata(dev);

  if (dev->pin) {
    free_irq(dev->irq, data);
    pci_disable_msi(dev);
  }


  if (ch368_io_class)
    device_destroy(ch368_io_class, MKDEV(major, data->minor));

  pci_release_regions(dev);
  pci_disable_device(dev);

  list_del(&data->link);

  kfree(data);

  pr_info("ch368_io: device removed\n");
}

static struct pci_driver ch368_io_driver = {
  .name =	"ch368_io",
  .id_table =	ch368_io_id_table,
  .probe =	ch368_io_probe,		/* Init one device */
  .remove =	ch368_io_remove,		/* Remove one device */
};

/*
 * Init and Exit
 */
static int __init ch368_io_init(void)
{
  int ret;

  /* Register the device driver */
  ret = register_chrdev(major, "ch368_io", &ch368_io_fops);
  if (ret < 0) {
    pr_warn("ch368_io: unable to get a major\n");

    return ret;
  }

  if (major == 0)
    major = ret; /* dynamic value */

    // Create class in /sys/class
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 4, 0)
  ch368_io_class = class_create (THIS_MODULE, "ch368_io");
#else
  ch368_io_class = class_create ("ch368_io");
#endif  
  if (IS_ERR(ch368_io_class)) {
    pr_warn("ch368_io: failed to create class\n");
    ch368_io_class = NULL;
  }

  /* Register PCI driver */
  ret = pci_register_driver(&ch368_io_driver);
  if (ret < 0) {
    pr_warn("ch368_io: unable to register PCI driver\n");
    unregister_chrdev(major, "ch368_io");

    return ret;
  }

  return 0;
}

static void __exit ch368_io_exit(void)
{
  pci_unregister_driver(&ch368_io_driver);

  unregister_chrdev(major, "ch368_io");

  if (ch368_io_class)
    class_destroy (ch368_io_class);
}

/*
 * Module entry points
 */
module_init(ch368_io_init);
module_exit(ch368_io_exit);
