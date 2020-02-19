/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>


#include "link_visual_def.h"
#include "link_visual_ipc.h"
#include "iot_import.h"
#include "linkkit_client.h"
#include "demo.h"

#ifdef DUMMY_IPC
#include "dummy_ipc.h"
#endif // DUMMY_IPC

#ifdef DUMMY_IPC
static int g_live_service_id = -1;
static int g_vod_service_id = -1;
static int g_voice_intercom_service_id = -1;
int g_voice_intercom_upstream_disable = 0;
static lv_stream_type_e g_vod_mod = LV_STREAM_CMD_STORAGE_RECORD_BY_FILE;//SDK支持多种点播模式

#endif // DUMMY_IPC

static int start_voice_intercom_cb(int service_id) {
    printf("service_id:%d\n", service_id);
#ifdef DUMMY_IPC
    lv_video_param_s video_param;
    lv_audio_param_s audio_param;
    memset(&video_param, 0, sizeof(lv_video_param_s));
    memset(&audio_param, 0, sizeof(lv_audio_param_s));
    double duration = 0;
    dummy_ipc_get_live_media_param(&video_param, &audio_param, &duration);

    printf("start voice intercom, service id=%d, meta data: %d, %d, %d\n", audio_param.channel, audio_param.sample_bits, audio_param.sample_rate, audio_param.format);
    int ret = lv_voice_intercom_start_service(service_id, &audio_param);
    if (ret < 0) {
        return -1;
    }
    g_voice_intercom_service_id = service_id;

#endif // DUMMY_IPC
    return 0;
}

static int stop_voice_intercom_cb(int service_id) {
    printf("stop voice intercom, service_id=%d\n", service_id);
    lv_voice_intercom_stop_service(service_id);
    g_voice_intercom_service_id = -1;
    return 0;
}

static int voice_intercom_receive_metadata_cb(int service_id, const lv_audio_param_s *audio_param) {
    printf("voice intercom receive metadata, service_id:%d\n", service_id);
    return 0;
}

static void voice_intercom_receive_data_cb(const char *buffer, unsigned int buffer_size) {
    printf("voice intercom receive voice data, size:%d\n", buffer_size);
}

static void query_storage_record_cb(unsigned int start_time,
                                    unsigned int stop_time,
                                    int num,
                                    const char *id,
                                    int (*on_complete)(int num,
                                                       const char *id,
                                                       const lv_query_storage_record_response_s *response)) {
    printf("start_time:%d stop_time:%d num:%d\n", start_time, stop_time, num);
#ifdef DUMMY_IPC
    dummy_ipc_report_vod_list(start_time, stop_time, num, id, on_complete);
#endif // DUMMY_IPC
}

static void query_storage_record_by_month_cb(const char *month, const char *id,
                                             int (*on_complete)(const char *id, const int *response)) {
    printf("Month:%s \n", month);
    int response[31] = {0};
    /* demo设置部分天数为有录像 */
    response[0] = 1;
    response[10] = 1;
    response[27] = 1;
    on_complete(id, response);
}

static int trigger_pic_capture_cb(const char *trigger_id) {
    printf("trigger_id:%s\n", trigger_id);
#ifdef DUMMY_IPC
    dummy_ipc_capture(trigger_id);
#endif // DUMMY_IPC

    return 0;
}

static int trigger_record_cb(int type, int duration, int pre_duration) {
    printf("\n");

    return 0;
}

static int on_push_streaming_cmd_cb(int service_id, lv_on_push_streaming_cmd_type_e cmd, unsigned int timestamp_ms) {
    printf("on_push_streaming_cmd_cb service_id:%d, cmd:%d\n", service_id, cmd);

#ifdef DUMMY_IPC
    DummyIpcMediaParam ipc_param = {0};
    ipc_param.service_id = service_id;
    if (cmd == LV_LIVE_REQUEST_I_FRAME) {
        lv_video_param_s video_param;
        lv_audio_param_s audio_param;
        memset(&video_param, 0, sizeof(lv_video_param_s));
        memset(&audio_param, 0, sizeof(lv_audio_param_s));
        double duration = 0;
        dummy_ipc_get_live_media_param(&video_param, &audio_param, &duration);
        lv_stream_send_config(service_id, 1000, duration, &video_param, &audio_param);
        dummy_ipc_live(DUMMY_IPC_MEDIA_REQUEST_I_FRAME, &ipc_param);
    } else if (cmd == LV_STORAGE_RECORD_SEEK) {
        ipc_param.seek_timestamp_ms = timestamp_ms;
        if (g_vod_mod == LV_STREAM_CMD_STORAGE_RECORD_BY_FILE) {
            dummy_ipc_vod_by_file(DUMMY_IPC_MEDIA_SEEK, &ipc_param);
        } else {
            dummy_ipc_vod_by_utc(DUMMY_IPC_MEDIA_SEEK, &ipc_param);
        }
    } else if (cmd == LV_STORAGE_RECORD_PAUSE) {
        if (g_vod_mod == LV_STREAM_CMD_STORAGE_RECORD_BY_FILE) {
            dummy_ipc_vod_by_file(DUMMY_IPC_MEDIA_PAUSE, &ipc_param);
        } else {
            dummy_ipc_vod_by_utc(DUMMY_IPC_MEDIA_PAUSE, &ipc_param);
        }
    } else if (cmd == LV_STORAGE_RECORD_UNPAUSE) {
        if (g_vod_mod == LV_STREAM_CMD_STORAGE_RECORD_BY_FILE) {
            dummy_ipc_vod_by_file(DUMMY_IPC_MEDIA_UNPAUSE, &ipc_param);
        } else {
            dummy_ipc_vod_by_utc(DUMMY_IPC_MEDIA_UNPAUSE, &ipc_param);
        }
    } else if (cmd == LV_STORAGE_RECORD_START) {
        if (g_vod_mod == LV_STREAM_CMD_STORAGE_RECORD_BY_FILE) {
            dummy_ipc_vod_by_file(DUMMY_IPC_MEDIA_UNPAUSE, &ipc_param);
        } else {
            dummy_ipc_vod_by_utc(DUMMY_IPC_MEDIA_UNPAUSE, &ipc_param);
        }
    }
#endif // DUMMY_IPC

    return 0;
}

static int start_push_streaming_cb(int service_id, lv_stream_type_e cmd_type, const lv_stream_param_s *param) {
    printf("startPushStreamingCallback:%d %d\n", service_id, cmd_type);

#ifdef DUMMY_IPC
    DummyIpcMediaParam ipc_param = {0};
    ipc_param.service_id = service_id;
    if (cmd_type == LV_STREAM_CMD_LIVE) {
        dummy_ipc_live(DUMMY_IPC_MEDIA_START, &ipc_param);
        g_live_service_id = service_id;
    } else if (cmd_type == LV_STREAM_CMD_STORAGE_RECORD_BY_FILE){
        memcpy(&ipc_param.source_file, param->file_name, DummyIpcMediaFileLen);
        ipc_param.base_time_ms = param->timestamp_base * 1000;
        ipc_param.seek_timestamp_ms = param->timestamp_offset * 1000;
        dummy_ipc_vod_by_file(DUMMY_IPC_MEDIA_PAUSE, &ipc_param);
        dummy_ipc_vod_by_file(DUMMY_IPC_MEDIA_START, &ipc_param);
        dummy_ipc_vod_by_file(DUMMY_IPC_MEDIA_SEEK, &ipc_param);
        g_vod_service_id = service_id;
    } else if (cmd_type == LV_STREAM_CMD_STORAGE_RECORD_BY_UTC_TIME) {
        ipc_param.seek_timestamp_ms = (param->seek_time - ipc_param.base_time_ms) * 1000;
        dummy_ipc_vod_by_utc(DUMMY_IPC_MEDIA_SEEK, &ipc_param);
        g_vod_service_id = service_id;
    } else {
        printf("Stream type is not support\n");
        return -1;
    }

    lv_video_param_s video_param;
    lv_audio_param_s audio_param;
    memset(&video_param, 0, sizeof(lv_video_param_s));
    memset(&audio_param, 0, sizeof(lv_audio_param_s));
    double duration = 0;
    if (cmd_type == LV_STREAM_CMD_LIVE) {
        dummy_ipc_get_live_media_param(&video_param, &audio_param, &duration);
    } else {
        dummy_ipc_vod_get_media_param(&video_param, &audio_param, &duration);
        if (cmd_type == LV_STREAM_CMD_STORAGE_RECORD_BY_UTC_TIME) {
            duration = param->stop_time - param->start_time;
        }
    }

    lv_stream_send_config(service_id, 1000, duration, &video_param, &audio_param);

#endif // DUMMY_IPC

    return 0;
}

static int stop_push_streaming_cb(int service_id) {
    printf("stopPushStreamingCallback:%d\n", service_id);

#ifdef DUMMY_IPC
    DummyIpcMediaParam ipc_param = {0};
    if (service_id == g_live_service_id) {
        g_live_service_id = -1;
        dummy_ipc_live(DUMMY_IPC_MEDIA_STOP, &ipc_param);
    } else if (service_id == g_vod_service_id) {
        g_vod_service_id = -1;
        dummy_ipc_vod_by_file(DUMMY_IPC_MEDIA_STOP, &ipc_param);
    }
#endif // DUMMY_IPC

    return 0;
}

/**
 * linkvisual_demo_init负责LinkVisual相关回调的注册。
 * 1. 开发者需要注册相关认证信息和回调函数，并进行初始化
 * 2. 根据回调函数中的信息，完成音视频流的上传
 * 3. 当前文件主要用于打印回调函数的命令，并模拟IPC的行为进行了音视频的传输
 */
static int g_init = 0;
int linkvisual_client_init(const char *product_key,
                           const char *device_name,
                           const char *device_secret,
                           lv_log_level_e log_level) {
    printf("before init linkvisual\n");

    if (g_init) {
        printf("linkvisual_demo_init already init\n");
        return 0;
    }

    if (!(product_key && device_name && device_secret)) {
        printf("linkvisual_demo_init failed\n");
        return -1;
    }

    lv_config_s config;
    memset(&config, 0, sizeof(lv_config_s));

    //设备三元组
    string_safe_copy(config.product_key, product_key, PRODUCT_KEY_LEN);
    string_safe_copy(config.device_name, device_name, DEVICE_NAME_LEN);
    string_safe_copy(config.device_secret, device_secret, DEVICE_SECRET_LEN);

    /* SDK的日志配置 */
    config.log_level = log_level;
    config.log_dest = LV_LOG_DESTINATION_STDOUT;

    /* 码流路数限制 */
    config.storage_record_mode = LV_STREAM_CMD_STORAGE_RECORD_BY_UTC_TIME;
    config.live_p2p_num = 4;
    config.storage_record_source_num = 1;

    /* 码流检查功能 */
    config.stream_auto_check = 1;
#if 1 /* 码流保存为文件功能，按需使用 */
    config.stream_auto_save = 0;
#else
    config.stream_auto_save = 1;
    char *path = "/tmp/";
    memcpy(config.stream_auto_save_path, path, strlen(path));
#endif

    //音视频推流服务
    config.start_push_streaming_cb = start_push_streaming_cb;
    config.stop_push_streaming_cb = stop_push_streaming_cb;
    config.on_push_streaming_cmd_cb = on_push_streaming_cmd_cb;

    //语音对讲服务
    config.start_voice_intercom_cb = start_voice_intercom_cb;
    config.stop_voice_intercom_cb = stop_voice_intercom_cb;
    config.voice_intercom_receive_metadata_cb = voice_intercom_receive_metadata_cb;
    config.voice_intercom_receive_data_cb = voice_intercom_receive_data_cb;

    //获取存储录像录像列表
    config.query_storage_record_cb = query_storage_record_cb;
    config.query_storage_record_by_month_cb = query_storage_record_by_month_cb;

    //触发设备抓图
    config.trigger_pic_capture_cb = trigger_pic_capture_cb;

    //触发设备录像
    config.trigger_record_cb = trigger_record_cb;

    //先准备好LinkVisual相关资源
    int ret = lv_init(&config);
    if (ret < 0) {
        printf("lv_init failed, result = %d\n", ret);
        return -1;
    }

    g_init = 1;

    printf("after init linkvisual\n");

#ifdef DUMMY_IPC
    g_vod_mod = config.storage_record_mode;
#endif

    return 0;
}

void linkvisual_client_destroy() {
    printf("before destroy linkvisual\n");

    if (!g_init) {
        printf("linkvisual_demo_destroy is not init\n");
        return;
    }

    lv_destroy();

    printf("after destroy linkvisual\n");
}

void linkvisual_client_video_handler(int service_id, lv_video_format_e format, unsigned char *buffer, unsigned int buffer_size,
                                     unsigned int present_time, int nal_type) {
    //printf("video service_id:%d, format:%d, present_time:%u nal_type:%d size %u\n", service_id, format, present_time, nal_type, buffer_size);
    lv_stream_send_video(service_id, format, buffer, buffer_size, nal_type, present_time);
}

/* 音频帧数据回调 */
void linkvisual_client_audio_handler(int service_id,
                                     unsigned char *buffer,
                                     unsigned int buffer_size,
                                     unsigned int present_time) {
    //printf("audio service_id:%d, present_time:%u buffer_size:%u\n", service_id, present_time, buffer_size);
    lv_stream_send_audio(service_id, buffer, buffer_size, present_time);
    if (g_voice_intercom_service_id != -1 && !g_voice_intercom_upstream_disable) {
        printf("voice intercom send data, size:%d\n", buffer_size);
        lv_voice_intercom_send_audio(g_voice_intercom_service_id, buffer, buffer_size, present_time);
    }
}

/* 报警图片数据回调 */
void linkvisual_client_picture_handler(unsigned char *buffer,
                                       unsigned int buffer_size,
                                       lv_event_type_e type,
                                       const char *id) {
    lv_post_alarm_image((char *)buffer, buffer_size, type, id);
}

void linkvisual_client_control_test() {
    /* 一个十分简易的命令行解析程序，请按照示例命令来使用 */
#define CMD_LINE_MAX (128)
    char str[CMD_LINE_MAX] = {0};
    gets(str);

    char *key = strtok(str, " ");
    if (!key) {
        return;
    }
    char *value = strtok(NULL, " ");
    if (!value) {
        return;
    }

    if (!strcmp(key, "-l")) {// 日志级别设置，使用示例： -l 3
        int log_level = value[0] - '0';
        lv_control(LV_CONTROL_LOG_LEVEL, log_level);
    } else if (!strcmp(key, "-c")) {// 码流自检功能，使用示例： -c 0
        int check = value[0] - '0';
        lv_control(LV_CONTROL_STREAM_AUTO_CHECK, check);
    } else if (!strcmp(key, "-s")) {// 码流自动功能，使用示例： -s 0
        int save = value[0] - '0';
        const char *path = "/tmp/";// 需要打开时，使用默认的保存路径
        lv_control(LV_CONTROL_STREAM_AUTO_SAVE, save, path);
    } else {
        return;
    }
}
