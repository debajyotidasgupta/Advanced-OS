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

#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/init.h>    /* Needed for the macros */
#include <linux/proc_fs.h> /* Needed for the proc filesystem */
#include <linux/uaccess.h> /* Needed for copy_from_user */
#include <linux/mutex.h>   /* Needed for mutex */
#include <linux/types.h>   /* Needed for size_t */
#include <linux/slab.h>    /* Needed for kmalloc */
#include <linux/string.h>  /* Needed for strlen */
#include <linux/sched.h>   /* Needed for current */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debajyoti Dasgupta");
MODULE_DESCRIPTION("LKM for Deque");
MODULE_VERSION("0.1");

#define PROC_FILENAME "cs60038_a2_18CS30051"
#define MAX_PROCESS_LIMIT 1024
#define MAX_BUFF_SIZE 256

#define PB2_SET_CAPACITY _IOW(0x10, 0x31, int32_t *)
#define PB2_INSERT_RIGHT _IOW(0x10, 0x32, int32_t *)
#define PB2_INSERT_LEFT _IOW(0x10, 0x33, int32_t *)
#define PB2_GET_INFO _IOR(0x10, 0x34, int32_t *)
#define PB2_POP_LEFT _IOR(0x10, 0x35, int32_t *)
#define PB2_POP_RIGHT _IOR(0x10, 0x36, int32_t *)

DEFINE_MUTEX(read_write_mutex);
DEFINE_MUTEX(open_close_mutex);

static char *buffer = NULL;
static ssize_t buffer_len = 0;
static struct proc_ops file_ops;
static size_t MAX_DEQUE_SIZE = 0;

int32_t iter;
int32_t data;
int32_t retval;
int32_t err_code;
int32_t capacity;
int32_t deque_size;
int32_t process_index;
int32_t process_state;

/**
 * @brief Structure to store the date of deque
 * @details This structure is used to store  the  data  which is
 *          to be stored in the deque in the form of a linked list.
 * @param max_size Maximum size of the deque
 * @param front_ptr Pointer to the front of the deque
 * @param rear_ptr Pointer to the rear of the deque
 * @param size Current size of the deque
 */

struct ListNode
{                                // Structure for node of the linked list
    int data;                    // Data to be stored in the deque
    struct ListNode *prev;       // Pointer to the previous node
    struct ListNode *next;       // Pointer to the next node
} * left_elem, *new_node, *temp; // Pointer to the new node

struct Deque
{                               // Structure to store the data of deque
    int max_size;               // Maximum size of the deque
    struct ListNode *front_ptr; // Pointer to the front of the deque
    struct ListNode *rear_ptr;  // Pointer to the rear of the deque
    int size;                   // Current size of the deque
};

/**
 * @brief Descriptors for the state of the process
 */

enum process_state
{
    PROC_FILE_OPEN,    /**< Process is open */
    PROC_DEQUE_CREATE, /**< Process is creating a deque */
    PROC_DEQUE_WRITE,  /**< Process is writing to the deque */
    PROC_FILE_CLOSE    /**< Process is closing the file */
};

/**
 * @brief Structure to store the data of a unit process
 * @details This structure is used to store the data of a unit process
 *         in the form of a structure.
 */

struct unit_process
{                             // Structure to store the data of a unit process
    pid_t pid;                // PID of the process
    enum process_state state; //!< State of the process
    struct Deque *proc_deque; //!< Pointer to the deque
};

struct many_process
{                                       // Structure to store the data of a unit process
    struct unit_process *process_queue; // Array of processes
    size_t number_of_process;           // Number of processes
};

struct obj_info
{
    int32_t deque_size; // current number of elements in deque
    int32_t capacity;   // maximum capacity of the deque
};

struct many_process *process_list;

/**
 * Utility Functions
 * -----------------
 * create_node()  - Creates a new node
 * create_deque() - Creates a new deque
 * push_front()   - Pushes a new node to the front of the deque
 * push_rear()    - Pushes a new node to the rear of the deque
 * delete_front() - Deletes the front node of the deque
 */

// Utility function for Deque
struct ListNode *create_node(int data);                       // Creates a new node
struct Deque *create_deque(int max_size);                     // Creates a new deque
struct ListNode *insert_front(struct Deque *deque, int data); // Inserts a new node to the front of the deque
struct ListNode *insert_rear(struct Deque *deque, int data);  // Inserts a new node to the rear of the deque
bool delete_front(struct Deque *deque);                       // Deletes the front node of the deque
bool delete_rear(struct Deque *deque);                        // Deletes the rear node of the deque
void delete_deque(struct Deque *deque);                       // Deletes the deque
void print_deque(struct Deque *deque);                        // Prints the deque
static int init_proc_deque(size_t process_index);             // Initializes the deque of a process
static int insert_proc_deque(size_t process_index);           // Inserts a new node to the deque of a process

// Utility function for process
static size_t handle_read(size_t process_index);                                           // Handles the read request
static ssize_t handle_write(size_t process_index);                                         // Handles the write request
static void init_process_list(void);                                                       // Initializes the process list
static int file_open(struct inode *inode, struct file *file);                              // Handles the open request
static int file_close(struct inode *inode, struct file *file);                             // Handles the close request
static long file_ioctl(struct file *, unsigned int, unsigned long);                        // Handles the ioctl request
static ssize_t file_read(struct file *file, char *buf, size_t len, loff_t *offset);        // Handles the read request
static ssize_t file_write(struct file *file, const char *buf, size_t len, loff_t *offset); // Handles the write request

// Utility function for file operations
size_t get_list_index(pid_t pid); // Returns the index of the process in the process list

/**
 * Kernel Module Functions
 * -----------------------
 * lkm_init() - Initializes the kernel module
 * lkm_exit() - Exits the kernel module and frees the memory
 */

static int __init lkm_init(void)
{
    struct proc_dir_entry *entry = proc_create(PROC_FILENAME, 0666, NULL, &file_ops);
    if (!entry)
        return -ENOENT;

    /* Initialize the function pointers of file_ops */
    file_ops.proc_open = file_open;
    file_ops.proc_release = file_close;
    file_ops.proc_read = file_read;
    file_ops.proc_write = file_write;
    file_ops.proc_ioctl = file_ioctl;

    init_process_list();                                                // Initialize the process list
    buffer = (char *)kmalloc(MAX_BUFF_SIZE * sizeof(char), GFP_KERNEL); // Allocate memory for the buffer

    printk(KERN_ALERT "Deque LKM Inserted Successfully\n");
    return 0;
}

static void __exit lkm_exit(void)
{
    remove_proc_entry(PROC_FILENAME, NULL);
    kfree(process_list);
    printk(KERN_ALERT "Deque LKM Removed Successfully\n");
}

module_init(lkm_init);
module_exit(lkm_exit);

// ------- END OF LOADABLE KERNEL MODULE CODE -------

/**
 * Function Definitions
 * --------------------
 */

/**
 * @brief Initializes the process list and the Deques
 * @details This function initializes the process list and the Deques
 *         for each process.
 */

void init_process_list(void)
{
    process_list = (struct many_process *)kmalloc(sizeof(struct many_process), GFP_KERNEL);
    process_list->process_queue = (struct unit_process *)kmalloc(sizeof(struct unit_process) * MAX_PROCESS_LIMIT, GFP_KERNEL);
    process_list->number_of_process = 0;

    for (iter = 0; iter < MAX_PROCESS_LIMIT; iter++)
    {
        process_list->process_queue[iter].pid = -1;
        process_list->process_queue[iter].state = PROC_FILE_CLOSE;
        process_list->process_queue[iter].proc_deque = NULL;
    }
}

/**
 * @brief Open proc file for interaction
 * @details This function is called when the proc file is to be opened.
 *         It checks if the process is already opened or not.
 * @param inode Inode of the proc file
 * @param file File structure of the proc file
 * @return Returns the error code
 */

int file_open(struct inode *inode, struct file *file)
{
    mutex_lock(&open_close_mutex);
    err_code = 0;

    process_index = get_list_index(current->pid);

    // If the process is already opened or the process list is full and needs to be inserted
    if (process_index == MAX_PROCESS_LIMIT || process_list->process_queue[process_index].state != PROC_FILE_CLOSE)
    {
        err_code = -EACCES; // Too many files open
        goto exit_file_open;
    }

    // Initialize the process
    process_list->process_queue[process_index].pid = current->pid;
    process_list->process_queue[process_index].state = PROC_FILE_OPEN;
    process_list->process_queue[process_index].proc_deque = create_deque(MAX_DEQUE_SIZE);
    process_list->number_of_process++;

    printk(KERN_ALERT "Process %d opened\n", current->pid);

    // Return the error code
exit_file_open:
    mutex_unlock(&open_close_mutex);

    return err_code;
}

int file_close(struct inode *inode, struct file *file)
{
    mutex_lock(&open_close_mutex);
    err_code = 0;

    process_index = get_list_index(current->pid);

    // If the process is not opened or file that needs to be closed is not the current file
    if (process_index == MAX_PROCESS_LIMIT || process_list->process_queue[process_index].pid != current->pid)
    {
        err_code = -EINVAL; // File not opened
        goto exit_file_close;
    }

    // Delete the deque
    if (process_list->process_queue[process_index].state >= PROC_DEQUE_CREATE)
    {
        delete_deque(process_list->process_queue[process_index].proc_deque);
        process_list->process_queue[process_index].proc_deque = NULL;
    }

    // Set the state of the process to closed
    process_list->process_queue[process_index].state = PROC_FILE_CLOSE;
    process_list->process_queue[process_index].pid = -1;
    process_list->number_of_process--;

    printk(KERN_ALERT "Process %d closed\n", current->pid);

    // Return the error code
exit_file_close:
    mutex_unlock(&open_close_mutex);
    return err_code;
}

/**
 * @brief Handles the ioctl request
 * @details This function handles the ioctl request.
 *        It checks if the process is opened and if the request is valid.
 * @param file File structure of the proc file
 * @param cmd Command of the ioctl request
 * @param arg Argument of the ioctl request
 * @return Returns the error code
 */

static long file_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct obj_info info;
    err_code = 0;

    mutex_lock(&read_write_mutex);
    switch (cmd)
    {
    case PB2_SET_CAPACITY:;
        /**
         * @brief Sets the capacity of the deque
         * @details This function sets the capacity of the deque.
         *        It checks if the process is opened and if the capacity is valid.
         * @param file File structure of the proc file
         * @param cmd Command of the ioctl request
         * @param arg Argument of the ioctl request
         * @return Returns the error code
         */

        retval = copy_from_user(&capacity, (int32_t *)arg, sizeof(int32_t));
        if (retval)
        {
            err_code = -EINVAL;
            goto exit_file_ioctl;
        }

        // Initialize heap after elementary checks
        if (capacity < 1 || capacity > 100)
        {
            printk(KERN_ALERT "PID %d: PB2_SET_CAPACITY cannot create as capacity is invalid", current->pid);
            err_code = -EINVAL;
            goto exit_file_ioctl;
        }

        delete_deque(process_list->process_queue[process_index].proc_deque);            // Delete the deque
        process_list->process_queue[process_index].proc_deque = create_deque(capacity); // Create the deque
        process_list->process_queue[process_index].state = PROC_DEQUE_CREATE;           // Set the state of the process to created
        process_list->process_queue[process_index].pid = current->pid;                  // Set the pid of the process
        printk(KERN_ALERT "PID %d: PB2_SET_CAPACITY created with capacity %d\n", current->pid, capacity);

        break;

    case PB2_INSERT_RIGHT:;
        /**
         * @brief Inserts an object to the right of the deque
         * @details This function inserts an object to the right of the deque.
         *       It checks if the process is opened and if the object is valid.
         * @param file File structure of the proc file
         * @param cmd Command of the ioctl request
         * @param arg Argument of the ioctl request
         * @return Returns the error code
         */

        process_index = get_list_index(current->pid);

        // If the process is already opened or the process list is full and needs to be inserted
        if (process_index == MAX_PROCESS_LIMIT)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        if (process_list->process_queue[process_index].state == PROC_FILE_OPEN)
        {
            printk(KERN_ALERT "PID %d: PB2_INSERT_RIGHT cannot insert as deque is not initialized", current->pid);
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        // Get the integer number to be inserted in the deque
        retval = copy_from_user(&data, (int32_t *)arg, sizeof(int32_t));
        if (retval)
        {
            return -EINVAL;
            goto exit_file_ioctl;
        }

        printk(KERN_ALERT "PID %d: PB2_INSERT_RIGHT Inserting %d into the deque\n", current->pid, data);
        temp = insert_rear(process_list->process_queue[process_index].proc_deque, data);

        if (!temp)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }
        break;

    case PB2_INSERT_LEFT:;
        /**
         * @brief Inserts an object to the left of the deque
         * @details This function inserts an object to the left of the deque.
         *      It checks if the process is opened and if the object is valid.  
         * @param file File structure of the proc file
         * @param cmd Command of the ioctl request
         * @param arg Argument of the ioctl request
         * @return Returns the error code
         * @return Returns the error code
         */

        process_index = get_list_index(current->pid);

        // If the process is already opened or the process list is full and needs to be inserted
        if (process_index == MAX_PROCESS_LIMIT)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        if (process_list->process_queue[process_index].state == PROC_FILE_OPEN)
        {
            printk(KERN_ALERT "PID %d: PB2_INSERT_LEFT cannot insert as deque is not initialized", current->pid);
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        // Get the integer number to be inserted in the deque
        retval = copy_from_user(&data, (int32_t *)arg, sizeof(int32_t));
        if (retval)
        {
            return -EINVAL;
            goto exit_file_ioctl;
        }

        printk(KERN_ALERT "PID %d: PB2_INSERT_LEFT Inserting %d into the deque\n", current->pid, data);
        temp = insert_front(process_list->process_queue[process_index].proc_deque, data);

        if (!temp)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }
        break;

    case PB2_GET_INFO:
        /**
         * @brief Gets the information of the deque
         * @details This function gets the information of the deque.
         *     It checks if the process is opened and if the object is valid.
         * @param file File structure of the proc file
         * @param cmd Command of the ioctl request
         * @param arg Argument of the ioctl request
         * @return Returns the error code
         */

        process_index = get_list_index(current->pid);

        // If the process is already opened or the process list is full and needs to be inserted
        if (process_index == MAX_PROCESS_LIMIT)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        if (process_list->process_queue[process_index].state == PROC_FILE_OPEN)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        info.capacity = process_list->process_queue[process_index].proc_deque->max_size;
        info.deque_size = process_list->process_queue[process_index].proc_deque->size;

        retval = copy_to_user((struct obj_info *)arg, &info, sizeof(struct obj_info));
        if (retval)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }
        break;

    case PB2_POP_LEFT:;
        /**
         * @brief Removes an object from the left of the deque
         * @details This function removes an object from the left of the deque.
         *    It checks if the process is opened and if the object is valid.
         * @param file File structure of the proc file
         * @param cmd Command of the ioctl request
         * @param arg Argument of the ioctl request
         * @return Returns the error code
         */

        process_index = get_list_index(current->pid);

        if (process_index == MAX_PROCESS_LIMIT)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        if (process_list->process_queue[process_index].state == PROC_FILE_OPEN)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        if (process_list->process_queue[process_index].proc_deque->front_ptr)
            data = process_list->process_queue[process_index].proc_deque->front_ptr->data;
        retval = delete_front(process_list->process_queue[process_index].proc_deque);
        if (!retval)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }
        printk(KERN_ALERT "PID %d: PB2_POP_LEFT Popping %d from the deque\n", current->pid, data);
        retval = copy_to_user((int32_t *)arg, &data, sizeof(int32_t));
        if (retval)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }
        break;

    case PB2_POP_RIGHT:;
        /**
         * @brief Removes an object from the right of the deque
         * @details This function removes an object from the right of the deque.
         *   It checks if the process is opened and if the object is valid.
         * @param file File structure of the proc file
         * @param cmd Command of the ioctl request
         * @param arg Argument of the ioctl request
         * @return Returns the error code
         */

        process_index = get_list_index(current->pid);

        if (process_index == MAX_PROCESS_LIMIT)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        if (process_list->process_queue[process_index].state == PROC_FILE_OPEN)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }

        if (process_list->process_queue[process_index].proc_deque->rear_ptr)
            data = process_list->process_queue[process_index].proc_deque->rear_ptr->data;
        retval = delete_rear(process_list->process_queue[process_index].proc_deque);
        if (!retval)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }
        printk(KERN_ALERT "PID %d: PB2_POP_RIGHT Popping %d from the deque\n", current->pid, data);
        retval = copy_to_user((int32_t *)arg, &data, sizeof(int32_t));
        if (retval)
        {
            err_code = -EACCES;
            goto exit_file_ioctl;
        }
        break;

    default:
        err_code = -EINVAL;
    }

    // Return the error code
exit_file_ioctl:
    // print_deque(process_list->process_queue[process_index].proc_deque);
    mutex_unlock(&read_write_mutex);
    return err_code;
}

ssize_t file_read(struct file *file, char *buf, size_t len, loff_t *offset)
{
    mutex_lock(&read_write_mutex);
    err_code = 0;
    process_index = get_list_index(current->pid);

    // Process not found
    if (process_index == MAX_PROCESS_LIMIT)
    {
        err_code = -EINVAL;
        goto exit_file_read;
    }

    // empty buffer
    if (!buf || !len)
    {
        err_code = -EINVAL;
        goto exit_file_read;
    }

    buffer_len = len < MAX_BUFF_SIZE ? len : MAX_BUFF_SIZE;
    err_code = handle_read(process_index);

    if (!err_code && copy_to_user(buf, buffer, buffer_len))
        err_code = -ENOBUFS;
    if (!err_code)
        err_code = buffer_len;

    // Return the error code
exit_file_read:
    mutex_unlock(&read_write_mutex);
    return err_code;
}

ssize_t file_write(struct file *file, const char *buf, size_t len, loff_t *offset)
{
    mutex_lock(&read_write_mutex);
    err_code = 0;
    process_index = get_list_index(current->pid);

    // Process not found
    if (process_index == MAX_PROCESS_LIMIT)
    {
        err_code = -EINVAL;
        goto exit_file_write;
    }

    // empty buffer
    if (!buf || !len)
    {
        err_code = -EINVAL;
        goto exit_file_write;
    }

    buffer_len = len < MAX_BUFF_SIZE ? len : MAX_BUFF_SIZE;

    if (copy_from_user(buffer, buf, buffer_len))
        err_code = -ENOBUFS;

    if (!err_code)
        err_code = handle_write(process_index);
    if (!err_code)
        err_code = buffer_len;

    // printk(KERN_INFO "Process [%d] wrote [%d] code [%d]\n", current->pid, buffer[0], err_code);
    // Return the error code
exit_file_write:
    mutex_unlock(&read_write_mutex);
    return err_code;
}

/**
 * @brief Get the index of the process id in the process list
 * @details This function returns the index of the process id in the
 *       process list.
 * @param pid Process id of the process
 * @return Returns the index of the process id in the process list
 *        Returns minimum index which is free in the process list if
 *        the process id is not found
 */

size_t get_list_index(pid_t pid)
{
    iter = 0;

    // Search for the process id in the process list
    for (iter = 0; iter < MAX_PROCESS_LIMIT; iter++)
    {
        if (process_list->process_queue[iter].pid == pid && process_list->process_queue[iter].state < PROC_FILE_CLOSE)
        {
            return iter;
        }
    }

    // If the process id is not found, return the minimum index which is free
    for (iter = 0; iter < MAX_PROCESS_LIMIT; iter++)
    {
        if (process_list->process_queue[iter].pid == -1 || process_list->process_queue[iter].state == PROC_FILE_CLOSE)
        {
            return iter;
        }
    }

    // If all the indexes are full, return MAX_PROCESS_LIMIT
    return MAX_PROCESS_LIMIT;
}

/**
 * @brief Handle the read operation
 * @details This function handles the read operation.
 * @param process_index Index of the process in the process list
 * @return Returns the error code
 * @note This function is called with the read_write_mutex locked
 */

static size_t handle_read(size_t process_index)
{
    process_state = process_list->process_queue[process_index].state;
    if (process_state < PROC_DEQUE_CREATE)
    {
        return -EACCES;
    }

    left_elem = process_list->process_queue[process_index].proc_deque->front_ptr;
    if (left_elem != NULL)
    {
        data = left_elem->data;
        if (!delete_front(process_list->process_queue[process_index].proc_deque))
        {
            return -EACCES;
        }
        strncpy(buffer, (const char *)&data, sizeof(int));
        buffer[sizeof(left_elem->data)] = '\0';
        return 0;
    }
    else
    {
        return -EACCES;
    }
    return 0;
}

/**
 * @brief Handle the write operation
 * @details This function handles the write operation.
 * @param process_index Index of the process in the process list
 * @return Returns the error code
 * @note This function is called with the read_write_mutex locked
 */

static ssize_t handle_write(size_t process_index)
{
    process_state = process_list->process_queue[process_index].state;
    err_code = 0;

    if (process_state == PROC_FILE_OPEN)
    {
        // Initialize the deque
        err_code = init_proc_deque(process_index);
        if (err_code >= 0)
        {
            process_list->process_queue[process_index].state = PROC_DEQUE_CREATE;
        }
    }
    else if (process_state >= PROC_DEQUE_CREATE)
    {
        // Add the data to the deque
        err_code = insert_proc_deque(process_index);
        if (err_code >= 0)
        {
            process_list->process_queue[process_index].state = PROC_DEQUE_WRITE;
        }
    }
    else
    {
        err_code = -EACCES;
    }

    return err_code;
}

/**
 * @brief Function to create a new node
 * @details This function is used to create a new node in the deque.
 * @param data Data to be stored in the node
 * @return Returns the pointer to the new node
 * @retval NULL If the node cannot be created
 * @retval !NULL If the node is created successfully
 */

struct ListNode *create_node(int data)
{
    struct ListNode *new_node = (struct ListNode *)kmalloc(sizeof(struct ListNode), GFP_KERNEL);
    if (new_node == NULL)
    {
        printk(KERN_ALERT "Cannot create node\n");
        return NULL;
    }
    new_node->data = data;
    new_node->prev = NULL;
    new_node->next = NULL;
    return new_node;
}

/**
 * @brief Function to create a new deque
 * @details This function is used to create a new deque.
 * @param max_size Maximum size of the deque
 * @return Returns the pointer to the new deque
 * @retval NULL If the deque cannot be created
 * @retval !NULL If the deque is created successfully
 */

struct Deque *create_deque(int max_size)
{
    struct Deque *new_deque = (struct Deque *)kmalloc(sizeof(struct Deque), GFP_KERNEL);
    if (new_deque == NULL)
    {
        printk(KERN_ALERT "Cannot create deque\n");
        return NULL;
    }
    new_deque->max_size = max_size;
    new_deque->front_ptr = NULL;
    new_deque->rear_ptr = NULL;
    new_deque->size = 0;
    return new_deque;
}

/**
 * @brief Function to insert a node at the front of the deque
 * @details This function is used to insert a node at the front of the deque.
 * @param deque Pointer to the deque
 * @param data Data to be stored in the node
 * @return Returns the pointer to the new node
 * @retval NULL If the node cannot be inserted
 * @retval !NULL If the node is inserted successfully
 */

struct ListNode *insert_front(struct Deque *deque, int data)
{
    if (deque->size == deque->max_size)
    {
        printk(KERN_ALERT "Deque is full for pid [%d]\n", current->pid);
        return NULL;
    }

    new_node = create_node(data);
    if (new_node == NULL)
    {
        printk(KERN_INFO "Cannot insert node for pid [%d]\n", current->pid);
        return NULL;
    }
    if (deque->size == 0)
    {
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
    return new_node;
}

/**
 * @brief Function to insert a node at the rear of the deque
 * @details This function is used to insert a node at the rear of the deque.
 * @param deque Pointer to the deque
 * @param data Data to be stored in the node
 * @return Returns the pointer to the new node
 * @retval NULL If the node cannot be inserted
 * @retval !NULL If the node is inserted successfully
 */

struct ListNode *insert_rear(struct Deque *deque, int data)
{
    if (deque->size == deque->max_size)
    {
        printk(KERN_ALERT "Deque is full for pid [%d]\n", current->pid);
        return NULL;
    }

    new_node = create_node(data);
    if (new_node == NULL)
    {
        printk(KERN_INFO "Cannot insert node for pid [%d]\n", current->pid);
        return NULL;
    }
    if (deque->size == 0)
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
    // print_deque(deque);
    return new_node;
}

/**
 * @brief Function to print the deque
 * @details This function is used to print the deque.
 * @param deque Pointer to the deque
 * @return Returns void
 * @note This function is called with the read_write_mutex locked
 */

void print_deque(struct Deque *deque)
{
    struct ListNode *temp = deque->front_ptr;
    printk(KERN_INFO "\n");
    printk(KERN_INFO "Deque size [%d]\n", deque->size);
    printk(KERN_INFO "Deque max size [%d]\n", deque->max_size);

    printk(KERN_INFO "[Proc : %d] Deque size [%d]\n", current->pid, deque->size);
    while (temp != NULL)
    {
        printk(KERN_INFO "Node data [%d]\n", temp->data);
        temp = temp->next;
    }
}

/**
 * @brief Function to delete a node from the front of the deque
 * @details This function is used to delete a node from the front of the deque.
 * @param deque Pointer to the deque
 * @return boolean Returns true if the node is deleted successfully
 * @retval true If the node is deleted successfully
 * @retval false If the node cannot be deleted
 */

bool delete_front(struct Deque *deque)
{
    if (deque->size == 0)
    {
        printk(KERN_ALERT "Deque is empty\n");
        return false;
    }
    temp = deque->front_ptr;
    if (deque->size == 1)
    {
        deque->front_ptr = NULL;
        deque->rear_ptr = NULL;
    }
    else
    {
        deque->front_ptr = deque->front_ptr->next;
        deque->front_ptr->prev = NULL;
    }
    kfree(temp);
    deque->size--;
    return true;
}

/**
 * @brief Function to delete a node from the back of the deque
 * @details This function is used to delete a node from the back of the deque.
 * @param deque Pointer to the deque
 * @return boolean Returns true if the node is deleted successfully
 * @retval true If the node is deleted successfully
 * @retval false If the node cannot be deleted
 */

bool delete_rear(struct Deque *deque)
{
    if (deque->size == 0)
    {
        printk(KERN_ALERT "Deque is empty\n");
        return false;
    }
    temp = deque->rear_ptr;
    if (deque->size == 1)
    {
        deque->front_ptr = NULL;
        deque->rear_ptr = NULL;
    }
    else
    {
        deque->rear_ptr = deque->rear_ptr->prev;
        deque->rear_ptr->next = NULL;
    }
    kfree(temp);
    deque->size--;
    return true;
}

/**
 * @brief Function to delete the deque
 * @details This function is used to delete the deque.
 * @param deque Pointer to the deque
 * @return boolean Returns true if the deque is deleted successfully
 */
void delete_deque(struct Deque *deque)
{
    while (deque->size > 0)
    {
        delete_front(deque);
    }
    kfree(deque);
}

/**
 * @brief Function to initialize the deque attributes
 * @details This function is used to initialize the deque attributes.
 * @param process index of the process
 * @return error code
 * @retval 0 If the deque attributes are initialized successfully
 */

static int init_proc_deque(size_t process_index)
{
    if (buffer_len > sizeof(char))
    {
        printk(KERN_ALERT "Buffer length is too large\n");
        err_code = -EINVAL;
        goto out;
    }

    err_code = 0;
    deque_size = (size_t)buffer[0];
    if (deque_size < 1 || deque_size > 100)
    {
        printk(KERN_ALERT "Deque size is invalid\n");
        return -EINVAL;
    }

    err_code = ((process_list->process_queue[process_index].proc_deque = create_deque(deque_size)) != NULL ? 0 : -EINVAL);

    if (err_code != 0)
    {
        printk(KERN_ALERT "Cannot initialize deque\n");
        goto out;
    }

    printk(KERN_INFO "Deque initialized for process [%d]\n", current->pid);
out:
    return err_code;
}

/**
 * @brief Function to insert an integer into the deque
 * @details This function is used to insert an integer into the deque.
 * @param process index of the process
 * @param data integer to be inserted
 * @return error code
 * @retval len If the integer is inserted successfully
 */

static int insert_proc_deque(size_t process_index)
{
    err_code = 0;
    if (buffer_len != sizeof(int32_t))
    {
        err_code = -EINVAL;
        goto exit;
    }

    data = *(int32_t *)buffer;
    printk(KERN_INFO "Inserting [%d] into deque for process [%d]\n", data, current->pid);

    if (data % 2 == 0)
    {
        err_code = ((insert_rear(process_list->process_queue[process_index].proc_deque, data) != NULL) ? buffer_len : -EACCES);
    }
    else
    {
        err_code = ((insert_front(process_list->process_queue[process_index].proc_deque, data) != NULL) ? buffer_len : -EACCES);
    }

exit:
    return err_code;
}