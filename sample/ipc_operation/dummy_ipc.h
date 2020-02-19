/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

/**
 * 文件用于模拟IPC的功能，仅用于demo测试使用
 * 开发者不需要过多关注，实际产品不会使用到该部分代码
 */

#ifndef PROJECT_DUMMY_IPC_H
#define PROJECT_DUMMY_IPC_H


/**
 * 文件用于模拟IPC输出视音频流
 * 开发者不需要过多关注，实际产品不会使用到该部分代码
 */

#include "link_visual_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DummyIpcMediaFileLen (256)
typedef  struct {
    char source_file[DummyIpcMediaFileLen + 1];
} DummyIpcMediaFile;

typedef enum {
    DUMMY_IPC_MEDIA_START = 0,
    DUMMY_IPC_MEDIA_PAUSE,
    DUMMY_IPC_MEDIA_UNPAUSE,
    DUMMY_IPC_MEDIA_STOP,
    DUMMY_IPC_MEDIA_SEEK,
    DUMMY_IPC_MEDIA_REQUEST_I_FRAME,
    DUMMY_IPC_MEDIA_CHANGE_STREAM,//切换主子码流
} DummyIpcMediaCmd;

typedef struct {
    int service_id;
    unsigned int stream_type;
    unsigned int seek_timestamp_ms;
    unsigned int base_time_ms;
    char source_file[DummyIpcMediaFileLen + 1];
} DummyIpcMediaParam;

typedef struct {
    /* 模拟实时直播的源 */
    unsigned int live_num;//num最大值为2，主码流和子码流
    DummyIpcMediaFile *live_source;

    /* 模拟本地录像的源 */
    unsigned int vod_num;
    DummyIpcMediaFile *vod_source;

    /* 模拟图片源 */
    char *picture_source;

    /* 每隔一段时间，触发一次移动报警抓图 */
    unsigned int picture_interval_s;

    /* 视频帧数据回调 */
    void (*video_handler)(int service_id, lv_video_format_e format, unsigned char *buffer, unsigned int buffer_size,
                                  unsigned int timestamp_ms, int nal_type);

    /* 音频帧数据回调 */
    void (*audio_handler)(int service_id, unsigned char *buffer, unsigned int buffer_size, unsigned int timestamp_ms);

    /* 报警图片数据回调 */
    void (*picture_handler)(unsigned char *buffer, unsigned int buffer_size, lv_event_type_e type, const char *id);

    /* 属性设置回调 */
    void (*set_property_handler)(const char*key, const char *value);
} DummyIpcConfig;


/* 开启虚拟IPC */
int dummy_ipc_start(const DummyIpcConfig *config);

/* 停止虚拟IPC */
void dummy_ipc_stop();

/* 直播控制（开启、切换主子码流） */
int dummy_ipc_live(DummyIpcMediaCmd cmd, const DummyIpcMediaParam *param);

/* 获取直播流参数 */
int dummy_ipc_get_live_media_param(lv_video_param_s *vparam, lv_audio_param_s *aparam, double *duration);

/* 按文件点播控制（打开、关闭文件、暂停、seek) */
int dummy_ipc_vod_by_file(DummyIpcMediaCmd cmd, const DummyIpcMediaParam *param);

/* 获取当前点播文件的配置 */
int dummy_ipc_vod_get_media_param(lv_video_param_s *vparam, lv_audio_param_s *aparam, double *duration);

/* 按utc点播控制（打开、关闭文件、暂停、seek) */
int dummy_ipc_vod_by_utc(DummyIpcMediaCmd cmd, const DummyIpcMediaParam *param);

/* 上报点播文件的信息 */
int dummy_ipc_report_vod_list(unsigned int start_time,
                              unsigned int stop_time,
                              int num,
                              const char *id,
                              int (*on_complete)(int num,
                                                 const char *id,
                                                 const lv_query_storage_record_response_s *response));

/* 设置属性 */
int dummy_ipc_set_property(const char *key, const char *value);

/* 获取所有属性值 */
void dummy_ipc_get_all_property();


/* 触发一次抓图 */
int dummy_ipc_capture(const char *id);

#ifdef __cplusplus
}
#endif

#endif //PROJECT_DUMMY_IPC_H
