#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/current.h>

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


static ssize_t lol_fill(struct lolinfo *l, char *buff, size_t req)
{
	unsigned char n;
	ssize_t m;

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
		memset(buff, l->c, m);
		l->len -= m;
		req -= m;
		buff += m;
	}

	return 0;
}


static ssize_t lol_fill_user_pages(struct lolinfo *l, char __user *buff, size_t count)
{
	struct page **page_list;
	struct vm_area_struct **vma_list;
	long int uaddr;
	unsigned long int offset;
	unsigned long int nr_pages;
	ssize_t ret = 0;
	int pinned;
	int i;
	char *p;

	uaddr = (unsigned long int)buff & PAGE_MASK;
	offset = (unsigned long int)buff & ~PAGE_MASK;
	nr_pages = PAGE_ALIGN(count + offset) >> PAGE_SHIFT;

	page_list = (struct page **) __get_free_page(GFP_KERNEL);
	if (page_list == NULL) {
		return -ENOMEM;
	}

	vma_list = (struct vm_area_struct **) __get_free_page(GFP_KERNEL);

	down_write(&current->mm->mmap_sem);
	while (nr_pages) {
		pinned = get_user_pages(current, current->mm, uaddr,
					min_t(unsigned long, nr_pages, PAGE_SIZE / sizeof (struct page *)),
					1, 0,
					page_list, vma_list);
		up_write(&current->mm->mmap_sem);
		if (pinned <= 0) {
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < pinned; ++i) {
			ssize_t len = min_t(ssize_t, (PAGE_SIZE - offset), count);
			p = kmap(page_list[i]);
			lol_fill(l, p + offset, len);
			kunmap(page_list[i]);
			offset = 0;
			count -= len;
			ret += len;
		}
		nr_pages -= pinned;

		down_write(&current->mm->mmap_sem);
		for (i = 0; i < pinned; ++i) {
			set_page_dirty_lock(page_list[i]);
			put_page(page_list[i]);
		}

		uaddr += pinned * PAGE_SIZE;
	}
	up_write(&current->mm->mmap_sem);

out:
	if (vma_list) {
		free_page((unsigned long) vma_list);
	}
	free_page((unsigned long) page_list);

	return ret;
}

static ssize_t lol_read(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	struct lolinfo *l = filp->private_data;
	int r;

	if (l == NULL) {
		return -EBADFD;
	}

	r = lol_fill_user_pages(l, buff, count);

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

module_init(lol_init);
module_exit(lol_exit);
MODULE_LICENSE("GPL");
