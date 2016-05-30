#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cdev.h>		// 2.6
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");

#define DRVR		"refcount"
#define DRVRMAJOR  33
#define DRVRMINOR  	0	// 2.6
#define DRVRNUMDEVS  	1	// 2.6

static unsigned int DRVRmajor = DRVRMAJOR;
static unsigned int DRVRparm = DRVRMAJOR;

module_param(DRVRparm,int,0);

dev_t   firstdevno;

struct DRVRdev_struct {
        unsigned int    counter;
        char            *storage;
        struct cdev     cdev;
};

static struct DRVRdev_struct myDRVR;
static struct proc_dir_entry* proc;


typedef struct lnode_ *lnode;
int refcount = 0;
lnode reference_buffer[4096];
lnode head = NULL;
lnode tail = NULL;

struct lnode_ {
	char c;
	int id;
	lnode next;
};

lnode create_lnode(int id, char c ) {
	lnode node =  vmalloc( sizeof(lnode) );
	node->id = id;
	node->c = c;
	node->next = NULL;
	reference_buffer[refcount++] = node;
	return node;
}




static int DRVR_open (struct inode *inode, struct file *file)
{
	struct DRVRdev_struct *local =   container_of(inode->i_cdev, struct DRVRdev_struct, cdev);

	if ( file->f_flags & O_TRUNC )
	{
		printk(KERN_ALERT "file '%s' opened O_TRUNC\n", file->f_path.dentry->d_name.name);

		local->storage[0]=0;
        local->counter=0;
    }

	if ( file->f_flags & O_RDWR )
	{
			printk(KERN_ALERT "file '%s' opened O_ORDWR\n", file->f_path.dentry->d_name.name);
	}

	if ( file->f_flags & O_APPEND )
    {
    	printk(KERN_ALERT "file '%s' opened O_APPEND\n", file->f_path.dentry->d_name.name);
    }

    file->private_data=local;

	return 0;
}

static int DRVR_release (struct inode *inode, struct file *file)
{
 	struct DRVRdev_struct *local=file->private_data;

	if( local->counter <= 0 )
		return 0; // overcome compiler warning msg.

	return 0;
}


static ssize_t DRVR_read (struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int err;
 	struct DRVRdev_struct *local=file->private_data;
 	printk(KERN_INFO "DRVR_read ( count:%d ppos:%d counter:%d )\n", count, *ppos, local->counter);


	if( local->counter <= 0 ) {
		printk(KERN_INFO "read: counter:%d ", local->counter);
		return 0;
	}

	if( *ppos >= local->counter) {
		printk(KERN_INFO "read return 0: counter:%d < ppos:%d", local->counter, *ppos);
		return 0;
	} else if( *ppos + count >= local->counter) {
		printk(KERN_INFO "read advance:  *ppos + count:%d >= local->counter:%d", *ppos + count, *ppos);
		count = local->counter - *ppos;
	}

	if( count <= 0 ) {
		printk(KERN_INFO "read problem");
		return 0;
	}

	memset(buf,0,64);

	err = copy_to_user(buf,  &(local->storage[*ppos]) ,   count);

	if (err != 0) {
		printk(KERN_INFO "read problem");
		return -EFAULT;
	}

	buf[count]=0;
	*ppos += count;
	printk(KERN_INFO "DRVR_read return: count: %d, ppos :%d\n", count, *ppos);
	return count;
}

static ssize_t DRVR_write (struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int err;
	struct DRVRdev_struct *local = file->private_data;

	err = copy_from_user( &(local->storage[*ppos])  ,buf,count);
	if (err != 0)
		return -EFAULT;

	int i = 0;
	while(local->storage[i] != '\0' && i < count) {
		printk(KERN_INFO "-- %d:%c \n\t\t:%d,%d\n",i, (const char) local->storage[i], count, *ppos );


		if( head == NULL) {
			head = tail = create_lnode(refcount, (const char) local->storage[i]);
		} else {
			tail = tail->next =  create_lnode(refcount, (const char) local->storage[i]);
		}

		++i;
	}

	local->counter += count;
	*ppos+=count;
	return count;
}


loff_t DRVR_llseek( struct file *file, loff_t pos, int whence )
{
	loff_t	newpos = -1;
	struct DRVRdev_struct *local = file->private_data;

	switch ( whence )
	{
		case 0:	newpos = pos; break;   				// SEEK_SET
		case 1:	newpos = file->f_pos + pos; break; 	// SEEK_CUR
		case 2: newpos = local->counter + pos; break; 		// SEEK_END
	}

	if (( newpos < 0 )||( newpos > local->counter ))
		return -EINVAL;

	file->f_pos = newpos;
	return	newpos;
}


static struct file_operations DRVR_fops =
{
	owner:	THIS_MODULE, 	// struct module *owner
	open:	DRVR_open, 	// open method
	read:   DRVR_read,	// read method
	write:  DRVR_write, 	// write method
	llseek:  DRVR_llseek,  // covered in detail in Ch6. Just for fwd ref now.
	release:  DRVR_release 	// release method
};

static struct class *DRVR_class;

static struct DRVRdev {
	const char *name;
	umode_t mode;
	const struct file_operations *fops;
} DRVRdevlist[] =  {
	[0] = { "refcount",0666, &DRVR_fops },
};

static char *DRVR_devnode(struct device *dev, umode_t *mode)
{
	if (mode && DRVRdevlist[MINOR(dev->devt)].mode)
		*mode = DRVRdevlist[MINOR(dev->devt)].mode;

	return NULL;
}


static int PROC_show(struct seq_file *m, void *v)
{

	seq_printf(m, "\nVmalloced memory\n" );

	lnode tmp = head;
	while(tmp != NULL) {
		seq_printf(m, "%c", tmp->c);
		tmp = tmp->next;
	}

	seq_printf(m, "\nReleasing memory references\n" );

	int i = 0;
	for(i=0; i<refcount; i++) {
		seq_printf(m, "%i %llu %c\n", i, (unsigned int) reference_buffer[i], reference_buffer[i]->c);
		vfree( reference_buffer[i]);
	}

	seq_printf(m, "\nMemory references freed\n" );

	refcount = 0;
	head = tail = NULL;

	return 0;
}

static int PROC_open(struct inode *inode, struct file *file)
{
     return single_open(file, PROC_show, NULL);
}

static const struct file_operations PROC_fops = {
     .owner	= THIS_MODULE,
     .open	= PROC_open,
     .read	= seq_read,
     .llseek	= seq_lseek,
     .release	= single_release,
};



static int DRVR_init(void)
{
	int i;
	struct DRVRdev_struct *local=&myDRVR;

	local->storage = vmalloc(4096);

	DRVRmajor = DRVRparm;

	if (DRVRmajor)
	{
		//  Step 1a of 2:  create/populate device numbers
 		firstdevno = MKDEV(DRVRmajor, DRVRMINOR);

		//  Step 1b of 2:  request/reserve Major Number from Kernel
 		i = register_chrdev_region(firstdevno,1,DRVR);

		if (i < 0)
		{
			printk(KERN_ALERT "Error (%d) adding DRVR", i);
			return i;
		}

	} else {
		//  Step 1c of 2:  Request a Major Number Dynamically.
		i = alloc_chrdev_region(&firstdevno, DRVRMINOR, DRVRNUMDEVS, DRVR);

		if (i < 0)
		{
			printk(KERN_ALERT "Error (%d) adding DRVR", i);
			return i;
		}

		DRVRmajor = MAJOR(firstdevno);
		printk(KERN_ALERT "kernel assigned major number: %d to DRVR\n",DRVRmajor);
	}


	DRVR_class = class_create(THIS_MODULE, "refcount");

	if (IS_ERR(DRVR_class))
		return PTR_ERR(DRVR_class);

	DRVR_class->devnode = DRVR_devnode;
	device_create(DRVR_class, NULL, MKDEV(DRVRmajor, 0), NULL, DRVR);

	//  Step 2a of 2:  initialize local->cdev struct
 	cdev_init(&local->cdev, &DRVR_fops);

 	//  Step 2b of 2:  register device with kernel
 	local->cdev.owner = THIS_MODULE;
 	local->cdev.ops = &DRVR_fops;
 	i = cdev_add(&local->cdev, firstdevno, DRVRNUMDEVS);

 	if (i)
 	{
 		printk(KERN_ALERT "Error (%d) adding DRVR", i);
 		return i;
 	}


	proc = proc_create( "refcount",  0777, NULL, &PROC_fops );
	if (!proc) {
		return -ENOMEM;
	}


 	return 0;
}

static void DRVR_exit(void)
{
	struct DRVRdev_struct *local=&myDRVR;

	vfree(local->storage);

 	cdev_del(&local->cdev);

 	device_destroy(DRVR_class, MKDEV(DRVRmajor, 0));
 	class_destroy(DRVR_class);

 	unregister_chrdev_region(firstdevno, DRVRNUMDEVS);

	if (DRVRmajor != DRVRMAJOR) {
		printk(KERN_ALERT "kernel unassigned major number: %d from DRVR\n", DRVRmajor);
	}

	remove_proc_entry( "refcount", NULL );
}

module_init(DRVR_init);
module_exit(DRVR_exit);
