/*
 * PCI demo driver (IO based) for CH368 board
 * 
 * # insmod ch368_io.ko
 * # mknod /dev/ch368_io c <major> 0
 * # ./led_blink
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

MODULE_DESCRIPTION("pcidemo_io");
MODULE_AUTHOR("Pierre Ficheux");
MODULE_LICENSE("GPL");

#define BUF_SIZE 256

/*
 * Arguments
 */
static int major = 0; /* Major number */
module_param(major, int, 0660);
MODULE_PARM_DESC(major, "Static major number (none = dynamic)");

/*
 * Supported devices
*/

#define VENDOR_ID	0x1c00
#define DEVICE_ID	0x5834

static struct pci_device_id pcidemo_io_id_table[] = {
  {VENDOR_ID, DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
  {0,}	/* 0 terminated list */
};
MODULE_DEVICE_TABLE(pci, pcidemo_io_id_table);

/*
 * Global variables
 */
static LIST_HEAD(pcidemo_io_list);

struct pcidemo_io_struct {
  struct list_head	link; /* Double linked list */
  struct pci_dev		*dev; /* PCI device */
  int			minor; /* Minor number */
  unsigned int		iobase;
  u32			iolen;
};

/*
 * Event handlers
 */
static irqreturn_t pcidemo_io_irq_handler(int irq, void *dev_id)
{
  struct pcidemo_io_struct *data = (struct pcidemo_io_struct *)dev_id;

  printk(KERN_INFO "pcidemo_io: interrupt from device %d\n", data->minor);

  return IRQ_HANDLED;
}

/*
 * File operations
 */
static ssize_t pcidemo_io_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
  int i, j, bank = DEVICE_COUNT_RESOURCE;
  struct pcidemo_io_struct *data = file->private_data;
  unsigned char kbuf[BUF_SIZE];
  unsigned int port;
  int real;

  /* Find the first remapped I/O memory bank to read */
  for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
    if (data->iobase != 0) {
      bank = i;
      break;
    }
  }

  /* No bank found */
  if (bank == DEVICE_COUNT_RESOURCE) {
    printk(KERN_INFO "pcidemo_io: no I/O memory bank to read\n");
    return -ENXIO;
  }

  /* Check for overflow */
  real = min((int)data->iolen - (int)*ppos, (int)count);

  pr_info ("ppos= %x\n", (int)*ppos);
  port = data->iobase + *ppos;

  /* Copy data from board */
  if (real) {
    /*
    for (j = 0; j < real; j += sizeof(long)) {
      pr_info ("port[%d] = %x\n", j, inl(port+j));
      *((unsigned long *)(kbuf + j)) = inl(port + j);
    }
    */
    for (j = 0; j < real; j += sizeof(char)) {
      pr_info ("port[%d] = %x\n", j, inb(port+j));
      *(kbuf + j) = inb(port + j);
    }

    if (copy_to_user(buf, kbuf, real))
      return -EFAULT;
  }
  
  //  port = data->iobase + 0xe8;
  //  pr_info ("port= %x\n", inl(port));

  printk(KERN_INFO "pcidemo_io: read %d/%d chars at offset %d from I/O memory bank %d\n", real, (int)count, (int)*ppos, bank);

  return real;
}

static ssize_t pcidemo_io_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
  int i, j, bank = DEVICE_COUNT_RESOURCE;
  struct pcidemo_io_struct *data = file->private_data;
  unsigned char kbuf[BUF_SIZE];
  unsigned int port;
  int real;

  /* Find the first remapped I/O memory bank to read */
  for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
    if (data->iobase != 0) {
      bank = i;
      break;
    }
  }

  /* No bank found */
  if (bank == DEVICE_COUNT_RESOURCE) {
    printk(KERN_INFO "pcidemo_io: no I/O memory bank to read\n");
    return -ENXIO;
  }

  /* Check for overflow */
  real = min((int)data->iolen - (int)*ppos, (int)count);
  pr_info ("real= %d\n", real);
  
  // set base offset
  port = data->iobase + *ppos;
    
  /* Copy data to board */
  if (copy_from_user(kbuf, buf, real))
    return -EFAULT;

  for (j = 0; j < real; j += sizeof(char)) {
    pr_info ("writing 0x%x @ offset %d\n", *(kbuf+j), j);
    outb(*(kbuf+j), port + j);
  }
  
  printk(KERN_INFO "pcidemo_io: write %d/%d chars at offset %d from I/O memory bank %d\n", real, (int)count, (int)*ppos, bank);

  return real;
}

loff_t pcidemo_io_llseek(struct file *file, loff_t off, int whence)
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

static int pcidemo_io_open(struct inode *inode, struct file *file)
{
  int minor = MINOR(inode->i_rdev);
  struct list_head *cur;
  struct pcidemo_io_struct *data;

  printk("pcidemo_io_open()\n");

  list_for_each(cur, &pcidemo_io_list) {
    data = list_entry(cur, struct pcidemo_io_struct, link);

    if (data->minor == minor) {
      file->private_data = data;

      return 0;
    }
  }

  printk(KERN_WARNING "pcidemo_io: minor %d not found\n", minor);

  return -ENODEV;
}

static int pcidemo_io_release(struct inode *inode, struct file *file)
{
  printk("pcidemo_io_release()\n");

  file->private_data = NULL;

  return 0;
}

static struct file_operations pcidemo_io_fops = {
  .owner =	THIS_MODULE,
  .read =	pcidemo_io_read,
  .write =	pcidemo_io_write,
  .open =	pcidemo_io_open,
  .llseek =	pcidemo_io_llseek,
  .release =	pcidemo_io_release,
};

/*
 * PCI handling
 */
static int pcidemo_io_probe(struct pci_dev *dev, const struct pci_device_id *ent)
{
  int i, ret = 0;
  struct pcidemo_io_struct *data;
  static int minor = 0;

  printk(KERN_INFO "pcidemo_io: found %x:%x\n", ent->vendor, ent->device);
  printk(KERN_INFO "pcidemo_io: using major %d and minor %d for this device\n", major, minor);

  /* Allocate a private structure and reference it as driver's data */
  data = (struct pcidemo_io_struct *)kmalloc(sizeof(struct pcidemo_io_struct), GFP_KERNEL);
  if (data == NULL) {
    printk(KERN_INFO "pcidemo_io: unable to allocate private structure\n");

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
    printk(KERN_WARNING "pcidemo_io: unable to initialize PCI device\n");

    goto cleanup_pci_enable;
  }

  /* Reserve PCI I/O and memory resources */
  ret = pci_request_regions(dev, "pcidemo_io");
  if (ret < 0) {
    printk(KERN_WARNING "pcidemo_io: unable to reserve PCI resources\n");

    goto cleanup_regions;
  }

  /* Inspect PCI BARs and search IORESOURCE_IO */
  for (i=0; i < DEVICE_COUNT_RESOURCE; i++) {
    data->iobase = 0;

    if (pci_resource_len(dev, i) == 0)
      continue;

    if (pci_resource_start(dev, i) == 0)
      continue;

    printk(KERN_INFO "pcidemo_io: BAR %d (%#08x-%#08x), len=%d, flags=%#08x\n", i, (u32) pci_resource_start(dev, i), (u32) pci_resource_end(dev, i), (u32) pci_resource_len(dev, i), (u32) pci_resource_flags(dev, i));

    if (pci_resource_flags(dev, i) & IORESOURCE_IO) {
      data->iobase = pci_resource_start(dev, i);
      data->iolen = pci_resource_len(dev, i);
      printk(KERN_INFO "pcidemo_io: BAR %d is IO_RESOURCE_IO @ %x!\n", i, data->iobase);

      break;
    }
  }

  /* Install the irq handler */
  if (dev->pin) {
    ret = request_irq(dev->irq, pcidemo_io_irq_handler, IRQF_SHARED, "pcidemo_io", data);
    if (ret < 0) {
      printk(KERN_WARNING "pcidemo_io: unable to register irq handler\n");

      goto cleanup_irq;
    }
  }
  else
    printk(KERN_INFO "pcidemo_io: no IRQ!\n");

  /* Link the new data structure with others */
  list_add_tail(&data->link, &pcidemo_io_list);

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

static void pcidemo_io_remove(struct pci_dev *dev)
{
  struct pcidemo_io_struct *data = pci_get_drvdata(dev);

  if (dev->pin)
    free_irq(dev->irq, data);

  pci_release_regions(dev);
  pci_disable_device(dev);

  list_del(&data->link);

  kfree(data);

  printk(KERN_INFO "pcidemo_io: device removed\n");
}

static struct pci_driver pcidemo_io_driver = {
  .name =	"pcidemo_io",
  .id_table =	pcidemo_io_id_table,
  .probe =	pcidemo_io_probe,		/* Init one device */
  .remove =	pcidemo_io_remove,		/* Remove one device */
};

/*
 * Init and Exit
 */
static int __init pcidemo_io_init(void)
{
  int ret;

  /* Register the device driver */
  ret = register_chrdev(major, "pcidemo_io", &pcidemo_io_fops);
  if (ret < 0) {
    printk(KERN_WARNING "pcidemo_io: unable to get a major\n");

    return ret;
  }

  if (major == 0)
    major = ret; /* dynamic value */

  /* Register PCI driver */
  ret = pci_register_driver(&pcidemo_io_driver);
  if (ret < 0) {
    printk(KERN_WARNING "pcidemo_io: unable to register PCI driver\n");
    unregister_chrdev(major, "pcidemo_io");

    return ret;
  }

  return 0;
}

static void __exit pcidemo_io_exit(void)
{
  pci_unregister_driver(&pcidemo_io_driver);

  unregister_chrdev(major, "pcidemo_io");
}

/*
 * Module entry points
 */
module_init(pcidemo_io_init);
module_exit(pcidemo_io_exit);
