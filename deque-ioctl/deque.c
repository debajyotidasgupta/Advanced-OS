#include "deque.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/version.h>

MODULE_AUTHOR("Shubham Mishra (18CS10066)");
MODULE_LICENSE("GPL");

/* Deque data structure implementation
 * We use a hashtable mapping pid to individual Deque struct.
 * The Deque uses a circular queue implemented using an array
 */
struct Deque {
    int *q;
    int front;
    int end;
    int N;
    
    pid_t pid;
    int is_open;
    struct hlist_node node;
};

// 2 ^ 16 processes can access this LKM at the same time.
DECLARE_HASHTABLE(deque_tbl, 16);

// Global Lock on the hashtable for Open, close, read, write and ioctl
DEFINE_MUTEX(lck);

// Get the deque for the current user process
// May return NULL if deque doesn't exist yet.
struct Deque * get_deque(void)
{
    pid_t pid = task_pid_nr(current);
    struct Deque *curr;
    struct Deque *deque = NULL;

    hash_for_each_possible(deque_tbl, curr, node, pid){
        deque = curr;
        break;
    }

    return deque;
}

inline void deque_print(struct Deque *d)
{
    printk(KERN_DEBUG "[ ");
    int i = d->front;
    if (d->front != -1){
        do {
            printk(KERN_DEBUG "%d ", d->q[i]);
            if (i == d->end) break;
            
            i = (i + 1) % d->N;

        } while (1);
    }
    printk(KERN_DEBUG "]\n");
}

inline int deque_isfull(struct Deque *d)
{
    if (d->front == (d->end+1) % d->N){
        return 1;
    }
    return 0;
}

inline int deque_isempty(struct Deque *d)
{
    if (d->front == -1 && d->end == -1)
        return 1;

    return 0;
}

inline int deque_len(struct Deque *d)
{
    if (deque_isempty(d)){
        return 0;
    }

    if (deque_isfull(d)){
        return d->N;
    }

    if (d->front <= d->end){
        return (d->end - d->front + 1);
    }

    int gap = d->front - d->end - 1;
    return d->N - gap;
}
// Append at the end of a given deque
int deque_pushback(struct Deque *d, int val)
{
    if (deque_isfull(d)){
        return -1;
    }

    if (deque_isempty(d)){
        d->front = d->end = 0;
    }else{
        d->end = (d->end + 1) % d->N;
    }

    d->q[d->end] = val;
    return 0;
}

// Add at the beginning of a given deque
int deque_pushfront(struct Deque *d, int val)
{
    if (deque_isfull(d)){
        return -1;
    }

    if (deque_isempty(d)){
        d->front = d->end = 0;
    }else{
        d->front = (d->front - 1 + d->N) % d->N;
    }

    d->q[d->front] = val;
    
    return 0;
}

// Dequeues from the front.
int deque_popfront(struct Deque *d, int *ret)
{
    if (deque_isempty(d)){
        return -1;
    }

    *ret = d->q[d->front];

    if (d->front == d->end){
        d->front = d->end = -1;
    }else{
        d->front = (d->front + 1) % d->N;
    }

    return 0;
}

// Dequeues from the back.
int deque_popback(struct Deque *d, int *ret)
{
    if (deque_isempty(d)){
        return -1;
    }

    *ret = d->q[d->end];

    if (d->front == d->end){
        d->front = d->end = -1;
    }else{
        d->end = (d->end - 1 + d->N) % d->N;
    }

    return 0;
}


/* ProcFS function implementation */
static struct proc_dir_entry *proc_file;

// Open system call
int procfs_open(struct inode *inode, struct file *file)
{
    int ret = 0;
    mutex_lock(&lck);
    struct Deque *d = get_deque();
    if (d == NULL){
        // New process.
        d = kmalloc(sizeof(struct Deque), GFP_KERNEL);
        if (d == NULL){
            ret = -ENOMEM;
            goto OPEN_RETURN;
        }

        d->q = NULL;
        
        d->pid = task_pid_nr(current);

        hash_add(deque_tbl, &(d->node), d->pid);
    } else if (d->is_open){
        // Case of double opening
        ret = -EACCES;
        goto OPEN_RETURN;
    }
    
    d->is_open = 1;
    d->front = -1;
    d->end = -1;
    d->N = 0;

OPEN_RETURN:
    mutex_unlock(&lck);
    return ret;   
}

// Close system call
int procfs_close(struct inode *inode, struct file *file)
{
    int ret = 0;

    mutex_lock(&lck);
    struct Deque *d = get_deque();
    if (d == NULL){
        ret = -EINVAL;     // Never opened
        goto CLOSE_RETURN;
    }

    if (d->is_open){
        d->is_open = 0;
    }else{
        ret = -EACCES;      // Double close
        goto CLOSE_RETURN;
    }

    if (d->q != NULL){
        kfree(d->q);
        d->q = NULL;
    }

CLOSE_RETURN:
    mutex_unlock(&lck);
    return ret;
}

// Dummy Read system call
// Only returns errors
static ssize_t procfs_read(struct file *f, char *buffer, size_t length, loff_t *offset)
{
    // return -ENOEXEC;

    int ret = 0;

    if (length != sizeof(int)){
        ret = -EFAULT;
        goto READ_RETURN;
    }

    mutex_lock(&lck);
    struct Deque *deque = get_deque();
    if (deque == NULL){
        ret = -EACCES;
        goto READ_RETURN;
    }

    int val = -100;
    if (deque_popfront(deque, &val)){
        ret = -EACCES;
        goto READ_RETURN;
    }
    
    if (!access_ok(buffer, sizeof(int)) || raw_copy_to_user(buffer, &val, sizeof(int))){
        ret = -EFAULT;
        deque_pushfront(deque, val);
        goto READ_RETURN;
    }

    ret = sizeof(int);
    
READ_RETURN:
    mutex_unlock(&lck);
    printk(KERN_DEBUG "read RET %d", ret);
    return ret;
}

// Dummy Write system call
// Only returns errors
static ssize_t procfs_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
    int ret = 0;

    mutex_lock(&lck);
    struct Deque *deque = get_deque();
    if (deque == NULL){
        ret = -EACCES;
        goto WRITE_RETURN;
    }

    int val;

    if (deque->N == 0){
        // Uninitialised deque
        ret = -EACCES;
        goto WRITE_RETURN;
    }

    if (len != sizeof(int)){
        ret = -EFAULT;
        goto WRITE_RETURN;
    }

    if (!access_ok(buffer, sizeof(int)) || raw_copy_from_user(&val, buffer, sizeof(int))){
        ret = -EFAULT;
        goto WRITE_RETURN;
    }


    if (val % 2 == 0){
        if (deque_pushback(deque, val)){
            printk(KERN_DEBUG "Could not pushback %d\n", val);
            ret = -EACCES;
            goto WRITE_RETURN;
        }
    }else{
        if (deque_pushfront(deque, val)){
            printk(KERN_DEBUG "Could not pushfront %d\n", val);
            ret = -EACCES;
            goto WRITE_RETURN;
        }
    }

    printk(KERN_DEBUG "Write successful %d\n", val);
    ret = sizeof(int);
    
WRITE_RETURN:
    mutex_unlock(&lck);
    return ret;
}

/* Each function below belongs to the ioctl class
 * Each one assumes the lock is already held
 */

#define GET_DEQUE struct Deque *deque = get_deque();\
    if (deque == NULL){\
        return -EACCES;\
    }

int set_capacity(int N)
{
    GET_DEQUE

    if (N > 100 || N < 0){
        return -EINVAL;
    }

    if (deque->q != NULL){
        kfree(deque->q);
        deque->q = NULL;
    }

    deque->q = kmalloc(N * sizeof(int), GFP_KERNEL);
    if (deque->q == NULL){
        return -ENOMEM;
    }

    deque->N = N;
    deque->front = -1;
    deque->end = -1;

    printk(KERN_DEBUG "Deque init with %d elements\n", N);
    return 0;
}

int insert_right(int N)
{
    GET_DEQUE

    if (deque_pushback(deque, N)){
        printk(KERN_DEBUG "Could not pushback %d\n", N);
        return -EACCES;
    }

    printk(KERN_DEBUG "Insert Right successful: %d\n", N);
    return 0;
}

int insert_left(int N)
{
    GET_DEQUE

    if (deque_pushfront(deque, N)){
        printk(KERN_DEBUG "Could not pushfront %d\n", N);
        return -EACCES;
    }

    printk(KERN_DEBUG "Insert Left successful: %d\n", N);
    return 0;
}

int get_info(struct obj_info *info){
    GET_DEQUE

    info->capacity = deque->N;
    info->deque_size = deque_len(deque);

    return 0;
}

int extract_left(int *N)
{
    GET_DEQUE

    if (deque_popfront(deque, N)){
        return -EACCES;
    }

    return 0;
}

int extract_right(int *N)
{
    GET_DEQUE

    if (deque_popback(deque, N)){
        return -EACCES;
    }

    return 0;
}

long procfs_ioctl(struct file * f, unsigned int cmd, unsigned long arg){
    int ret = 0;
    mutex_lock(&lck);

    int N;
    struct obj_info info;

    switch (cmd) {
   
    case PB2_SET_CAPACITY:
        if (!access_ok((int *)arg, sizeof(int)) || raw_copy_from_user(&N, (int *)arg, sizeof(int))){
            ret = -EINVAL;
        }else{
            ret = set_capacity(N);
        }
        break;
    
    case PB2_INSERT_RIGHT:
        if (!access_ok((int *)arg, sizeof(int)) || raw_copy_from_user(&N, (int *)arg, sizeof(int))){
            ret = -EINVAL;
        }else{
            ret = insert_right(N);
        }
        break;

    case PB2_INSERT_LEFT:
        if (!access_ok((int *)arg, sizeof(int)) || raw_copy_from_user(&N, (int *)arg, sizeof(int))){
            ret = -EINVAL;
        }else{
            ret = insert_left(N);
        }
        break;

    case PB2_GET_INFO:
        get_info(&info);
        if (!access_ok((struct obj_info *)arg, sizeof(struct obj_info))
            || raw_copy_to_user((struct obj_info *)arg, &info, sizeof(struct obj_info))){
            ret = -EACCES;
        }else{
            ret = 0;
        }
        break;

    case PB2_EXTRACT_LEFT:
        ret = extract_left(&N);
        if (ret != 0)
            break;
        if (!access_ok((int *)arg, sizeof(int))
            || raw_copy_to_user((int *)arg, &N, sizeof(int))){
            ret = -EACCES;
            insert_left(N);
            break;
        }
        break;
    case PB2_EXTRACT_RIGHT:
        ret = extract_right(&N);
        if (ret != 0)
            break;
        if (!access_ok((int *)arg, sizeof(int))
            || raw_copy_to_user((int *)arg, &N, sizeof(int))){
            ret = -EACCES;
            insert_right(N);
            break;
        }
        break;
    default:
        ret = -ENOIOCTLCMD;
        break;
    }

    mutex_unlock(&lck);
    return ret;


}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)
static struct file_operations fops = {
    .read = procfs_read,
    .write = procfs_write,
    .open = procfs_open,
    .release = procfs_close,
    .unlocked_ioctl = procfs_ioctl,

    .owner = THIS_MODULE,
};
#else
static struct proc_ops fops = {
  .proc_open = procfs_open,
  .proc_read = procfs_read,
  .proc_write = procfs_write,
  .proc_release = procfs_close,
  .proc_ioctl = procfs_ioctl
};
#endif



int init_module()
{
    proc_file = proc_create(PROC_FILENAME, 0666, NULL, &fops);
    
    if (proc_file == NULL){
        printk(KERN_ALERT "Problem initialising file.\n");
        return -ENOMEM;
    }

    printk(KERN_INFO "%s is created.\n", PROC_FILENAME);

    return 0;
}

void cleanup_module()
{
    unsigned bkt;
    struct Deque *d;
    hash_for_each(deque_tbl, bkt, d, node){
        if (d->q != NULL){
            kfree(d->q);
        }
        hash_del(&(d->node));
    }

    remove_proc_entry(PROC_FILENAME, NULL);
    printk(KERN_INFO "%s removed.\n", PROC_FILENAME);
}