#ifndef DEQUE_H
#define DEQUE_H

#define PROC_FILENAME "cs60038_a2_18CS30051"
typedef int int32_t;

#define PB2_SET_CAPACITY        _IOW(0x10, 0x31, int32_t*)
#define PB2_INSERT_RIGHT        _IOW(0x10, 0x32, int32_t*)
#define PB2_INSERT_LEFT         _IOW(0x10, 0x33, int32_t*)
#define PB2_GET_INFO            _IOR(0x10, 0x34, int32_t*)
#define PB2_EXTRACT_LEFT        _IOR(0x10, 0x35, int32_t*)
#define PB2_EXTRACT_RIGHT       _IOR(0x10, 0x36, int32_t*)

struct obj_info {
    int32_t deque_size;     // current number of elements in deque
    int32_t capacity;       // maximum capacity of deque
};

#endif