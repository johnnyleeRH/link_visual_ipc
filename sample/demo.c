/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <unistd.h>
#include <stdio.h>

#include "link_visual_def.h"
#include "link_visual_ipc.h"
#include "linkkit_client.h"
#include "demo.h"
#include "linkvisual_client.h"
#ifdef DUMMY_IPC
/* 0. 虚拟IPC功能打开 */
#include "dummy_ipc.h"
#endif
/**
 * IPC使用SDK完成功能时，需要具备以下基本能力：
 * 1. 直播
 *  a. 开启直播功能，输出视频帧、音频帧
 *  b. 开启时的第一个视频帧为I帧，后续按一定时间间隔持续输出I帧、P帧，不支持B帧
 *  c. 响应强制I帧指令，尽快输出I帧
 *  d. 切换分辨率、切换码流时，第一个帧为I帧
 * 2. 录像点播
 *  a. 读取录像文件，并输出视频帧、音频帧
 *  b. 开启时的第一个视频帧为I帧，后续按一定时间间隔持续输出I帧、P帧，不支持B帧
 *  c. 支持文件内按时间戳搜索，且搜索后第一个视频帧为I帧
 *  d. 暂停、恢复功能
 *  e. 按月、天、特定时间段查询录像文件
 * 3. 语音对讲
 *  a. 开启语音对讲功能，输出音频帧（音频帧一般与直播的音频帧相同）
 *  b. 解码从对端收到的音频帧
 * 4. 报警事件生成
 *  a. 侦测一定的环境变化（如人物移动、声音变化等），产生报警事件
 *  b. 产生报警事件时，附带一定的视音频数据，如图片
 * 5. 属性设置
 *  a. 存在一个掉电不丢失的存储空间，存储属性设置
 *  b. 开机时，主动上报存储的属性设置
 *  c. 响应对端的属性设置指令，并改写存储空间
 * 6. PTZ功能
 *  a. 拥有上、下、左、右四个方向的旋转能力
 *  b. 能控制每次转动的具体角度
 *  c. 能将摄像头重置至原始位置
 */


/**
 * main是程序入口。
 * 主要包括两部分功能：linkkit提供的命令通道功能；linkvisual提供的音视频传输功能。
 * 生命周期说明：
 * 1. 初始化linkvisual的资源
 * 2. 初始化linkkit的资源，并linkkit长连接到服务器
 * 3. linkkit接收服务器命令，转交给linkvisual，linkvisual建立音视频连接，用于开发者发送视音频
 * 4. linkkit长连接断开，并释放资源
 * 5. linkvisual断开音视频连接，并释放资源
 * 另外，demo模拟了IPC的音视频推流、配置信息读写等过程，用宏DUMMY_IPC分隔开。
 */
int main(int argc, char* argv[])
{
    char product_key[PRODUCT_KEY_LEN + 1] = {0};
    char device_name[DEVICE_NAME_LEN + 1] = {0};
    char device_secret[DEVICE_SECRET_LEN + 1] = {0};
    char product_secret[PRODUCT_SECRET_LEN + 1] = {0};//产品秘钥只在配网过程中需要
    int log_level = LV_LOG_DEBUG;//默认debug日志级别

    /**
     * 可选从外部读取四元组信息和日志级别，读取方式为:
     * ./exe -p product_key -n device_name -s device_secret -t product_secret -l log_level
     * 外部读取参数对提高调试效率有很大帮助，建议开发者在对接过程中，将此部分代码集成到自身的程序中
     */
    int ch;
    while ((ch = getopt(argc, argv, "p:n:s:l:")) != -1) {
        switch (ch) {
            case 'p': string_safe_copy(product_key, optarg, PRODUCT_KEY_LEN);break;
            case 'n': string_safe_copy(device_name, optarg, DEVICE_NAME_LEN);break;
            case 's': string_safe_copy(device_secret, optarg, DEVICE_SECRET_LEN);break;
            case 't': string_safe_copy(product_secret, optarg, PRODUCT_SECRET_LEN);break;
            case 'l': log_level = (optarg[0] - '0');
                      log_level = (log_level >= LV_LOG_ERROR && log_level <= LV_LOG_VERBOSE)?log_level:LV_LOG_DEBUG;
                      break;
            default: break;
        }
    }
    printf("PRODUCT_KEY            :%s\n", product_key);
    printf("DEVICE_NAME            :%s\n", device_name);
    printf("DEVICE_SECRET          :%s\n", device_secret);
    printf("Log level              :%d\n", log_level);

    int ret = 0;
#ifdef DUMMY_IPC
    /* 0.初始化虚拟IPC的功能 */
    DummyIpcConfig dummy_ipc_config = {0};
    dummy_ipc_config.picture_interval_s = 600;//设置虚拟IPC生成图片事件的间隔
    dummy_ipc_config.audio_handler = linkvisual_client_audio_handler;//设置虚拟IPC生成音频数据的输入函数
    dummy_ipc_config.video_handler = linkvisual_client_video_handler;//设置虚拟IPC生成视频数据的输入函数
    dummy_ipc_config.picture_handler = linkvisual_client_picture_handler;//设置虚拟IPC生成图片事件数据的输入函数
    dummy_ipc_config.set_property_handler = linkkit_client_set_property_handler;//设置虚拟IPC生成属性设置的输入函数

    ret = dummy_ipc_start(&dummy_ipc_config);
    if (ret < 0) {
        printf("Start dummy ipc failed\n");
        return -1;
    }
#endif // DUMMY_IPC

    /* 1. 初始化LinkVisual的资源 */
    ret = linkvisual_client_init(product_key, device_name, device_secret, (lv_log_level_e) log_level);
    if (ret < 0) {
        printf("linkvisual_client_init failed\n");
#ifdef DUMMY_IPC
        dummy_ipc_stop();
#endif // DUMMY_IPC
        return -1;
    }

    /* 2. 初始化linkkit的资源，并长连接到服务器 */
    ret = linkkit_client_start(product_key, product_secret, device_name, device_secret);
    if (ret < 0) {
        printf("linkkit_client_start failed\n");
        linkvisual_client_destroy();
#ifdef DUMMY_IPC
        dummy_ipc_stop();
#endif // DUMMY_IPC
        return -1;
    }

    /* 3. 运行，等待服务器命令 */
    while(1) {
#if 0
        linkvisual_client_control_test();//读取命令行参数，来控制SDK日志级别等行为，仅用于调试
#endif
        usleep(1000 * 500);
    }

    /* 4. linkkit长连接断开，并释放资源 */
    linkkit_client_destroy();

    /* 5. LinkVisual断开音视频连接，并释放资源 */
    linkvisual_client_destroy();

#ifdef DUMMY_IPC
    /* 停止虚拟IPC的功能 */
    dummy_ipc_stop();
#endif // DUMMY_IPC

    return 0;
}