/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

/**
 * 文件用于模拟IPC的功能，仅用于demo测试使用
 * 开发者不需要过多关注，实际产品不会使用到该部分代码
 */

#include "property_parser.h"

#include <map>

static std::map<std::string, std::string> property_map =
    {{"MicSwitch", "0"},
     {"SpeakerSwitch", "0"},
     {"StatusLightSwitch","0"},
     {"DayNightMode", "0"},
     {"StreamVideoQuality", "0"},
     {"SubStreamVideoQuality", "0"},
     {"ImageFlipState", "0"},
     {"EncryptSwitch", "0"},
     {"AlarmSwitch", "0"},
     {"MotionDetectSensitivity", "0"},
     {"VoiceDetectionSensitivity", "0"},
     {"AlarmFrequencyLevel", "0"},
     {"AlarmNotifyPlan", "{{\"DayOfWeek\":0,\"BeginTime\":21600,\"EndTime\":36000},{\"DayOfWeek\":1,\"BeginTime\":21600,\"EndTime\":36000}}"},
     {"StorageStatus", "1"},
     {"StorageTotalCapacity", "128.000"},
     {"StorageRemainCapacity", "64.12"},
     {"StorageRecordMode", "0"}
};

PropertyHandler PropertyParser::handler_ = nullptr;
void PropertyParser::SetHandler(PropertyHandler handler) {
    handler_ = handler;
}

void PropertyParser::GetAllProperty() {
    if (handler_) {
        for (const auto& search:property_map) {
            handler_(search.first, search.second);
        }
    }
}

void PropertyParser::SetProperty(const std::string &key, const std::string &value) {
    auto search = property_map.find(key);
    if (search != property_map.end()) {
        property_map[key] = value;
    }
    if (handler_) {
        handler_(key, value);
    }
}