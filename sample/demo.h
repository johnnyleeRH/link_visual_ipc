/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#ifndef PROJECT_DEMO_H
#define PROJECT_DEMO_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>

#include "link_visual_ipc.h"


/* 模拟IPC功能开关，该宏包含的内容都是模拟IPC生产流的功能，开发者集成到设备时需要删除宏内容并修改为实际IPC的功能 */
#define DUMMY_IPC

#define string_safe_copy(target, source, max_len) \
    do {        \
    if (target && source && max_len) \
            memcpy(target, source, (strlen(source) > max_len)?max_len:strlen(source));\
} while(0)


#if defined(__cplusplus)
}
#endif
#endif // PROJECT_DEMO_H