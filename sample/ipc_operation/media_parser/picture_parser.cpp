/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

/**
 * 文件用于模拟IPC的功能，仅用于demo测试使用
 * 开发者不需要过多关注，实际产品不会使用到该部分代码
 */

#include "picture_parser.h"

#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <assert.h>

#include "demo.h"

PictureParser::PictureParser(PictureBufferHandler handler) {
    assert(handler != nullptr);

    running_ = false;
    thread_not_quit_ = false;
    buffer_len_ = 0;
    picture_interval_ = kMinInterval;
    count_ = 0;
    handler_ = handler;
    buffer_ = new unsigned char[kMaxBufferLen];
}

PictureParser::~PictureParser() {
    Stop();
    delete [] buffer_;
}

int PictureParser::Start(unsigned int picture_interval, std::string &file_name) {
    FILE *fp = fopen(file_name.c_str(), "r");
    if (!fp) {
        printf("Open file failed: %s\n", file_name.c_str());
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    unsigned int file_size = ftell(fp);
    if (file_size > kMaxBufferLen) {
        printf("File is too large: %s\n", file_name.c_str());
        fclose(fp);
        return -1;
    }

    fseek(fp, 0, SEEK_SET);
    buffer_len_ = fread(buffer_, 1, file_size, fp);
    if(buffer_len_ != file_size) {
        printf("File read error: %s\n", file_name.c_str());
        fclose(fp);
        return -1;
    }
    fclose(fp);

    picture_interval_ = picture_interval < kMinInterval?kMinInterval:picture_interval;

    running_ = true;
    thread_not_quit_ = true;
    thread_handler_ = std::thread(pictureThread, this);
    thread_handler_.detach();

    return 0;
}

void *PictureParser::pictureThread(void *arg) {
    assert(arg != nullptr);
    PictureParser *picture_parser = (PictureParser *)arg;

    while (picture_parser->running_) {
        usleep(1000000);
        picture_parser->count_++;
        if (picture_parser->count_ > picture_parser->picture_interval_) {
            picture_parser->count_ = 0;
            picture_parser->handler_(picture_parser->buffer_, picture_parser->buffer_len_, nullptr);
        }
    }

    picture_parser->thread_not_quit_ = false;
    return nullptr;
}

void PictureParser::Stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    while (thread_not_quit_) {
        usleep(50000);
    }
}

int PictureParser::Trigger(const char *id) {
    if (running_) {
        handler_(buffer_, buffer_len_, id);
    }

    return 0;
}
