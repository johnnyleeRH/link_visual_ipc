/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

/**
 * 文件用于模拟IPC的功能，仅用于demo测试使用
 * 开发者不需要过多关注，实际产品不会使用到该部分代码
 */

#include "media_parser.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <stdio.h>
#include <chrono>

#include "demo.h"

struct IndexInfo {
    unsigned int media_type = 0;//0-video, 1-audio
    unsigned int len = 0;
    unsigned int offset = 0;
    unsigned int timestamp = 0;
    unsigned int key_frame = 0;//only for video
};

MediaParse::MediaParse(VideoBufferHandler video_handler, AudioBufferHandler audio_handler) {
    assert(video_handler != nullptr);
    assert(audio_handler != nullptr);

    index_position_ = 0;
    start_time_ = 0;
    base_time_ = 0;
    complement_time_ = 0;
    service_id_ = 0;
    seek_time_ = 0;
    pause_time_ = 0;
    circle_ = false;
    running_ = false;
    thread_not_quit_ = false;
    wait_for_i_frame_ = false;
    seek_ = false;
    pause_ = false;
    video_handler_ = video_handler;
    audio_handler_ = audio_handler;
    video_param_ = new lv_video_param_s;
    memset(video_param_, 0, sizeof(lv_video_param_s));
    audio_param_ = new lv_audio_param_s;
    memset(audio_param_, 0, sizeof(lv_audio_param_s));
    duration_ = 0;
    stream_file_ = nullptr;
    buf_ = new unsigned char [kBufferMaxLen];
}

MediaParse::~MediaParse() {
    Stop();
    delete video_param_;
    delete audio_param_;
    delete [] buf_;
}

int MediaParse::Start(int service_id, const std::string &stream_prefix, bool circle, uint64_t complement_time) {
    std::string stream_index = stream_prefix + ".index";
    if (parseIndex(stream_index) < 0) {
        printf("Parse stream index failed\n");
        return -1;
    }

    std::string stream_param = stream_prefix + ".meta";
    if (parseParam(stream_param) < 0) {
        printf("Parse stream param failed\n");
        return -1;
    }

    circle_ = circle;
    service_id_ = service_id;
    complement_time_ = complement_time;

    if (parseStream(stream_prefix) < 0) {
        printf("Parse stream file failed\n");
        return -1;
    }

    stream_prefix_ = stream_prefix;

    return 0;
}

int MediaParse::Pause(bool flag) {
    if (pause_ == flag) {
        return 0;//忽略重复的命令
    }

    pause_ = flag;
    if (pause_) {
        pause_time_ = getTime();
    }

    return 0;
}

void MediaParse::Stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    while (thread_not_quit_) {
        usleep(50000);
    }
    if (stream_file_) {
        fclose(stream_file_);
        stream_file_ = nullptr;
    }
    if (!index_.empty()) {
        std::vector<struct IndexInfo>().swap(index_);
    }
    index_position_ = 0;
    memset(video_param_, 0, sizeof(lv_video_param_s));
    memset(audio_param_, 0, sizeof(lv_audio_param_s));
}

void MediaParse::Seek(unsigned int timestamp_ms) {
    seek_time_ = timestamp_ms;
    seek_ = true;
}

void *MediaParse::readThread(void *parse) {
    assert(parse != nullptr);

    auto *media_parse = (MediaParse *)parse;
    while (media_parse->running_) {
        if (media_parse->getFrames() < 0) {
            printf("Thread auto quit\n");
            media_parse->running_ = false;
        }
        usleep(20000);
    }

    media_parse->thread_not_quit_ = false;
    return nullptr;
}

int MediaParse::getFrames() {
    if (!stream_file_) {
        return -1;
    }

    if (processPause() < 0) {
        return 0;
    }

    processSeek();

    processRequestIFrame();

    return processFrames();
}

int MediaParse::parseIndex(const std::string &stream_index) {
    FILE *index_file = fopen(stream_index.c_str(), "r");
    if (!index_file) {
        printf("Open file failed: %s\n", stream_index.c_str());
        return -1;
    }

    int ret = 0;
    while (!feof(index_file)) {
        char line[kIndexLineMaxLne];
        memset(line, 0, kIndexLineMaxLne);
        struct IndexInfo index_info;

        fgets(line, kIndexLineMaxLne, index_file);
        ret = sscanf(line, "media=%d,offset=%d,len=%d,timestamp=%d,key=%d",
                     &index_info.media_type, &index_info.offset, &index_info.len, &index_info.timestamp, &index_info.key_frame);
        if (ret != 5) {
            printf("Read file failed: %s\n", stream_index.c_str());
            fclose(index_file);
            return -1;
        }
        index_.push_back(index_info);
    }
    fclose(index_file);

    if (index_.empty()) {
        printf("Read index file failed: %s\n", stream_index.c_str());
        return -1;
    }

    return 0;
}

int MediaParse::parseParam(const std::string &stream_param) {
    FILE *fp = fopen(stream_param.c_str(), "r");
    if (!fp) {
        printf("Open file failed: %s\n", stream_param.c_str());
        return -1;
    }

    int ret = 0;
    const char* split = "=";
    while (!feof(fp)) {
        char line[kIndexLineMaxLne];
        memset(line, 0, kIndexLineMaxLne);
        fgets(line, kIndexLineMaxLne, fp);
        char *key = strtok(line, split);
        if (!key) {
            continue;
        }
        char *value = strtok(NULL, "\r\n");
        if (!strcmp(key, "video.format")) {
            if (!strcmp(value, "h264")) {
                video_param_->format = LV_VIDEO_FORMAT_H264;
            } else if (!strcmp(value, "h265")) {
                video_param_->format = LV_VIDEO_FORMAT_H265;
            } else {
                printf("Invalid param:  %s\n", value);
            }
        } else if (!strcmp(key, "video.fps")) {
            video_param_->fps = atoi(value);
        } else if (!strcmp(key, "audio.format")) {
            if (!strcmp(value, "aac")) {
                audio_param_->format = LV_AUDIO_FORMAT_AAC;
            } else if (!strcmp(value, "g711a")) {
                audio_param_->format = LV_AUDIO_FORMAT_G711A;
            } else if (!strcmp(value, "pcm")) {
                audio_param_->format = LV_AUDIO_FORMAT_PCM;
            } else {
                printf("Invalid param:  %s\n", value);
            }
        } else if (!strcmp(key, "audio.sample_rate")) {
            switch (atoi(value)) {
                case 8000:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_8000;
                    break;
                case 11025:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_11025;
                    break;
                case 12000:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_12000;
                    break;
                case 16000:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_16000;
                    break;
                case 22050:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_22050;
                    break;
                case 24000:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_24000;
                    break;
                case 32000:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_32000;
                    break;
                case 44100:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_44100;
                    break;
                case 48000:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_48000;
                    break;
                case 64000:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_64000;
                    break;
                case 88200:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_88200;
                    break;
                case 96000:
                    audio_param_->sample_rate = LV_AUDIO_SAMPLE_RATE_96000;
                    break;
                default:
                    printf("Invalid param:  %s\n", value);
                    ret = -1;
                    break;
            }
        } else if (!strcmp(key, "audio.bits_per_sample")) {
            switch (atoi(value)) {
                case 8:
                    audio_param_->sample_bits = LV_AUDIO_SAMPLE_BITS_8BIT;
                    break;
                case 16:
                    audio_param_->sample_bits = LV_AUDIO_SAMPLE_BITS_16BIT;
                    break;
                default:
                    printf("Invalid param:  %s\n", value);
                    ret = -1;
                    break;
            }
        } else if (!strcmp(key, "audio.channel")) {
            switch (atoi(value)) {
                case 1:
                    audio_param_->channel = LV_AUDIO_CHANNEL_MONO;
                    break;
                case 2:
                    audio_param_->channel = LV_AUDIO_CHANNEL_STEREO;
                    break;
                default:
                    printf("Invalid param:  %s\n", value);
                    ret = -1;
                    break;
            }
        } else if (!strcmp(key, "duration")) {
            duration_ = atoi(value);
        } else {

        }
    }

    fclose(fp);
    return ret;
}

int MediaParse::parseStream(const std::string &stream_file) {
    stream_file_ = fopen(stream_file.c_str(), "r");
    if (!stream_file_) {
        printf("Open file failed: %s\n", stream_file.c_str());
        return -1;
    }

    start_time_ = getTime();

    running_ = true;
    thread_not_quit_ = true;
    read_thread_ = std::thread(readThread, this);
    read_thread_.detach();

    return 0;
}

void MediaParse::GetParam(lv_video_param_s &video_param, lv_audio_param_s &audio_param, double &duration) const {
    video_param.format = video_param_->format;
    video_param.fps = video_param_->fps;

    audio_param.format = audio_param_->format;
    audio_param.sample_rate = audio_param_->sample_rate;
    audio_param.sample_bits = audio_param_->sample_bits;
    audio_param.channel = audio_param_->channel;
    duration = duration_;
}

void MediaParse::processSeek() {
    /* seek处理 */
    if (seek_) {
        if (index_[index_.size() - 1].timestamp < seek_time_) {
            start_time_ -= index_[index_.size() - 1].timestamp - index_[index_position_].timestamp;
            index_position_ = index_.size() - 1;
            fseek(stream_file_, index_[index_position_].offset, SEEK_SET);
        } else {
            bool find = false;
            unsigned int i = 0;
            start_time_ += index_[index_position_].timestamp - index_[0].timestamp;
            for (i = 0; i + 1 < index_.size() ; i++) {
                if (index_[i].timestamp > seek_time_ &&
                    index_[i].key_frame) {
                    find = true;
                    break;
                }
            }
            if (find) {
                start_time_ -= index_[i].timestamp - index_[0].timestamp;
                index_position_ = i;
                fseek(stream_file_, index_[index_position_].offset, SEEK_SET);
            }
        }
        seek_ = false;
    }
}

int MediaParse::processPause() {
    if (pause_) {
        return -1;
    }

    if (pause_time_ != 0) {
        uint64_t time_now = getTime();
        start_time_ += time_now - pause_time_;
        pause_time_ = 0;
    }
    return 0;
}

void MediaParse::processRequestIFrame() {
    /* 强制I帧处理 */
    if (wait_for_i_frame_) {
        bool forward = false;
        uint32_t i = 0;
        for (i = 0; i + index_position_ < index_.size(); i++) {
            if (index_[index_position_ + i].key_frame) {
                forward = true;
                break;
            }
        }
        if (forward) {
            base_time_ -= (index_[index_position_ + i].timestamp - index_[index_position_].timestamp);
            index_position_ += i;
            fseek(stream_file_, index_[index_position_].offset, SEEK_SET);
        } else {
            bool back = false;
            for (i = 0; index_position_ > i; i++) {
                if (index_[index_position_ - i].key_frame) {
                    back = true;
                    break;
                }
            }
            if (back) {
                base_time_ += (index_[index_position_].timestamp - index_[index_position_ - i].timestamp);
                index_position_ -= i;
                fseek(stream_file_, index_[index_position_].offset, SEEK_SET);
            }
        }
        wait_for_i_frame_ = false;
    }
}

int MediaParse::processFrames() {
    uint64_t current = getTime();
    while ((index_[index_position_].timestamp + base_time_ + start_time_) < current){
        if (index_[index_position_].len > kBufferMaxLen) {
            printf("Buf is too large\n");
            return -1;
        }
        uint64_t read_bit = fread(buf_, 1, index_[index_position_].len, stream_file_);
        if (read_bit != index_[index_position_].len || ((unsigned int *)buf_)[0] != kMediaFrameHeader || read_bit < sizeof(kMediaFrameHeader)) {
            printf("Read frame failed\n");
            return -1;
        }

        if (index_[index_position_].media_type == 0) {
            video_handler_(video_param_->format, buf_ + sizeof(kMediaFrameHeader), read_bit - sizeof(kMediaFrameHeader), (unsigned int)(index_[index_position_].timestamp + base_time_ + complement_time_), index_[index_position_].key_frame, service_id_);
        } else if (index_[index_position_].media_type == 1) {
            audio_handler_(buf_+ sizeof(kMediaFrameHeader), read_bit - sizeof(kMediaFrameHeader), (unsigned int)(index_[index_position_].timestamp + base_time_ + complement_time_), service_id_);
        }

        if (index_position_ + 1 == index_.size()) {
            if (!circle_) {
                return -1;
            }
            base_time_ += index_[index_position_].timestamp;
            index_position_ = 0;
            fseek(stream_file_, 0, SEEK_SET);
        } else {
            index_position_++;
        }
    }

    return 0;
}

uint64_t MediaParse::getTime() {
    std::chrono::steady_clock::time_point zero;
    std::chrono::steady_clock::time_point cur = std::chrono::steady_clock::now();

    return (std::chrono::duration_cast<std::chrono::milliseconds>(cur - zero)).count();
}

int MediaParse::GetDuration(const std::string &stream_prefix, double &duration) {
    std::string stream_param = stream_prefix + ".meta";
    FILE *fp = fopen(stream_param.c_str(), "r");
    if (!fp) {
        printf("Open file failed: %s\n", stream_param.c_str());
        return -1;
    }

    const char* split = "=";
    while (!feof(fp)) {
        char line[kIndexLineMaxLne];
        memset(line, 0, kIndexLineMaxLne);
        fgets(line, kIndexLineMaxLne, fp);
        char *key = strtok(line, split);
        char *value = strtok(NULL, "\r\n");
        if (!key || !value) {
            continue;
        }
        if (!strcmp(key, "duration")) {
            duration = atoi(value);
        } else {
            //printf("Invalid line: %s\n", key);
        }
    }

    fclose(fp);
    return 0;
}




