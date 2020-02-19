/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

/**
 * 文件用于模拟IPC的功能，仅用于demo测试使用
 * 开发者不需要过多关注，实际产品不会使用到该部分代码
 */

#ifndef PROJECT_PROPERTY_PARSER_H
#define PROJECT_PROPERTY_PARSER_H

#include <string>

typedef void (*PropertyHandler)(const std::string &key, const std::string &value);


class PropertyParser {
public:
    static void SetHandler(PropertyHandler handler);

    static void GetAllProperty();

    static void SetProperty(const std::string &key, const std::string &value);

private:
    static PropertyHandler handler_;
};


#endif // PROJECT_PROPERTY_PARSER_H
