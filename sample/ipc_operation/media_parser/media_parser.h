/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

/**
 * 文件用于模拟IPC的功能，仅用于demo测试使用
 * 开发者不需要过多关注，实际产品不会使用到该部分代码
 */

#ifndef HEVC_PARSE_H_
#define HEVC_PARSE_H_

#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>
#include <sys/time.h>
#include <thread>

#include "link_visual_def.h"

typedef void (*VideoBufferHandler)(lv_video_format_e format, unsigned char *buffer, unsigned int buffer_size,
                          unsigned int timestamp_ms, int nal_type, int service_id);
typedef void (*AudioBufferHandler)(unsigned char *buffer, unsigned int buffer_size,
                                   unsigned int timestamp_ms, int service_id);

struct IndexInfo;
class MediaParse {
public:
    MediaParse(VideoBufferHandler video_handler, AudioBufferHandler audio_handler);
    virtual ~MediaParse();

    int Start(int service_id, const std::string &stream_prefix, bool circle, uint64_t complement_time);

    int Pause(bool flag);

    void Stop();

    void Seek(unsigned int timestamp_ms);

    void RequestIFrame() {wait_for_i_frame_ = true;};

    void SetServiceId(int service_id) {service_id_ = service_id;};

    int GetServiceId() const {return service_id_;} ;

    bool GetStatus() const {return running_;};

    std::string GetStreamPrefix() const {return stream_prefix_;};

    void GetParam(lv_video_param_s &video_param, lv_audio_param_s &audio_param, double &duration) const;

    static int GetDuration(const std::string &stream_prefix, double &duration);

private:
    int parseIndex(const std::string &stream_index);
    int parseParam(const std::string &stream_param);
    int parseStream(const std::string &stream_file);
    int getFrames();
    void processSeek();
    int processPause();
    void processRequestIFrame();
    int processFrames();
private:
    static uint64_t getTime();
    static void *readThread(void *parse);
private:
    uint32_t index_position_;
    uint64_t start_time_;
    uint64_t base_time_;
    uint64_t seek_time_;
    uint64_t pause_time_;
    uint64_t complement_time_;
    int service_id_;
    bool circle_;
    bool running_;
    bool thread_not_quit_;
    bool wait_for_i_frame_;
    bool seek_;
    bool pause_;
    lv_video_param_s *video_param_;
    lv_audio_param_s *audio_param_;
    double duration_;
    VideoBufferHandler video_handler_;
    AudioBufferHandler audio_handler_;
    FILE *stream_file_;
    unsigned char *buf_;
    std::thread read_thread_;
    std::string stream_prefix_;
    std::vector<struct IndexInfo> index_;

private:
    static const unsigned int kBufferMaxLen = 128 * 1024;
    static const unsigned int kIndexLineMaxLne = 128;
    static const unsigned int kMillisecondPerSecond = 1000;
    static const unsigned int kMediaFrameHeader = 0xFCFCFCFC;
};



#endif // HEVC_PARSE_H_