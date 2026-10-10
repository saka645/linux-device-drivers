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
		[0] = {.buffer = device_buffer1, .size = MEM_SIZE_MAX_PCD1, .serial_number = "PCDEV1111", .perm = RDONLY},
		[1] = {.buffer = device_buffer2, .size = MEM_SIZE_MAX_PCD2, .serial_number = "PCDEV2222", .perm = WRONLY},
		[2] = {.buffer = device_buffer3, .size = MEM_SIZE_MAX_PCD3, .serial_number = "PCDEV3333", .perm = RDWR},
		[3] = {.buffer = device_buffer4, .size = MEM_SIZE_MAX_PCD4, .serial_number = "PCDEV4444", .perm = RDWR}
	}
};

loff_t pcd_lseek(struct file *filp, loff_t offset, int whence){
	loff_t temp;
	pr_info("lseek req\n");
	pr_info("current file pos=%lld\n", filp->f_pos);
	switch(whence)
	{
		case SEEK_SET:
		    if(offset > DEV_MEM_SIZE || offset < 0)
				return -EINVAL;
			filp->f_pos = offset;
			break;	
		case SEEK_CUR:
			temp = filp->f_pos + offset;
			if(temp > DEV_MEM_SIZE || temp < 0)
				return -EINVAL;
			filp->f_pos += temp;
			break;
		case SEEK_END:
			temp = filp->f_pos + offset;
			if(temp > DEV_MEM_SIZE || temp < 0)
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
	if((*f_pos + count) > DEV_MEM_SIZE)
	{
		count = DEV_MEM_SIZE - *f_pos;
	}
	/*copy to user*/
	if(copy_to_user(buff,&device_buffer[*f_pos], count))
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
	if((*f_pos + count) > DEV_MEM_SIZE)
	{
		count = DEV_MEM_SIZE - *f_pos;
	}
	if(!count) 
		return -ENOMEM;
	/*copy from user*/
	if(copy_from_user(&device_buffer[*f_pos], buff, count))
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
	/*ALLOCATE*/
	ret = alloc_chrdev_region(&pcdrv_data.device_number,0,NO_OF_DEVICES,"pcd_devices");
	if(ret < 0)
	{
		pr_err("Alloc chrdev failed\n");
		goto out;
	}
	for(int i=0; i<NO_OF_DEVICES; i++)
	{
		pr_info("Device-%d, major=%d, minor=%d\n",i,MAJOR(pcdrv_data.device_number+i),MINOR(pcdrv_data.device_number+i));
	}
	for(int i=0; i<NO_OF_DEVICES; i++)
	{
		cdev_init(&pcdrv_data.cdev[i], &pcd_fops);

		pcdrv_data.pcdev_data[i].cdev.owner = THIS_MODULE;;
		ret = cdev_add(&pcdrv_data.cdev[i], pcdrv_data.device_number + i, 1);
		if(ret < 0)
		{
			pr_err("cdev_add failed for device %d\n", i);
			goto unregister_chrdev;
		}


		/*create device class*/
		class_pcd = class_create(THIS_MODULE, "pcd_class");
		if(IS_ERR(class_pcd))
		{
			pr_err("class creation failed\n");
			ret = PTR_ERR(class_pcd);
			goto cdev_del;
		}

		device_pcd = device_create(class_pcd, NULL, pcdrv_data.device_number+i, NULL, "pcd-%d",i);
		if(IS_ERR(device_pcd))
		{
			pr_err("device creation failed\n");
			ret = PTR_ERR(device_pcd);
			goto class_del;
		}
	}
	pr_info("Module init sucessfull\n");
	return 0;

class_del:
	class_destroy(class_pcd);
cdev_del:
	/*delete cdev*/
	cdev_del(&pcd_cdev);
unregister_chrdev:
	unregister_chrdev_region(device_number,1);
out:
	return ret;
}


static void __exit pcd_driver_cleanup(void)
{
	device_destroy(class_pcd, device_number);
	class_destroy(class_pcd);
	cdev_del(&pcd_cdev);
	unregister_chrdev_region(device_number,1);
	pr_info("Module unloaded\n");
}


module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ME");
MODULE_DESCRIPTION("SIMPLE MODULE");
MODULE_INFO(board, "pocket beagle");


