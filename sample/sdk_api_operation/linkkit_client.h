/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#ifndef PROJECT_LINKKIT_DEMO_H
#define PROJECT_LINKKIT_DEMO_H

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * 文件用于描述linkkit的启动、结束过程，以及linkkit生命周期过程中对命令的收发处理
 */


/* linkkit资源初始化，并连接服务器 */
int linkkit_client_start(const char *product_key,
                         const char *product_secret,
                         const char *device_name,
                         const char *device_secret);

/* linkkit与服务器断开连接，并释放资源 */
void linkkit_client_destroy();

/* 属性设置回调 */
void linkkit_client_set_property_handler(const char *key, const char *value);


#if defined(__cplusplus)
}
#endif
#endif //PROJECT_LINKKIT_DEMO_H
