
/*
Assignment 2
------------------------------------------
subject:        Advanced Operating Systems
year:           2021 - 2022
------------------------------------------
Name:           Debajyoti Dasgupta
Roll No:        18CS30051
------------------------------------------
Kernel Version: 5.11.0-37-generic
System:         Ubuntu 20.04.3 LTS
*/

#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_ALERT */
#include <linux/init.h>      /* Needed for the macros */
#include <linux/proc_fs.h>   /* Needed for the proc filesystem */
#include <linux/uaccess.h>   /* Needed for copy_from_user */
#include <linux/mutex.h>     /* Needed for mutex */
#include <linux/types.h>     /* Needed for size_t */
#include <linux/slab.h>      /* Needed for kmalloc */
#include <linux/string.h>    /* Needed for strlen */
#include <linux/sched.h>     /* Needed for current */
#include <linux/hashtable.h> /* Needed for hashtable */
#include <linux/version.h>   /* Needed for KERNEL_VERSION */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debajyoti Dasgupta");
MODULE_DESCRIPTION("LKM for Deque");
MODULE_VERSION("0.1");

#define PROC_FILENAME "cs60038_a2_18CS30051"
static struct proc_ops file_ops;

#define PB2_SET_CAPACITY _IOW(0x10, 0x31, int32_t *)
#define PB2_INSERT_RIGHT _IOW(0x10, 0x32, int32_t *)
#define PB2_INSERT_LEFT _IOW(0x10, 0x33, int32_t *)
#define PB2_GET_INFO _IOR(0x10, 0x34, int32_t *)
#define PB2_EXTRACT_LEFT _IOR(0x10, 0x35, int32_t *)
#define PB2_EXTRACT_RIGHT _IOR(0x10, 0x36, int32_t *)

DECLARE_HASHTABLE(hashtable_deque, 10);
DEFINE_MUTEX(global_lock);

struct ListNode
{                          // Structure for node of the linked list
    int data;              // Data to be stored in the deque
    struct ListNode *prev; // Pointer to the previous node
    struct ListNode *next; // Pointer to the next node
};

struct Deque
{

    pid_t pid;                  // Process ID of the process that created the deque
    int is_open;                // Flag to check if the deque is open
    int size;                   // Current size of the deque
    int max_size;               // Maximum size of the deque
    struct ListNode *front_ptr; // Pointer to the front of the deque
    struct ListNode *rear_ptr;  // Pointer to the rear of the deque
    struct hlist_node node;     // Node for the hashtable
};

struct obj_info
{
    int32_t deque_size; // current number of elements in deque
    int32_t capacity;   // maximum capacity of the deque
};

struct ListNode *create_node(int);    // Creates a new node
struct Deque *create_deque(int);      // Creates a new deque
struct Deque *get_proc_deque(void);   // Returns the deque of the current process
bool full(struct Deque *);            // Checks if the deque is full
bool empty(struct Deque *);           // Checks if the deque is empty
int push_back(struct Deque *, int);   // Pushes an element to the back of the deque
int push_front(struct Deque *, int);  // Pushes an element to the front of the deque
int pop_front(struct Deque *, int *); // Pops an element from the front of the deque
int pop_back(struct Deque *, int *);  // Pops an element from the back of the deque

int file_open(struct inode *, struct file *);                             // Opens the file
int file_close(struct inode *, struct file *);                            // Closes the file
static ssize_t file_read(struct file *, char *, size_t, loff_t *);        // Reads from the file
static ssize_t file_write(struct file *, const char *, size_t, loff_t *); // Writes to the fileq
long file_ioctl(struct file *, unsigned int, unsigned long);              // Handles IOCTL calls

static int lkm_init(void)
{
    struct proc_dir_entry *entry = proc_create(PROC_FILENAME, 0666, NULL, &file_ops);
    if (entry == NULL)
        return -ENOENT;

    /* Initialize the function pointers of file_ops */
    file_ops.proc_open = file_open;
    file_ops.proc_release = file_close;
    file_ops.proc_read = file_read;
    file_ops.proc_write = file_write;
    file_ops.proc_ioctl = file_ioctl;

    printk(KERN_ALERT "[%d] Deque LKM Inserted Successfully\n", current->pid);
    return 0;
}

static void lkm_exit(void)
{
    unsigned bucket;
    struct Deque *iter;
    struct ListNode *temp;

    hash_for_each(hashtable_deque, bucket, iter, node)
    {
        while (iter->front_ptr)
        {
            if (iter->front_ptr->next)
            {
                temp = iter->front_ptr->next;
                kfree(iter->front_ptr);
                iter->front_ptr = temp;
            }
            else
            {
                kfree(iter->front_ptr);
                iter->front_ptr = NULL;
            }
        }
        hash_del(&(iter->node));
    }

    remove_proc_entry(PROC_FILENAME, NULL);
    printk(KERN_ALERT "[%d] Deque LKM Removed Successfully\n", current->pid);
}

module_init(lkm_init);
module_exit(lkm_exit);

// ------- END OF LOADABLE KERNEL MODULE CODE -------

/**
 * Function Definitions
 * --------------------
 */

struct ListNode *create_node(int data)
{
    struct ListNode *new_node = (struct ListNode *)kmalloc(sizeof(struct ListNode), GFP_KERNEL);
    if (new_node == NULL)
    {
        printk(KERN_ALERT "[%d] Cannot create node\n", current->pid);
        return NULL;
    }
    new_node->data = data;
    new_node->prev = NULL;
    new_node->next = NULL;
    return new_node;
}

struct Deque *get_proc_deque(void)
{
    pid_t pid = current->pid;
    struct Deque *iter;

    hash_for_each_possible(hashtable_deque, iter, node, pid) return iter;
    return NULL;
}

bool full(struct Deque *deque)
{
    return deque->size == deque->max_size;
}

bool empty(struct Deque *deque)
{
    return deque->size == 0;
}

int push_back(struct Deque *deque, int data)
{
    struct ListNode *new_node;
    if (full(deque))
        return -1;

    new_node = create_node(data);
    if (new_node == NULL)
    {
        printk(KERN_ALERT "[%d] Cannot create node\n", current->pid);
        return -1;
    }

    printk(KERN_INFO "[%d] New node created\n", current->pid);
    if (empty(deque))
    {
        deque->front_ptr = new_node;
        deque->rear_ptr = new_node;
    }
    else
    {
        deque->rear_ptr->next = new_node;
        new_node->prev = deque->rear_ptr;
        deque->rear_ptr = new_node;
    }
    deque->size++;
    printk(KERN_ALERT "[%d] Pushed %d to the back of the deque\n", current->pid, data);
    return 0;
}

int push_front(struct Deque *deque, int data)
{
    struct ListNode *new_node;

    if (full(deque))
        return -1;

    new_node = create_node(data);

    if (new_node == NULL)
        return -1;

    printk(KERN_INFO "[%d] New node created\n", current->pid);
    if (empty(deque))
    {
        printk(KERN_INFO "[%d] Pushing in empty queue\n", current->pid);
        deque->front_ptr = new_node;
        deque->rear_ptr = new_node;
    }
    else
    {
        new_node->next = deque->front_ptr;
        deque->front_ptr->prev = new_node;
        deque->front_ptr = new_node;
    }
    deque->size++;

    printk(KERN_ALERT "[%d] Pushed %d to the front of the deque\n", current->pid, data);
    return 0;
}

int pop_front(struct Deque *deque, int *retval)
{
    struct ListNode *temp;

    if (empty(deque))
        return -1;

    *retval = deque->front_ptr->data;
    temp = deque->front_ptr;
    deque->front_ptr = deque->front_ptr->next;

    printk(KERN_INFO "returning %d", *retval);

    if (deque->front_ptr != NULL)
        deque->front_ptr->prev = NULL;
    deque->size--;
    kfree(temp);

    printk(KERN_ALERT "[%d] Popped %d from the front of the deque\n", current->pid, *retval);
    return 0;
}

int pop_back(struct Deque *deque, int *retval)
{
    struct ListNode *temp;

    if (empty(deque))
        return -1;

    printk(KERN_ALERT "popping out size: %d", deque->size);

    *retval = deque->rear_ptr->data;
    temp = deque->rear_ptr;
    deque->rear_ptr = deque->rear_ptr->prev;
    if (deque->rear_ptr != NULL)
        deque->rear_ptr->next = NULL;
    deque->size--;
    kfree(temp);

    printk(KERN_ALERT "[%d] Popped %d from the back of the deque\n", current->pid, *retval);
    return 0;
}

struct Deque *create_deque(int max_size)
{
    struct Deque *new_deque = (struct Deque *)kmalloc(sizeof(struct Deque), GFP_KERNEL);
    if (new_deque == NULL)
    {
        printk(KERN_ALERT "[%d] Cannot create deque\n", current->pid);
        return NULL;
    }
    new_deque->pid = current->pid;
    new_deque->max_size = max_size;
    new_deque->front_ptr = NULL;
    new_deque->rear_ptr = NULL;
    new_deque->size = 0;
    new_deque->is_open = 1;
    return new_deque;
}

int file_open(struct inode *inode, struct file *file)
{
    int retval = 0;
    struct Deque *deque;

    mutex_lock(&global_lock);
    deque = get_proc_deque();

    if (deque == NULL)
    {
        printk(KERN_DEBUG "[%d] Deque does not exist\n", current->pid);
        deque = create_deque(0);

        if (deque == NULL)
        {
            printk(KERN_ALERT "[%d] Cannot create deque\n", current->pid);
            retval = -ENOMEM;
            goto out;
        }

        hash_add(hashtable_deque, &(deque->node), deque->pid);
        printk(KERN_DEBUG "[%d] Created and inserted deque\n", current->pid);
    }
    else if (deque->is_open)
    {
        retval = -EACCES;
        goto out;
    }
out:
    mutex_unlock(&global_lock);
    return retval;
}

int file_close(struct inode *inode, struct file *file)
{
    int retval = 0;
    struct Deque *deque;
    struct ListNode *temp;

    mutex_lock(&global_lock);

    deque = get_proc_deque();
    if (deque == NULL)
    {
        retval = -EINVAL;
        goto out;
    }

    if (deque->is_open)
        deque->is_open = 0;
    else
    {
        retval = -EACCES;
        goto out;
    }

    while (deque->front_ptr)
    {
        temp = deque->front_ptr;
        deque->front_ptr = deque->front_ptr->next;
        kfree(temp);
    }

out:
    mutex_unlock(&global_lock);
    return retval;
}

static ssize_t file_read(struct file *f, char *buffer, size_t length, loff_t *offset)
{
    int retval = 0;
    int data;
    struct Deque *deque;

    if (length != sizeof(int))
    {
        retval = -EFAULT;
        goto out;
    }

    mutex_lock(&global_lock);

    deque = get_proc_deque();
    if (deque == NULL)
    {
        retval = -EACCES;
        goto out;
    }

    if (pop_front(deque, &data))
    {
        retval = -EACCES;
        goto out;
    }

    if (!access_ok(buffer, sizeof(int)) || raw_copy_to_user(buffer, &data, sizeof(int)))
    {
        retval = -EFAULT;
        push_front(deque, data);
        goto out;
    }

    retval = sizeof(int);

out:
    mutex_unlock(&global_lock);
    return retval;
}

static ssize_t file_write(struct file *f, const char *buffer, size_t buf, loff_t *offset)
{
    int retval = 0;
    int data;
    struct Deque *deque;

    mutex_lock(&global_lock);
    deque = get_proc_deque();

    if (deque == NULL)
        retval = -EACCES;
    else if (empty(deque))
        retval = -EACCES;
    else if (buf != sizeof(int))
        retval = -EFAULT;
    else if (!access_ok(buffer, sizeof(int)) || copy_from_user(&data, buffer, sizeof(int)))
        retval = -EFAULT;
    else
    {
        if (!(data & 1))
        {
            retval = push_back(deque, data);
            if (retval)
                retval = -EACCES;
            else
                retval = sizeof(int);
        }
        else
        {
            retval = push_front(deque, data);
            if (retval)
                retval = -EACCES;
            else
                retval = sizeof(int);
        }
    }

    mutex_unlock(&global_lock);
    return retval;
}

long file_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    int err, data;
    struct ListNode *temp;
    struct Deque *deque;

    struct obj_info info;
    mutex_lock(&global_lock);

    err = 0;
    switch (cmd)
    {
    case PB2_SET_CAPACITY:
        if (!access_ok((int *)arg, sizeof(int)) || copy_from_user(&data, (int *)arg, sizeof(int)))
        {
            err = -EINVAL;
            goto out;
        }
        if (data > 100 || data < 0)
        {
            err = -EINVAL;
            goto out;
        }

        deque = get_proc_deque();
        if (deque == NULL)
        {
            err = -EACCES;
            goto out;
        }

        while (deque->front_ptr)
        {
            temp = deque->front_ptr;
            deque->front_ptr = deque->front_ptr->next;
            kfree(temp);
        }
        deque->max_size = data;
        break;

    case PB2_INSERT_RIGHT:
        if (!access_ok((int *)arg, sizeof(int)) || copy_from_user(&data, (int *)arg, sizeof(int)))
        {
            err = -EINVAL;
            goto out;
        }

        deque = get_proc_deque();
        if (deque == NULL)
        {
            err = -EACCES;
            goto out;
        }
        if (!push_back(deque, data))
            err = -EACCES;
        break;

    case PB2_INSERT_LEFT:
        if (!access_ok((int *)arg, sizeof(int)) || copy_from_user(&data, (int *)arg, sizeof(int)))
        {
            err = -EINVAL;
            goto out;
        }

        deque = get_proc_deque();
        if (deque == NULL)
        {
            err = -EACCES;
            goto out;
        }
        printk(KERN_DEBUG "Inserting %d in deque for pid [%d]", data, deque->pid);
        if (!push_front(deque, data))
            err = -EACCES;
        printk(KERN_DEBUG "Inserted %d in deque for pid [%d]", data, deque->pid);
        break;

    case PB2_GET_INFO:
        deque = get_proc_deque();
        if (deque == NULL)
        {
            err = -EACCES;
            goto out;
        }

        info.deque_size = deque->size;
        info.capacity = deque->max_size;

        if (!access_ok((struct obj_info *)arg, sizeof(struct obj_info)) || raw_copy_to_user((struct obj_info *)arg, &info, sizeof(struct obj_info)))
            err = -EACCES;
        break;

    case PB2_EXTRACT_LEFT:
        deque = get_proc_deque();
        if (deque == NULL)
        {
            err = -EACCES;
            goto out;
        }
        err = pop_front(deque, &data);
        if (!err)
            if (!access_ok((int *)arg, sizeof(int)) || raw_copy_to_user((int *)arg, &data, sizeof(int)))
            {
                err = -EACCES;
                push_front(deque, data);
                goto out;
            }
        break;
    case PB2_EXTRACT_RIGHT:
        deque = get_proc_deque();
        if (deque == NULL)
        {
            err = -EACCES;
            goto out;
        }
        err = pop_back(deque, &data);
        if (!err)
            if (!access_ok((int *)arg, sizeof(int)) || raw_copy_to_user((int *)arg, &data, sizeof(int)))
            {
                err = -EACCES;
                push_back(deque, data);
                goto out;
            }
        break;
    default:
        err = -EINVAL;
    }

out:
    mutex_unlock(&global_lock);
    return err;
}
