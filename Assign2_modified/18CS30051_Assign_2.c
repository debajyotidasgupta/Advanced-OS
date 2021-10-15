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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debajyoti Dasgupta");
MODULE_DESCRIPTION("LKM for Deque");
MODULE_VERSION("0.1");

#define PROC_FILENAME "cs60038_a2_18CS30051"
#define PROCESS_LIMIT 100000
#define BUFF_SIZE 256

#define PB2_SET_CAPACITY _IOW(0x10, 0x31, int32_t *)
#define PB2_INSERT_RIGHT _IOW(0x10, 0x32, int32_t *)
#define PB2_INSERT_LEFT _IOW(0x10, 0x33, int32_t *)
#define PB2_GET_INFO _IOR(0x10, 0x34, int32_t *)
#define PB2_EXTRACT_RIGHT _IOR(0x10, 0x36, int32_t *)
#define PB2_EXTRACT_LEFT _IOR(0x10, 0x35, int32_t *)

DEFINE_MUTEX(mutex);

struct Deque
{
    int32_t *data;
    int32_t capacity;
    int32_t size;
    int32_t front;
    int32_t rear;
};

struct obj_info
{
    int32_t deque_size; // current number of elements in deque
    int32_t capacity;   // maximum capacity of the deque
};

struct process
{
    int pid;
    struct Deque *deque;
};

struct process *process_list[PROCESS_LIMIT];
static struct proc_ops file_ops;

int get_process_index(int pid);
void createProc(struct process **proc, int pid);
void createDeque(struct Deque **deque, int32_t capacity);
int set_capacity(struct Deque **deque, int32_t capacity);

int push_right(struct Deque *deque, int32_t data);
int push_left(struct Deque *deque, int32_t data);
int pop_right(struct Deque *deque, int32_t *data);
int pop_left(struct Deque *deque, int32_t *data);
int get_info(struct Deque *deque, struct obj_info **info);

static int file_open(struct inode *inode, struct file *file);                              // Handles the open request
static int file_close(struct inode *inode, struct file *file);                             // Handles the close request
static long file_ioctl(struct file *file, unsigned int cmd, unsigned long args);           // Handles the ioctl request
static ssize_t file_read(struct file *file, char *buf, size_t len, loff_t *offset);        // Handles the read request
static ssize_t file_write(struct file *file, const char *buf, size_t len, loff_t *offset); // Handles the write request

static int __init lkm_init(void)
{
    int i;
    struct proc_dir_entry *entry = proc_create(PROC_FILENAME, 0666, NULL, &file_ops);
    if (!entry)
        return -ENOENT;

    /* Initialize the function pointers of file_ops */
    file_ops.proc_open = file_open;
    file_ops.proc_release = file_close;
    file_ops.proc_read = file_read;
    file_ops.proc_write = file_write;
    file_ops.proc_ioctl = file_ioctl;

    for (i = 0; i < PROCESS_LIMIT; i++)
        process_list[i] = NULL;

    printk(KERN_ALERT "Deque LKM Inserted Successfully\n");
    return 0;
}

static void __exit lkm_exit(void)
{
    remove_proc_entry(PROC_FILENAME, NULL);
    printk(KERN_ALERT "Deque LKM Removed Successfully\n");
}

module_init(lkm_init);
module_exit(lkm_exit);

int get_process_index(int pid)
{
    int i;
    for (i = 0; i < PROCESS_LIMIT; i++)
    {
        if (process_list[i] != NULL && process_list[i]->pid == pid)
            return i;
    }
    for (i = 0; i < PROCESS_LIMIT; i++)
    {
        if (process_list[i] == NULL)
            return i;
    }
    return -1;
}

void createProc(struct process **proc, int pid)
{
    (*proc) = (struct process *)kmalloc(sizeof(struct process), GFP_KERNEL);
    if (*proc == NULL)
        return;
    (*proc)->pid = pid;
    (*proc)->deque = NULL;
}

void createDeque(struct Deque **deque, int32_t capacity)
{
    (*deque) = (struct Deque *)kmalloc(sizeof(struct Deque), GFP_KERNEL);
    if (*deque == NULL)
        return;
    (*deque)->data = (int32_t *)kmalloc(sizeof(int32_t) * capacity, GFP_KERNEL);
    (*deque)->capacity = capacity;
    (*deque)->size = 0;
    (*deque)->front = -1;
    (*deque)->rear = -1;
}

int set_capacity(struct Deque **deque, int32_t capacity)
{
    if (*deque != NULL)
        kfree(*deque);
    createDeque(deque, capacity);
    if ((*deque == NULL) || (*deque)->data == NULL)
        return -1;
    return 0;
}

int push_right(struct Deque *deque, int32_t data)
{
    if (deque->size == deque->capacity)
    {
        printk(KERN_ALERT "Deque is full\n");
        return -1;
    }
    if (deque->size == 0)
    {
        deque->front = 0;
        deque->rear = 0;
    }
    else
    {
        deque->rear = (deque->rear + 1) % deque->capacity;
    }
    deque->data[deque->rear] = data;
    deque->size++;
    return 0;
}

int push_left(struct Deque *deque, int32_t data)
{
    if (deque->size == deque->capacity)
    {
        printk(KERN_ALERT "Deque is full\n");
        return -1;
    }
    if (deque->size == 0)
    {
        deque->front = 0;
        deque->rear = 0;
    }
    else
    {
        deque->front = (deque->front - 1 + deque->capacity) % deque->capacity;
    }
    deque->data[deque->front] = data;
    deque->size++;
    return 0;
}

int pop_right(struct Deque *deque, int32_t *data)
{
    if (deque->size == 0)
    {
        printk(KERN_ALERT "Deque is empty\n");
        return -1;
    }
    *data = deque->data[deque->rear];
    deque->rear = (deque->rear - 1 + deque->capacity) % deque->capacity;
    deque->size--;
    return 0;
}

int pop_left(struct Deque *deque, int32_t *data)
{
    if (deque->size == 0)
    {
        printk(KERN_ALERT "Deque is empty\n");
        return -1;
    }
    *data = deque->data[deque->front];
    deque->front = (deque->front + 1) % deque->capacity;
    deque->size--;
    return 0;
}

int get_info(struct Deque *deque, struct obj_info **info)
{
    (*info) = (struct obj_info *)kmalloc(sizeof(struct obj_info), GFP_KERNEL);
    (*info)->deque_size = deque->size;
    (*info)->capacity = deque->capacity;
    return 0;
}

int file_open(struct inode *inode, struct file *file)
{
    int32_t ret;
    int32_t pid;
    int32_t index;

    mutex_lock(&mutex);
    pid = current->pid;
    index = get_process_index(pid);

    if (index == -1)
    {
        printk(KERN_ALERT "Process not found\n");
        ret = -EACCES;
    }
    else if (process_list[index] != NULL)
    {
        printk(KERN_ALERT "Process already exists\n");
        ret = -EACCES;
    }
    else
    {
        createProc(&process_list[index], pid);
        if (process_list[index] == NULL)
        {
            printk(KERN_ALERT "Process cannot be created\n");
            ret = -EACCES;
        }
        else
        {
            createDeque(&process_list[index]->deque, 0);
            if (process_list[index]->deque == NULL)
            {
                printk(KERN_ALERT "Deque cannot be created\n");
                ret = -EACCES;
            }
            else
            {
                printk(KERN_ALERT "[%d] Deque LKM: File Opened\n", pid);
                ret = 0;
            }
        }
    }

    mutex_unlock(&mutex);
    return ret;
}

int file_close(struct inode *inode, struct file *file)
{
    int32_t ret;
    int32_t pid;
    int32_t index;

    mutex_lock(&mutex);
    pid = current->pid;
    index = get_process_index(pid);

    if (index == -1)
    {
        printk(KERN_ALERT "Process not found\n");
        ret = -EACCES;
    }
    else if (process_list[index] == NULL)
    {
        printk(KERN_ALERT "Process not found\n");
        ret = -EACCES;
    }
    else
    {
        kfree(process_list[index]->deque);
        process_list[index]->deque = NULL;
        kfree(process_list[index]);
        process_list[index] = NULL;
        printk(KERN_ALERT "[%d] Deque LKM: File Closed\n", pid);
        ret = 0;
    }

    mutex_unlock(&mutex);
    return ret;
}

ssize_t file_read(struct file *file, char *buf, size_t len, loff_t *offset) { return 0; }
ssize_t file_write(struct file *file, const char *buf, size_t len, loff_t *offset) { return 0; }

long file_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
    int32_t ret;
    int32_t pid;
    int32_t capacity;
    int32_t data;
    int32_t index;

    struct obj_info *info;

    mutex_lock(&mutex);

    ret = 0;
    pid = current->pid;

    switch (cmd)
    {
    case PB2_SET_CAPACITY:
        ret = access_ok((int *)args, sizeof(int));
        if (ret == 0)
        {
            printk(KERN_ALERT "access_ok failed\n");
            ret = -EACCES;
            goto out;
        }
        ret = copy_from_user(&capacity, (int32_t *)args, sizeof(int32_t));
        if (ret != 0)
        {
            printk(KERN_ALERT "copy_from_user failed\n");
            ret = -EACCES;
            goto out;
        }
        if (capacity > 100 || capacity < 0)
        {
            ret = -EINVAL;
            goto out;
        }
        index = get_process_index(pid);
        if (index == -1 || process_list[index] == NULL)
        {
            printk(KERN_ALERT "process not found\n");
            ret = -EACCES;
            goto out;
        }

        ret = set_capacity(&(process_list[index]->deque), capacity);
        if (ret != 0)
        {
            printk(KERN_ALERT "set_capacity failed\n");
            ret = -EACCES;
            goto out;
        }
        break;
    case PB2_INSERT_LEFT:
        ret = access_ok((int *)args, sizeof(int));
        if (ret == 0)
        {
            printk(KERN_ALERT "access_ok failed\n");
            ret = -EACCES;
            goto out;
        }
        ret = copy_from_user(&data, (int32_t *)args, sizeof(int32_t));
        if (ret != 0)
        {
            printk(KERN_ALERT "copy_from_user failed\n");
            ret = -EACCES;
            goto out;
        }

        index = get_process_index(pid);
        if (index == -1 || process_list[index] == NULL)
        {
            printk(KERN_ALERT "process not found\n");
            ret = -EACCES;
            goto out;
        }

        ret = push_left(process_list[index]->deque, data);
        if (ret != 0)
        {
            printk(KERN_ALERT "push_left failed\n");
            ret = -EACCES;
            goto out;
        }
        break;
    case PB2_INSERT_RIGHT:
        ret = access_ok((int *)args, sizeof(int));
        if (ret == 0)
        {
            printk(KERN_ALERT "access_ok failed\n");
            ret = -EACCES;
            goto out;
        }
        ret = copy_from_user(&data, (int32_t *)args, sizeof(int32_t));
        if (ret != 0)
        {
            printk(KERN_ALERT "copy_from_user failed\n");
            ret = -EACCES;
            goto out;
        }

        index = get_process_index(pid);
        if (index == -1 || process_list[index] == NULL)
        {
            printk(KERN_ALERT "process not found\n");
            ret = -EACCES;
            goto out;
        }
        ret = push_right(process_list[index]->deque, data);
        if (ret != 0)
        {
            printk(KERN_ALERT "push_right failed\n");
            ret = -EACCES;
            goto out;
        }
        break;
    case PB2_EXTRACT_LEFT:
        index = get_process_index(pid);
        if (index == -1 || process_list[index] == NULL)
        {
            printk(KERN_ALERT "process not found\n");
            ret = -EACCES;
            goto out;
        }
        ret = pop_left(process_list[index]->deque, &data);
        if (ret != 0)
        {
            printk(KERN_ALERT "pop_left failed\n");
            ret = -EACCES;
            goto out;
        }
        if (!access_ok((int32_t *)args, sizeof(int32_t)))
        {
            printk(KERN_ALERT "access_ok failed\n");
            ret = -EACCES;
            goto out;
        }
        ret = copy_to_user((int32_t *)args, &data, sizeof(int32_t));
        if (ret != 0)
        {
            printk(KERN_ALERT "copy_to_user failed\n");
            ret = -EACCES;
            goto out;
        }
        break;
    case PB2_EXTRACT_RIGHT:
        index = get_process_index(pid);
        if (index == -1 || process_list[index] == NULL)
        {
            printk(KERN_ALERT "process not found\n");
            ret = -EACCES;
            goto out;
        }
        ret = pop_right(process_list[index]->deque, &data);
        if (ret != 0)
        {
            printk(KERN_ALERT "pop_right failed\n");
            ret = -EACCES;
            goto out;
        }
        if (!access_ok((int32_t *)args, sizeof(int32_t)))
        {
            printk(KERN_ALERT "access_ok failed\n");
            ret = -EACCES;
            goto out;
        }
        ret = copy_to_user((int *)args, &data, sizeof(int));
        if (ret != 0)
        {
            printk(KERN_ALERT "copy_to_user failed\n");
            ret = -EACCES;
            goto out;
        }
        break;
    case PB2_GET_INFO:
        index = get_process_index(pid);
        if (index == -1 || process_list[index] == NULL)
        {
            printk(KERN_ALERT "process not found\n");
            ret = -EACCES;
            goto out;
        }

        ret = get_info(process_list[index]->deque, &info);
        if (ret != 0)
        {
            printk(KERN_ALERT "get_info failed\n");
            ret = -EACCES;
            goto out;
        }

        if (!access_ok((struct obj_info *)args, sizeof(struct obj_info)))
        {
            printk(KERN_ALERT "access_ok failed\n");
            ret = -EACCES;
            goto out;
        }

        ret = copy_to_user((struct obj_info *)args, info, sizeof(struct obj_info));
        if (ret != 0)
        {
            printk(KERN_ALERT "copy_to_user failed\n");
            ret = -EACCES;
            goto out;
        }
        break;
    default:
        printk(KERN_ALERT "Invalid Command\n");
        ret = -EINVAL;
    }

out:
    mutex_unlock(&mutex);
    return ret;
}
