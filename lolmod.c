#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/random.h>

struct miscdevice ldev;

struct lolinfo {
	int c;/* wWe */
	int len;
};

static int lol_open(struct inode *inode, struct file *filp)
{
	struct lolinfo *l;

	l = kmalloc(sizeof (*l), GFP_KERNEL);
	filp->private_data = l;
	l->c = 'w';
	l->len = 0;

	return 0;
}


static int lol_release(struct inode *inode, struct file *filp)
{
	struct lolinfo *l = filp->private_data;

	filp->private_data = NULL;
	wmb();
	kfree(l);

	return 0;
}


static ssize_t lol_read(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	struct lolinfo *l = filp->private_data;
	char *p;
	char *q;
	unsigned char n;
	size_t req = count;
	size_t m;
	int r;	

	if (l == NULL) {
		return -EBADFD;
	}

	q = p = kmalloc(count, GFP_KERNEL);
	while (req) {
		if (l->len == 0) {
			get_random_bytes(&n, 1);
			switch (n & 3) {
			case 0:
				l->c = 'e';
				l->len = 1;
				break;
			case 1:
				l->c = 'W';
				l->len = 1;
				break;
			default:
				l->c = 'w';
				l->len = n >> 2;
				break;
			}
		}
		m = req < l->len ? req : l->len;
		memset(q, l->c, m);
		l->len -= m;
		req -= m;
		q += m;
	}

	r = copy_to_user(buff, p, count);
	kfree(p);
	if (r) {
		return r;
	}
	*pos += count;

	return count;
}


static struct file_operations lol_fops = {
	.owner		= THIS_MODULE,
	.open		= lol_open,
	.release	= lol_release,
	.read		= lol_read,
};


int __init lol_init(void)
{
	ldev.name = "lol";
	ldev.minor = MISC_DYNAMIC_MINOR;
	ldev.fops = &lol_fops;

	return misc_register(&ldev);
}


static void lol_exit(void)
{
	misc_deregister(&ldev);
}

module_init(lol_init)
module_exit(lol_exit)

