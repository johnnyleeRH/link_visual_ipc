/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#include "linkkit_client.h"

#include <stdio.h>
#include <iot_export.h>

#include "link_visual_ipc.h"
#include "link_visual_def.h"
#include "iot_export_linkkit.h"
#include "iot_export.h"
#include "iot_import.h"
#include "cJSON.h"
#include "demo.h"

#ifdef DUMMY_IPC
#include "dummy_ipc.h"
#endif // DUMMY_IPC

static int g_master_dev_id = -1;
static int g_running = 0;
static void *g_thread = NULL;
static int g_thread_not_quit = 0;
static int g_connect = 0;


/* 这个字符串数组用于说明LinkVisual需要处理的物模型服务 */
static char *link_visual_service[] = {
    "TriggerPicCapture",//触发设备抓图
    "StartVoiceIntercom",//开始语音对讲
    "StopVoiceIntercom",//停止语音对讲
    "StartVod",//开始录像观看
    "StartVodByTime",//开始录像按时间观看
    "StopVod",//停止录像观看
    "QueryRecordList",//查询录像列表
    "StartP2PStreaming",//开始P2P直播
    "StartPushStreaming",//开始直播
    "StopPushStreaming",//停止直播
    "QueryRecordTimeList",//按月查询卡录像列表
};

static int user_connected_event_handler(void) {
    printf("Cloud Connected\n");
    g_connect = 1;

    /**
     * Linkkit连接后，上报设备的属性值。
     * 当APP查询设备属性时，会直接从云端获取到设备上报的属性值，而不会下发查询指令。
     * 对于设备自身会变化的属性值（存储使用量等），设备可以主动隔一段时间进行上报。
     */
#ifdef DUMMY_IPC
    /* 开机获取和上报所有属性值 */
    dummy_ipc_get_all_property();
#endif // DUMMY_IPC

    /* Linkkit连接后，查询下ntp服务器的时间戳，用于同步服务器时间。查询结果在user_timestamp_reply_handler中 */
    IOT_Linkkit_Query(g_master_dev_id, ITM_MSG_QUERY_TIMESTAMP, NULL, 0);

    // linkkit上线消息同步给LinkVisual
    lv_linkkit_adapter(LV_LINKKIT_ADAPTER_TYPE_CONNECTED, NULL);
    return 0;
}

static int user_disconnected_event_handler(void) {
    printf("Cloud Disconnected\n");
    g_connect = 0;

    return 0;
}

static int user_service_request_handler(const int devid, const char *id, const int id_len,
                                            const char *serviceid, const int serviceid_len,
                                            const char *request, const int request_len,
                                            char **response, int *response_len) {
    printf("Service Request Received, Devid: %d, ID %.*s, Service ID: %.*s, Payload: %s\n",
             devid, id_len, id, serviceid_len, serviceid, request);

    /* 部分物模型服务消息由LinkVisual处理，部分需要自行处理。 */
    int link_visual_process = 0;
    for (unsigned int i = 0; i < sizeof(link_visual_service)/sizeof(link_visual_service[0]); i++) {
        /* 这里需要根据字符串的长度来判断 */
        if (!strncmp(serviceid, link_visual_service[i], strlen(link_visual_service[i]))) {
            link_visual_process = 1;
            break;
        }
    }

    if (link_visual_process) {
        /* ISV将某些服务类消息交由LinkVisual来处理，不需要处理response */
        lv_linkkit_adapter_property_s in = {0};
        in.dev_id = devid;
        in.id = id;
        in.id_len = id_len;
        in.service_id = serviceid;
        in.service_id_len = serviceid_len;
        in.request = request;
        in.request_len = request_len;
        int ret = lv_linkkit_adapter(LV_LINKKIT_ADAPTER_TYPE_TSL_SERVICE, &in);
        if (ret < 0) {
            printf("LinkVisual process service request failed, ret = %d\n", ret);
            return -1;
        }
    } else {
        /* 非LinkVisual处理的消息示例 */
        if (!strncmp(serviceid, "PTZActionControl", (serviceid_len > 0)?serviceid_len:0)) {
            cJSON *root = cJSON_Parse(request);
            if (root == NULL) {
                printf("JSON Parse Error\n");
                return -1;
            }

            cJSON *child = cJSON_GetObjectItem(root, "ActionType");
            if (!child) {
                printf("JSON Parse Error\n");
                cJSON_Delete(root);
                return -1;
            }
            int action_type = child->valueint;

            child = cJSON_GetObjectItem(root, "Step");
            if (!child) {
                printf("JSON Parse Error\n");
                cJSON_Delete(root);
                return -1;
            }
            int step = child->valueint;

            cJSON_Delete(root);
            printf("PTZActionControl %d %d\n", action_type, step);
        }
    }

    return 0;
}

static int user_property_set_handler(const int devid, const char *request, const int request_len) {
    printf("Property Set Received, Devid: %d, Request: %s\n", devid, request);

#ifdef DUMMY_IPC
    cJSON *root = cJSON_Parse(request);
    if (!root) {
        printf("value parse error\n");
        return -1;
    }
    cJSON *child = root->child;

    /* 设置属性 */
    if (child->type == cJSON_Number) {
        char value[32] = {0};
        snprintf(value, 32, "%d", child->valueint);
        dummy_ipc_set_property(child->string, value);
    } else if (child->type == cJSON_String) {
        dummy_ipc_set_property(child->string, child->valuestring);
    }

    /* 切换分辨率指令处理 */
    if (!strcmp(child->string, "StreamVideoQuality")) {
        DummyIpcMediaParam param = {0};
        param.stream_type = child->valueint;
        dummy_ipc_live(DUMMY_IPC_MEDIA_CHANGE_STREAM, &param);
    }

    cJSON_Delete(root);

    int res = IOT_Linkkit_Report(g_master_dev_id, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)request, request_len);
    printf("Post Property Message ID: %d\n", res);

#endif // DUMMY_IPC
    return 0;
}

static int user_timestamp_reply_handler(const char *timestamp) {
    printf("Current Timestamp: %s \n", timestamp);//时间戳为字符串格式，单位：毫秒

    return 0;
}

static int user_fota_handler(int type, const char *version) {
    char buffer[1024] = {0};
    int buffer_length = 1024;

    if (type == 0) {
        printf("New Firmware Version: %s\n", version);

        IOT_Linkkit_Query(g_master_dev_id, ITM_MSG_QUERY_FOTA_DATA, (unsigned char *)buffer, buffer_length);
    }

    return 0;
}

static int user_event_notify_event_handler(const int devid, const char *request, const int request_len) {
    printf("Event Notify Received, Devid: %d, Request: %s \n", devid, request);
    return 0;
}

static int user_link_visual_handler(const int devid, const char *payload,
                                    const int payload_len) {
    /* Linkvisual自定义的消息，直接全交由LinkVisual来处理 */
    if (payload == NULL || payload_len == 0) {
        return 0;
    }

    lv_linkkit_adapter_property_s in = {0};
    in.request = payload;
    in.request_len = payload_len;
    int ret = lv_linkkit_adapter(LV_LINKKIT_ADAPTER_TYPE_LINK_VISUAL, &in);
    if (ret < 0) {
        printf("LinkVisual process service request failed, ret = %d\n", ret);
        return -1;
    }

    return 0;
}

static void *user_dispatch_yield(void *args) {
    while (g_running) {
        IOT_Linkkit_Yield(10);
    }

    g_thread_not_quit = 0;
    return NULL;
}

int linkkit_client_start(const char *product_key,
                         const char *product_secret,
                         const char *device_name,
                         const char *device_secret) {
    printf("Before start linkkit\n");

    if (g_running) {
        printf("Already running\n");
        return 0;
    }

    /* 设置调试的日志级别 */
    IOT_SetLogLevel(IOT_LOG_DEBUG);

    /* 注册链接状态的回调 */
    IOT_RegisterCallback(ITE_CONNECT_SUCC, user_connected_event_handler);
    IOT_RegisterCallback(ITE_DISCONNECTED, user_disconnected_event_handler);

    /* 注册消息通知 */
    IOT_RegisterCallback(ITE_LINK_VISUAL, user_link_visual_handler);//linkvisual自定义消息
    IOT_RegisterCallback(ITE_SERVICE_REQUST, user_service_request_handler);//物模型服务类消息
    IOT_RegisterCallback(ITE_PROPERTY_SET, user_property_set_handler);//物模型属性设置
    IOT_RegisterCallback(ITE_TIMESTAMP_REPLY, user_timestamp_reply_handler);//NTP时间
    IOT_RegisterCallback(ITE_FOTA, user_fota_handler);//固件OTA升级事件
    IOT_RegisterCallback(ITE_EVENT_NOTIFY, user_event_notify_event_handler);

    iotx_linkkit_dev_meta_info_t master_meta_info;
    memset(&master_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));
    string_safe_copy(master_meta_info.product_key, product_key, PRODUCT_KEY_MAXLEN - 1);
    string_safe_copy(master_meta_info.product_secret, product_secret, PRODUCT_SECRET_MAXLEN - 1);
    string_safe_copy(master_meta_info.device_name, device_name, DEVICE_NAME_MAXLEN - 1);
    string_safe_copy(master_meta_info.device_secret, device_secret, DEVICE_SECRET_MAXLEN - 1);

    /* 选择服务器地址，当前使用上海服务器 */
    int domain_type = IOTX_CLOUD_REGION_SHANGHAI;
    IOT_Ioctl(IOTX_IOCTL_SET_DOMAIN, (void *)&domain_type);

    /* 动态注册 */
    int dynamic_register = 0;
    IOT_Ioctl(IOTX_IOCTL_SET_DYNAMIC_REGISTER, (void *)&dynamic_register);

    /* 创建linkkit资源 */
    g_master_dev_id = IOT_Linkkit_Open(IOTX_LINKKIT_DEV_TYPE_MASTER, &master_meta_info);
    if (g_master_dev_id < 0) {
        printf("IOT_Linkkit_Open Failed\n");
        return -1;
    }

    /* 连接到服务器 */
    int ret = IOT_Linkkit_Connect(g_master_dev_id);
    if (ret < 0) {
        printf("IOT_Linkkit_Connect Failed\n");
        return -1;
    }

    /* 创建线程，线程用于轮训消息 */
    g_running = 1;
    g_thread_not_quit = 1;
    ret = HAL_ThreadCreate(&g_thread, user_dispatch_yield, NULL, NULL, NULL);
    if (ret != 0) {//!= 0 而非 < 0
        printf("HAL_ThreadCreate Failed, ret = %d\n", ret);
        g_running = 0;
        g_thread_not_quit = 0;
        IOT_Linkkit_Close(g_master_dev_id);
        return -1;
    }

    /* 等待linkkit链接成功（demo做了有限时长的等待，实际产品中，可设置为在网络可用时一直等待） */
    for(int i = 0; i < 100; i++) {
        if(!g_connect) {
            HAL_SleepMs(200);
        } else {
            break;
        }
    }

    if (!g_connect) {
        printf("linkkit connect Failed\n");
        linkkit_client_destroy();
        return -1;
    }

    printf("After start linkkit\n");

    return 0;
}

void linkkit_client_destroy() {
    printf("Before destroy linkkit\n");
    if (!g_running) {
        return;
    }
    g_running = 0;

    /* 等待线程退出，并释放线程资源，也可用分离式线程，但需要保证线程不使用linkkit资源后，再去释放linkkit */
    while (g_thread_not_quit) {
        HAL_SleepMs(20);
    }
    if (g_thread) {
        HAL_ThreadDelete(g_thread);
        g_thread = NULL;
    }

    IOT_Linkkit_Close(g_master_dev_id);
    g_master_dev_id = -1;

    printf("After destroy linkkit\n");
}

/* 属性设置回调 */
void linkkit_client_set_property_handler(const char *key, const char *value) {
    /**
     * 当收到属性设置时，开发者需要修改设备配置、改写已存储的属性值，并上报最新属性值。demo只上报了最新属性值。
     */
    char result[1024] = {0};
    snprintf(result, 1024, "{\"%s\":\"%s\"}", key, value);
    IOT_Linkkit_Report(0, ITM_MSG_POST_PROPERTY, (unsigned char *)result, strlen(result));
}
