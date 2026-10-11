#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>

#define MEM_SIZE_MAX_PCD1 1024
#define MEM_SIZE_MAX_PCD2 512
#define MEM_SIZE_MAX_PCD3 1024
#define MEM_SIZE_MAX_PCD4 512
#define NO_OF_DEVICES     4

#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt,__func__

char device_buffer1[MEM_SIZE_MAX_PCD1];
char device_buffer2[MEM_SIZE_MAX_PCD2];
char device_buffer3[MEM_SIZE_MAX_PCD3];
char device_buffer4[MEM_SIZE_MAX_PCD4];


struct pcdev_private_data
{
	char *buffer;
	unsigned size;
	const char *serial_number;
	int perm;
	struct cdev cdev;
};

/*DRIVER PVT DATA STR*/
struct pcdrv_private_data
{
	int total_devices;
	dev_t device_number;
	struct class *class_pcd;
	struct device *device_pcd;
	struct pcdev_private_data pcdev_data[NO_OF_DEVICES];
};

struct pcdrv_private_data pcdrv_data = 
{
	.total_devices = NO_OF_DEVICES,
	.device_number = 0,
	.class_pcd = NULL,
	.device_pcd = NULL,
	.pcdev_data =
	{
		[0] = {.buffer = device_buffer1, .size = MEM_SIZE_MAX_PCD1, .serial_number = "PCDEV1111", .perm = O_RDONLY},
		[1] = {.buffer = device_buffer2, .size = MEM_SIZE_MAX_PCD2, .serial_number = "PCDEV2222", .perm = O_WRONLY},
		[2] = {.buffer = device_buffer3, .size = MEM_SIZE_MAX_PCD3, .serial_number = "PCDEV3333", .perm = O_RDWR},
		[3] = {.buffer = device_buffer4, .size = MEM_SIZE_MAX_PCD4, .serial_number = "PCDEV4444", .perm = O_RDWR}
	}
};

int check_permission(int dev_pern, int acc_mode)
{
	if(dev_pern == O_RDWR)
		return 0;
	if(dev_pern == O_RDONLY && (acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE))
		return 0;
	if(dev_pern == O_WRONLY && (acc_mode & FMODE_WRITE) && !(acc_mode & FMODE_READ))
		return 0;
	return -EPERM;
}

loff_t pcd_lseek(struct file *filp, loff_t offset, int whence){
	loff_t temp;
	struct pcdev_private_data *pcdev_data = (struct pcdev_private_data *)filp->private_data;
	int max_size = pcdev_data->size;
	pr_info("lseek req\n");
	pr_info("current file pos=%lld\n", filp->f_pos);
	switch(whence)
	{
		case SEEK_SET:
		    if(offset > max_size || offset < 0)
				return -EINVAL;
			filp->f_pos = offset;
			break;	
		case SEEK_CUR:
			temp = filp->f_pos + offset;
			if(temp > max_size || temp < 0)
				return -EINVAL;
			filp->f_pos += temp;
			break;
		case SEEK_END:
			temp = filp->f_pos + offset;
			if(temp > max_size || temp < 0)
				return -EINVAL;	
			filp->f_pos = temp;
			break;
		default:
			return -EINVAL;
	}
	pr_info("update file pos=%lld\n",filp->f_pos);
	return filp->f_pos;
}
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t * f_pos)
{
	pr_info("read req %zu bytes\n",count);
	pr_info("current file pos=%lld\n",*f_pos);
	struct pcdev_private_data *pcdev_data = (struct pcdev_private_data *)filp->private_data;
	int max_size = pcdev_data->size;

	if((*f_pos + count) > max_size)
	{
		count = max_size - *f_pos;
	}
	/*copy to user*/
	if(copy_to_user(buff,pcdev_data->buffer+(*f_pos), count))
	{
		return -EFAULT;
	}
	/*UPDATE current file pos*/
	*f_pos += count;
	pr_info("num of byte read =%zu\n",count);
	pr_info("update file pos=%lld\n",*f_pos);
	return count;
}
ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos){
	pr_info("write req %zu bytes\n",count);
	pr_info("current file pos=%lld\n",*f_pos);
	struct pcdev_private_data *pcdev_data = (struct pcdev_private_data *)filp->private_data;
	int max_size = pcdev_data->size;

	if((*f_pos + count) > max_size)
	{
		count = max_size - *f_pos;
	}
	if(!count) 
		return -ENOMEM;
	/*copy from user*/
	if(copy_from_user(pcdev_data->buffer+(*f_pos), buff, count))
	{
		return -EFAULT;
	}
	/*UPDATE current file pos*/
	*f_pos += count;
	pr_info("num of byte write =%zu\n",count);
	pr_info("update file pos=%lld\n",*f_pos);
	return count;
}
int pcd_open(struct inode *inode, struct file *filp){
	int ret;
	int minor_n;
	struct pcdev_private_data *pcdev_data;
	/*find out which device file is opened*/
	minor_n = MINOR(inode->i_rdev);
	pr_info("minor access=%d\n",minor_n);
	/*get device's private data structure*/
	pcdev_data = container_of(inode->i_cdev, struct pcdev_private_data, cdev);
	/*save device's private data structure in filp->private_data*/
	filp->private_data = pcdev_data;
	/*check permission*/
	ret = check_permission((pcdev_data->perm), filp->f_mode);
	if(ret)
		return -EPERM;		
		pr_info("open succesful\n");
		return 0;
}
int pcd_release(struct inode *inode, struct file *filp){
	pr_info("close succesful\n");
		return 0;
}


struct file_operations pcd_fops =
{
	.open = pcd_open, 
	.write = pcd_write,
	.read = pcd_read,
	.llseek = pcd_lseek,
	.release = pcd_release,
	.owner = THIS_MODULE
};



static int __init pcd_driver_init(void)
{	
	int ret;
	int i=0;
	/*ALLOCATE*/
	ret = alloc_chrdev_region(&pcdrv_data.device_number,0,NO_OF_DEVICES,"pcd_devices");
	if(ret < 0)
	{
		pr_err("Alloc chrdev failed\n");
		goto out;
	}
	for(i=0; i<NO_OF_DEVICES; i++)
	{
		pr_info("Device-%d, major=%d, minor=%d\n",i,MAJOR(pcdrv_data.device_number+i),MINOR(pcdrv_data.device_number+i));
	}
	/*create device class*/
	pcdrv_data.class_pcd = class_create(THIS_MODULE, "pcd_class");
	if(IS_ERR(pcdrv_data.class_pcd))
	{
		pr_err("class creation failed\n");
		ret = PTR_ERR(pcdrv_data.class_pcd);
		goto unreg_chrdev;
	}

	for(i=0; i<NO_OF_DEVICES; i++)
	{
		cdev_init(&pcdrv_data.pcdev_data[i].cdev, &pcd_fops);

		pcdrv_data.pcdev_data[i].cdev.owner = THIS_MODULE;;
		ret = cdev_add(&pcdrv_data.pcdev_data[i].cdev, pcdrv_data.device_number + i, 1);
		if(ret < 0)
		{
			pr_err("cdev_add failed for device %d\n", i);
			goto cdev_del;
		}
	}



	for(i=0; i<NO_OF_DEVICES; i++)
	{
		pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL, pcdrv_data.device_number + i, NULL, "pcd-%d",i);
		if(IS_ERR(pcdrv_data.device_pcd))
		{
			pr_err("device creation failed\n");
			ret = PTR_ERR(pcdrv_data.device_pcd);
			goto class_del;
		}
	}
	pr_info("Module init sucessfull\n");
	return 0;

cdev_del:
class_del:
	for(;i>=0; i--)
	{
		device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_number + i);
	}	
		/*delete cdev*/
	for(i=0; i<NO_OF_DEVICES; i++)
	{
		cdev_del(&pcdrv_data.pcdev_data[i].cdev);
	}
	class_destroy(pcdrv_data.class_pcd);

unreg_chrdev:
	unregister_chrdev_region(pcdrv_data.device_number,NO_OF_DEVICES);

out:
	return ret;
}


static void __exit pcd_driver_cleanup(void)
{
	int i=0;
	for(;i>=0; i--)
	{
		device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_number + i);
	}	
		/*delete cdev*/
	for(i=0; i<NO_OF_DEVICES; i++)
	{
		cdev_del(&pcdrv_data.pcdev_data[i].cdev);
	}
	class_destroy(pcdrv_data.class_pcd);
	unregister_chrdev_region(pcdrv_data.device_number,NO_OF_DEVICES);
	pr_info("Module unloaded\n");
}


module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ME");
MODULE_DESCRIPTION("SIMPLE MODULE");
MODULE_INFO(board, "pocket beagle");


