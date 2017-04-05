#include <linux/init.h>      // Macros used to mark up functions e.g., __init __exit
#include <linux/module.h>    // Core header for loading LKMs into the kernel
#include <linux/device.h>    // Header to support the kernel Driver Model
#include <linux/kernel.h>    // Contains types, macros, functions for the kernelmak
#include <linux/fs.h>        // Header for the Linux file system support
#include <asm/uaccess.h>     // Required for the copy to user function
#include <linux/slab.h>
#include <linux/cdev.h>

//

MODULE_LICENSE("GPL");          	///< The license type -- this affects runtime behavior
MODULE_AUTHOR("OpenR2");      		///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("Holoprojector");  	///< The description -- see modinfo
MODULE_VERSION("0.1");          	///< The version of the module
 
static int majorNumber=0; 
static int minorNumbers=3;
static int minorNumbersToDestroy=0;
//

static int     OpenR2_open(struct inode *, struct file *);
static int     OpenR2_release(struct inode *, struct file *);
static ssize_t OpenR2_read(struct file *, char *, size_t, loff_t *);
static ssize_t OpenR2_write(struct file *, const char *, size_t, loff_t *);

static dev_t OpenR2_dev;

static struct class *OpenR2_class=NULL;

static struct device **OpenR2_devices;

static struct cdev OpenR2_cdev;

static struct file_operations OpenR2_fops =
{
   .owner = THIS_MODULE,
   .open = OpenR2_open,
   .read = OpenR2_read,
   .write = OpenR2_write,
   .release = OpenR2_release,
};

//

static int __init OpenR2_init(void)
{

	// call alloc_chrdev_region() to get major number and a range of minor numbers to work with
	// create device classes for devices with class_create()
	// for each device call cdev_init() and cdev_add()to add the character dvice to the system
	// for each device call device_create() to have udev create devices in /dev

	int result;
	int minor=0;
	int j=0;

	printk(KERN_INFO "%s: >> %s\n","Holoprojector",__func__);
   
	// Try to dynamically allocate a major number for the device

	result = alloc_chrdev_region(&OpenR2_dev, 0, minorNumbers, "OpenR2-Holoprojector");
	if( result < 0 )
	{
		printk(KERN_WARNING "%s: alloc_chrdev_region FAILED\n","Holoprojector");
		return -1;
	}

	majorNumber=MAJOR(OpenR2_dev);

	printk(KERN_INFO "%s:   major=%d %d\n","Holoprojector",MAJOR(OpenR2_dev),0);
	printk(KERN_INFO "%s:   major=%d %d\n","Holoprojector",MAJOR(OpenR2_dev),1);
	printk(KERN_INFO "%s:   major=%d %d\n","Holoprojector",MAJOR(OpenR2_dev),2);

	// udev virtual device class

        OpenR2_class = class_create(THIS_MODULE,"OpenR2-Holoprojector");
	if(IS_ERR(OpenR2_class))
	{
		printk(KERN_WARNING "%s: class_create FAILED\n","Holoprojector");
		unregister_chrdev_region(OpenR2_dev, minorNumbers);
		return -1;
	}


	// need to allocate an array of devices

	OpenR2_devices = (struct device **)kmalloc(minorNumbers * sizeof(struct device *),GFP_KERNEL);
	if(OpenR2_devices == NULL)
	{
		printk(KERN_WARNING "%s: kmalloc FAILED\n","Holoprojector");
		class_destroy(OpenR2_class);
		unregister_chrdev_region(OpenR2_dev, minorNumbers);
		return -1;		
	}	
	memset(OpenR2_devices,0,minorNumbers * sizeof(struct device *));
	
	// use the newer udev method to create a char devices

	for(minor=0; minor<minorNumbers; minor++)
	{
		printk(KERN_WARNING "%s: %d %d\n","Holoprojector",majorNumber,minor);

		OpenR2_devices[minor]=device_create(OpenR2_class,NULL,MKDEV(majorNumber,minor),NULL,"OpenR2-Holoprojector%d",minor);
		if(IS_ERR(OpenR2_devices[minor]))
		{
			printk(KERN_WARNING "%s: device_create %d %d FAILED\n","Holoprojector%",majorNumber,0);
			if(minorNumbersToDestroy>0)
			{
				for(j=0;j<minorNumbersToDestroy;j++)
				{
					device_destroy(OpenR2_class,MKDEV(majorNumber,j));
				}		
			}
			kfree(OpenR2_devices);
			class_destroy(OpenR2_class);
			unregister_chrdev_region(OpenR2_dev, minorNumbers);
			return -1;
		}

		minorNumbersToDestroy++;
	}

	//

	cdev_init(&OpenR2_cdev,&OpenR2_fops);
	
	//

	result=cdev_add(&OpenR2_cdev,OpenR2_dev,minorNumbers);
	if( result < 0 )
	{
		printk(KERN_WARNING "%s: Error registering device driver with major number %d\n","Holoprojector",majorNumber);

		if(minorNumbersToDestroy>0)
		{
			for(j=0;j<minorNumbersToDestroy;j++)
			{
				device_destroy(OpenR2_class,MKDEV(majorNumber,j));
			}
		}
		kfree(OpenR2_devices);
		class_destroy(OpenR2_class);                  
		unregister_chrdev_region(OpenR2_dev, minorNumbers);

		return -1;
	}


	//

	printk(KERN_INFO "%s: << %s\n","Holoprojector",__func__);
	return(0);

}
 
static void __exit OpenR2_exit(void){
  
	int j;

	printk(KERN_INFO "%s: >> %s\n","Holoprojector",__func__);

	cdev_del(&OpenR2_cdev);
	if(minorNumbersToDestroy>0)
	{
		for(j=0;j<minorNumbersToDestroy;j++)
		{
			device_destroy(OpenR2_class,MKDEV(majorNumber,j));
		}
	}
	kfree(OpenR2_devices);
	class_destroy(OpenR2_class);   
               
	unregister_chrdev_region(OpenR2_dev, minorNumbers);

	printk(KERN_INFO "%s: << %s\n","Holoprojector",__func__);
	return;
}
 
static const char mbuffer[] = "Astromech Corporation: Holoprojector\0";
static ssize_t OpenR2_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
  	
	printk(KERN_INFO "%s: >> %s\n","Holoprojector",__func__);

	printk(KERN_INFO "%s:    %i %u\n","Holoprojector",(int)*offset,(unsigned int)len);

    	// If offset is behind the end of a file we have nothing to read
    	if( *offset >= sizeof(mbuffer))
	{
        	return(0);
	}    

	// If a user tries to read more than we have, read only as many bytes as we have
    	if( *offset + len > sizeof(mbuffer))
	{
        	len = sizeof(mbuffer) - *offset;
	}
    
	if( copy_to_user(buffer, mbuffer + *offset, len) != 0 )
	{
        	return(-EFAULT);
	}    
    
	printk(KERN_INFO "%s:    %s\n","Holoprojector",mbuffer);

	// Move reading position
    	*offset += len;

	printk(KERN_INFO "%s: << %s\n","Holoprojector",__func__);

	return(len);
}


static ssize_t OpenR2_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
 
	printk(KERN_INFO "%s: >> %s\n","Holoprojector",__func__);

 
	printk(KERN_INFO "%s: << %s\n","Holoprojector",__func__);
  	return(-EFAULT);
}

static int OpenR2_open(struct inode *inodep, struct file *filep){
 
	printk(KERN_INFO "%s: >> %s\n","Holoprojector",__func__);
	printk(KERN_INFO "%s:    %d %d\n","Holoprojector",imajor(filep->f_path.dentry->d_inode),iminor(filep->f_path.dentry->d_inode));
	printk(KERN_INFO "%s: << %s\n","Holoprojector",__func__);

   	return(0);
}

static int OpenR2_release(struct inode *inodep, struct file *filep){
  
	printk(KERN_INFO "%s: >> %s\n","Holoprojector",__func__);
	printk(KERN_INFO "%s: << %s\n","Holoprojector",__func__);

   	return(0);
}

module_init(OpenR2_init);
module_exit(OpenR2_exit);
