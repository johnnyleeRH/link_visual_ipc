/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#ifndef PROJECT_LINKVISUAL_DEMO_H
#define PROJECT_LINKVISUAL_DEMO_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "link_visual_def.h"

/**
* 文件用于描述LinkVisual的启动、结束过程，以及生命周期过程中对音视频的传输过程
*/

/* linkvisual资源初始化 */
int linkvisual_client_init(const char *product_key,
                           const char *device_name,
                           const char *device_secret,
                           lv_log_level_e log_level);

/* linkvisual资源销毁 */
void linkvisual_client_destroy();


/* 来自IPC的视频帧数据回调 */
void linkvisual_client_video_handler(int service_id, lv_video_format_e format, unsigned char *buffer, unsigned int buffer_size,
                                     unsigned int present_time, int nal_type);

/* 来自IPC的音频帧数据回调 */
void linkvisual_client_audio_handler(int service_id,
                                     unsigned char *buffer,
                                     unsigned int buffer_size,
                                     unsigned int present_time);

/* 来自IPC的报警图片数据回调 */
void linkvisual_client_picture_handler(unsigned char *buffer,
                                       unsigned int buffer_size,
                                       lv_event_type_e type,
                                       const char *id);

/* 调试过程中使用，可以读取交互式命令，改变部分SDK的行为，可改变的行为请参考：lv_control_type_e */
void linkvisual_client_control_test();

#if defined(__cplusplus)
}
#endif
#endif // PROJECT_LINKVISUAL_DEMO_H