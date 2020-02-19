/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

/**
 * 文件用于模拟IPC的功能，仅用于demo测试使用
 * 开发者不需要过多关注，实际产品不会使用到该部分代码
 */

#ifndef PROJECT_PICTURE_PARSER_H
#define PROJECT_PICTURE_PARSER_H

#include <string>
#include <thread>

typedef void (*PictureBufferHandler)(unsigned char *buffer, unsigned int buffer_len, const char *id);

class PictureParser {
public:
    explicit PictureParser(PictureBufferHandler handler);
    ~PictureParser();

    int Start(unsigned int picture_interval, std::string &file_name);

    int Trigger(const char* id);

    void Stop();

private:
    static void *pictureThread(void *arg);

private:
    bool running_;
    bool thread_not_quit_;
    unsigned int buffer_len_;
    unsigned int picture_interval_;
    unsigned int count_;
    unsigned char* buffer_;
    std::thread thread_handler_;
    PictureBufferHandler handler_;
private:
    const unsigned int kMaxBufferLen = 64 *1024;
    const unsigned int kMinInterval = 10;
};


#endif //PROJECT_PICTURE_PARSER_H
