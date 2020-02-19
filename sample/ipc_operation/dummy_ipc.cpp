/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

/**
 * 文件用于模拟IPC的功能，仅用于demo测试使用
 * 开发者不需要过多关注，实际产品不会使用到该部分代码
 */

#include "dummy_ipc.h"

#include <string.h>
#include <stdio.h>
#include <iostream>
#include <vector>

#include "media_parser.h"
#include "picture_parser.h"
#include "property_parser.h"
#include "demo.h"

const unsigned int kLiveMaxNum = 2;

std::string g_image_file = "1.jpg";
std::string g_default_files[2] = {"aac_h265_768", "aac_h265_640"};
std::vector<std::string> g_live_media_file_group;
std::vector<std::string> g_vod_media_file_group;

static bool g_init = false;
static DummyIpcConfig g_config = {0};

static MediaParse *g_live_current_media_parser = nullptr;
static MediaParse *g_live_main_media_parser = nullptr;
static MediaParse *g_live_sub_media_parser = nullptr;
static MediaParse *g_vod_media_parser = nullptr;
static PictureParser *g_picture_parser = nullptr;

static void videoBufferHandler(lv_video_format_e format, unsigned char *buffer, unsigned int buffer_size,
                                   unsigned int timestamp_ms, int nal_type, int service_id) {
    //printf("service_id:%d, present_time:%u nal_type:%d \n", service_id, timestamp_ms, nal_type);
    if (service_id > 0) { //service_id再往外抛出数据
        g_config.video_handler(service_id, format, buffer, buffer_size, timestamp_ms, nal_type);
    }
}

static void audioBufferHandler(unsigned char *buffer, unsigned int buffer_size,
                                   unsigned int timestamp_ms, int service_id) {
    //printf("service_id:%d, present_time:%u  buffer_size:%u\n", service_id, timestamp_ms, buffer_size);
    if (service_id > 0) {
        g_config.audio_handler(service_id, buffer, buffer_size, timestamp_ms);
    }
}

static void pictureBufferHandler(unsigned char *buffer, unsigned int buffer_len, const char *id) {
    if (id) {
        g_config.picture_handler(buffer, buffer_len, LV_TRIGGER_PIC, id);
    } else {
        g_config.picture_handler(buffer, buffer_len, LV_EVENT_MOVEMENT, id);
    }
}

static void propertyHandler(const std::string &key, const std::string &value) {
    g_config.set_property_handler(key.c_str(), value.c_str());
}


static bool checkFileExist(const std::string &file_name) {
    FILE *fp = nullptr;
    fp = fopen(file_name.c_str(), "r");
    if (!fp) {
        printf("Check files failed: %s\n", file_name.c_str());
        return false;
    }
    fclose(fp);
    return true;
}

static bool checkStreamFileExist(const std::string &stream_prefix) {
    std::string stream_index = stream_prefix + ".index";
    std::string stream_param = stream_prefix + ".meta";

    return checkFileExist(stream_index) &&
        checkFileExist(stream_param) &&
        checkFileExist(stream_prefix);
}

static bool checkStreamFileGroupExist(const std::vector<std::string> &stream_group) {
    for (auto &search :stream_group) {
        if (!checkStreamFileExist(search)) {
            return false;
        }
    }
    return true;
}

int dummy_ipc_start(const DummyIpcConfig *config) {
    if (g_init) {
        return 0;
    }

    if (!(config &&
        config->set_property_handler &&
        config->picture_handler &&
        config->video_handler &&
        config->audio_handler)) {
        return -1;
    }
    memcpy(&g_config, config, sizeof(DummyIpcConfig));

    unsigned int i = 0;
    if (config->live_num <= 1 || !config->live_source) {
        g_live_media_file_group.push_back(g_default_files[0]);
        g_live_media_file_group.push_back(g_default_files[1]);
    } else {
        for (i = 0; i < kLiveMaxNum && i < config->live_num; i++) {
            g_live_media_file_group.push_back(config->live_source[i].source_file);
        }
    }

    if (config->vod_num <= 1 || !config->vod_source) {
        g_vod_media_file_group.push_back(g_default_files[0]);
        g_vod_media_file_group.push_back(g_default_files[1]);
    } else {
        for (i = 0; i < config->vod_num; i++) {
            g_vod_media_file_group.push_back(config->vod_source[i].source_file);
        }
    }

    g_image_file = (config->picture_source == nullptr)?g_image_file:std::string(config->picture_source);

    if (!(checkStreamFileGroupExist(g_live_media_file_group) &&
        checkStreamFileGroupExist(g_vod_media_file_group) &&
        checkFileExist(g_image_file))) {
        return -1;
    }

    g_live_main_media_parser = new MediaParse(videoBufferHandler, audioBufferHandler);
    g_live_sub_media_parser = new MediaParse(videoBufferHandler, audioBufferHandler);
    g_vod_media_parser = new MediaParse(videoBufferHandler, audioBufferHandler);
    g_picture_parser = new PictureParser(pictureBufferHandler);

    /* 直播主子码流和抓图默认打开 */
    g_live_main_media_parser->Start(0, g_live_media_file_group[0], true, 0);
    g_live_sub_media_parser->Start(0, g_live_media_file_group[1], true, 0);
    g_picture_parser->Start(g_config.picture_interval_s, g_image_file);
    g_live_current_media_parser = g_live_main_media_parser;//对外输出默认使用主码流

    /* 设置属性设置的回调 */
    PropertyParser::SetHandler(propertyHandler);
    g_init = true;

    return 0;
}

int dummy_ipc_live(DummyIpcMediaCmd cmd, const DummyIpcMediaParam *param) {
    if (!g_init) {
        return -1;
    }
    if (!param) {
        return -1;
    }

    /* 直播流一直处于打开状态，仅在SDK的连接建立时，才对外抛出数据 */
    if (cmd == DUMMY_IPC_MEDIA_START) {
        g_live_current_media_parser->SetServiceId(param->service_id);
        g_live_current_media_parser->RequestIFrame();
    } else if (cmd == DUMMY_IPC_MEDIA_STOP){
        g_live_current_media_parser->SetServiceId(0);
    } else if (cmd == DUMMY_IPC_MEDIA_REQUEST_I_FRAME) {
        g_live_current_media_parser->RequestIFrame();
    } else if (cmd == DUMMY_IPC_MEDIA_CHANGE_STREAM) {
        int service_id = g_live_current_media_parser->GetServiceId();
        g_live_current_media_parser->SetServiceId(0);
        if (param->stream_type == 0) {
            g_live_current_media_parser = g_live_sub_media_parser;
        } else {
            g_live_current_media_parser = g_live_main_media_parser;
        }
        g_live_current_media_parser->Pause(true);
        g_live_current_media_parser->SetServiceId(service_id);
        g_live_current_media_parser->RequestIFrame();
        g_live_current_media_parser->Pause(false);
    }

    return 0;
}

int dummy_ipc_vod_by_file(DummyIpcMediaCmd cmd, const DummyIpcMediaParam *param) {
    if (!g_init) {
        return -1;
    }
    if (!param) {
        return -1;
    }

    if (cmd == DUMMY_IPC_MEDIA_START) {
        if (g_vod_media_parser->GetStatus()){
            g_vod_media_parser->Stop();
        }
        //g_vod_media_parser->Pause(false);
        g_vod_media_parser->Start(param->service_id, param->source_file, false, param->base_time_ms);
    } else if (cmd == DUMMY_IPC_MEDIA_STOP){
        g_vod_media_parser->Stop();
    } else if (cmd == DUMMY_IPC_MEDIA_PAUSE){
        g_vod_media_parser->Pause(true);
    } else if (cmd == DUMMY_IPC_MEDIA_UNPAUSE){
        g_vod_media_parser->Pause(false);
    } else if (cmd == DUMMY_IPC_MEDIA_SEEK){
        g_vod_media_parser->Seek(param->seek_timestamp_ms);
    }
    return 0;
}

int dummy_ipc_report_vod_list(unsigned int start_time,
                              unsigned int stop_time,
                              int num,
                              const char *id,
                              int (*on_complete)(int num,
                                                 const char *id,
                                                 const lv_query_storage_record_response_s *response)) {
    if (!g_init) {
        return -1;
    }
    int answer_num = g_vod_media_file_group.size();
    auto *response = new lv_query_storage_record_response_s[answer_num];
    memset(response, 0, sizeof(lv_query_storage_record_response_s) * answer_num);
    double duration = 0;
    for (int i = 0; i < answer_num; i ++) {
        MediaParse::GetDuration(g_vod_media_file_group[i], duration);
        response[i].file_size = 0;
        response[i].record_type = LV_STORAGE_RECORD_INITIATIVE;
        snprintf(response[i].file_name, 64, g_vod_media_file_group[i].c_str(), i);//注意不要溢出
        if (i == 0) {
            response[i].start_time = (int)start_time;
            response[i].stop_time = (int)start_time + (int)duration;
        } else {
            response[i].start_time = response[i - 1].stop_time;
            response[i].stop_time = response[i - 1].stop_time + (int)duration;
        }

    }
    int result = on_complete(answer_num, id, response);
    printf("dummy_ipc_report_vod_list result: %d\n", result);
    delete[] response;
    return 0;
}

int dummy_ipc_set_property(const char *key, const char *value) {
    if (!g_init) {
        return -1;
    }

    printf("Set property :%s %s\n", key, value);
    PropertyParser::SetProperty(key== nullptr?"":key, value== nullptr?"":value);
    return 0;
}

void dummy_ipc_get_all_property() {
    if (!g_init) {
        return;
    }

    PropertyParser::GetAllProperty();
}

int dummy_ipc_capture(const char *id) {
    if (!g_init) {
        return -1;
    }

    return g_picture_parser->Trigger(id);
}

void dummy_ipc_stop() {
    if (g_init) {
        g_init = false;
        g_live_main_media_parser->Stop();
        g_live_sub_media_parser->Stop();
        g_vod_media_parser->Stop();
        g_picture_parser->Stop();
        delete g_live_main_media_parser;
        delete g_live_sub_media_parser;
        delete g_vod_media_parser;
        delete g_picture_parser;
    }
}

int dummy_ipc_get_live_media_param(lv_video_param_s *vparam, lv_audio_param_s *aparam, double *duration) {
    if (!g_init || !vparam || !aparam || !duration) {
        return -1;
    }

    g_live_current_media_parser->GetParam(*vparam, *aparam, *duration);

    return 0;
}

int dummy_ipc_vod_get_media_param(lv_video_param_s *vparam, lv_audio_param_s *aparam, double *duration) {
    if (!g_init || !vparam || !aparam || !duration) {
        return -1;
    }

    g_vod_media_parser->GetParam(*vparam, *aparam, *duration);

    return 0;
}

int dummy_ipc_vod_by_utc(DummyIpcMediaCmd cmd, const DummyIpcMediaParam *param) {
    if (!g_init) {
        return -1;
    }
    if (!param) {
        return -1;
    }

    if (cmd == DUMMY_IPC_MEDIA_STOP){
        g_vod_media_parser->Stop();
        return 0;
    } else if (cmd == DUMMY_IPC_MEDIA_PAUSE){
        g_vod_media_parser->Pause(true);
        return 0;
    } else if (cmd == DUMMY_IPC_MEDIA_UNPAUSE){
        g_vod_media_parser->Pause(false);
        return 0;
    } else if (cmd == DUMMY_IPC_MEDIA_SEEK) {
        double duration = 0;
        unsigned int timestamp_file_ms = 0;
        unsigned int i;
        for (i = 0; i < g_vod_media_file_group.size(); i ++) {
            MediaParse::GetDuration(g_vod_media_file_group[i], duration);
            timestamp_file_ms += (unsigned int)duration * 1000;
            if (param->seek_timestamp_ms <= timestamp_file_ms) {
                break;
            }
        }
        if (param->seek_timestamp_ms > timestamp_file_ms) {
            return 0;
        }

        /* 第一次打开 */
        if (!g_vod_media_parser->GetStatus()) {
            g_vod_media_parser->Pause(true);
            g_vod_media_parser->Start(param->service_id, g_vod_media_file_group[i], false,  (timestamp_file_ms - (unsigned int)duration * 1000));
            g_vod_media_parser->Seek(param->seek_timestamp_ms - (timestamp_file_ms - (unsigned int)duration * 1000));
        } else {
            g_vod_media_parser->Pause(true);//先暂停当前流
            if (g_vod_media_file_group[i] != g_vod_media_parser->GetStreamPrefix()) {
                g_vod_media_parser->Stop();
                g_vod_media_parser->Start(param->service_id, g_vod_media_file_group[i], false,  (timestamp_file_ms - (unsigned int)duration * 1000));
            }
            g_vod_media_parser->Seek(param->seek_timestamp_ms - (timestamp_file_ms - (unsigned int)duration * 1000));
            g_vod_media_parser->Pause(false);
        }

    }

    return 0;
}


