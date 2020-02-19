#ifndef LINK_VISUAL_DEF_H_
#define LINK_VISUAL_DEF_H_


/* 流可变参数  */
typedef struct {
    //当LV_STREAM_CMD_LIVE有效
    int pre_time;//直播的预录时间，暂未使用

    //当LV_STREAM_CMD_STORAGE_RECORD_BY_FILE有效
    char file_name[256];//要点播的文件名
    int timestamp_offset;//文件的时间偏移值（相对于文件头的相对偏移时间），单位：s
    int timestamp_base;  //基准时间戳，单位：s

    //当LV_STREAM_CMD_STORAGE_RECORD_BY_UTC_TIME有效
    unsigned int start_time;//播放当天0点的UTC时间,单位：s
    unsigned int stop_time;//播放当前24点的UTC时间,单位：s
    unsigned int seek_time;//播放的UTC时间相对于start_time的相对时间，即 seek_time + start_time = 播放的utc时间,单位：s
} lv_stream_param_s;

typedef enum {
    LV_STREAM_CMD_LIVE = 0, //直播
    LV_STREAM_CMD_STORAGE_RECORD_BY_FILE = 1,//按文件名播放设备存储录像
    LV_STREAM_CMD_STORAGE_RECORD_BY_UTC_TIME = 2,//按UTC时间播放设备存储录像
} lv_stream_type_e;

typedef enum {
    LV_STORAGE_RECORD_START = 0,//开始播放，对于录像点播有效
    LV_STORAGE_RECORD_PAUSE,//暂停，对于录像点播有效
    LV_STORAGE_RECORD_UNPAUSE,// 继续播放，对于录像点播有效
    LV_STORAGE_RECORD_SEEK,// 定位，对于录像点播有效
    LV_STORAGE_RECORD_STOP,//停止，对于录像点播有效
    LV_LIVE_REQUEST_I_FRAME,//强制编码I帧，对于直播有效
} lv_on_push_streaming_cmd_type_e;

/* 视频格式  */
typedef enum {
    //不支持同一设备切换视频编码类型。
    // 编码参数不同可支持切换（如主子码流的宽、高参数不相等），切换后需保证首帧为I帧
    LV_VIDEO_FORMAT_H264 = 0, //AVC
    LV_VIDEO_FORMAT_H265, //HEVC
} lv_video_format_e;

/* 音频格式  */
typedef enum {
    //不支持同一设备切换音频编码类型，不支持切换编码参数
    LV_AUDIO_FORMAT_PCM = 0,
    LV_AUDIO_FORMAT_G711A = 1,
    LV_AUDIO_FORMAT_AAC = 2,
} lv_audio_format_e;

/* 音频采样率  */
typedef enum {
    LV_AUDIO_SAMPLE_RATE_96000 = 0,
    LV_AUDIO_SAMPLE_RATE_88200 = 1,
    LV_AUDIO_SAMPLE_RATE_64000 = 2,
    LV_AUDIO_SAMPLE_RATE_48000 = 3,
    LV_AUDIO_SAMPLE_RATE_44100 = 4,
    LV_AUDIO_SAMPLE_RATE_32000 = 5,
    LV_AUDIO_SAMPLE_RATE_24000 = 6,
    LV_AUDIO_SAMPLE_RATE_22050 = 7,
    LV_AUDIO_SAMPLE_RATE_16000 = 8,
    LV_AUDIO_SAMPLE_RATE_12000 = 9,
    LV_AUDIO_SAMPLE_RATE_11025 = 10,
    LV_AUDIO_SAMPLE_RATE_8000 = 11,
    LV_AUDIO_SAMPLE_RATE_7350 = 12,
} lv_audio_sample_rate_e;

/* 音频位宽  */
typedef enum {
    LV_AUDIO_SAMPLE_BITS_8BIT  = 0,
    LV_AUDIO_SAMPLE_BITS_16BIT = 1,
} lv_audio_sample_bits_e;

/* 音频声道  */
typedef enum {
    LV_AUDIO_CHANNEL_MONO = 0,
    LV_AUDIO_CHANNEL_STEREO = 1,
} lv_audio_channel_e;

/* 视频参数结构体 */
typedef struct lv_video_param_s {
    lv_video_format_e format; //视频编码格式
    unsigned int fps;  //帧率
    unsigned int key_frame_interval_ms;//最大关键帧间隔时间，单位：毫秒
} lv_video_param_s;

/* 音频参数结构体 */
typedef struct lv_audio_param_s {
    lv_audio_format_e format;
    lv_audio_sample_rate_e sample_rate;
    lv_audio_sample_bits_e sample_bits;
    lv_audio_channel_e channel;
} lv_audio_param_s;

/* 事件类型 */
typedef enum {
    LV_TRIGGER_PIC = 0, //远程触发设备被动抓图上报
    LV_EVENT_MOVEMENT = 1, //移动侦测
    LV_EVENT_SOUND = 2, //声音侦测
    LV_EVENT_HUMAN = 3,  //人形侦测
    LV_EVENT_PET = 4, //宠物侦测
    LV_EVENT_CROSS_LINE = 5, //越界侦测
    LV_EVENT_REGIONAL_INVASION = 6, //区域入侵侦测
    LV_EVENT_FALL = 7, //跌倒检测
    LV_EVENT_FACE = 8, //人脸检测
    LV_EVENT_SMILING = 9, //笑脸检测
    LV_EVENT_ABNORMAL_SOUND = 10, //异响侦测
    LV_EVENT_CRY = 11, //哭声侦测
    LV_EVENT_LAUGH = 12, //笑声侦测
} lv_event_type_e;

/* SDK日志等级 */
typedef enum {
    LV_LOG_ERROR = 0,
    LV_LOG_WARN,
    LV_LOG_INFO,
    LV_LOG_DEBUG,
    LV_LOG_VERBOSE,
    LV_LOG_MAX
} lv_log_level_e;

/* SDK日志输出定向*/
typedef enum {
    LV_LOG_DESTINATION_FILE,//写文件，未实现；需写文件请使用 LV_LOG_DESTINATION_USER_DEFINE
    LV_LOG_DESTINATION_STDOUT,//直接向stdout输出日志
    LV_LOG_DESTINATION_USER_DEFINE,//将日志消息放入回调函数 lv_log_cb 中。可在回调函数中实现写文件功能。
    LV_LOG_DESTINATION_MAX
} lv_log_destination;

/* SDK的函数返回值枚举量 */
typedef enum {
    LV_ERROR_NONE = 0,
    LV_ERROR_DEFAULT = -1,
    LV_ERROR_ILLEGAL_INPUT = -2,
} lv_error_e;

typedef enum {
    LV_STORAGE_RECORD_PLAN = 0, //计划录像
    LV_STORAGE_RECORD_ALARM = 1, //报警录像
    LV_STORAGE_RECORD_INITIATIVE = 2, // 主动录像
} lv_storage_record_type_e;

/* 录像查询列表结构体 */
typedef struct lv_query_storage_record_response_s {
    int start_time;  // 录像开始时间，UTC时间，秒数
    int stop_time;   // 录像结束时间，UTC时间，秒数
    int file_size;   // 录像的文件大小，单位字节
    char file_name[64 + 1]; // 录像的文件名
    lv_storage_record_type_e record_type;//录像类型
} lv_query_storage_record_response_s;

/* linkkit适配器内容 */
typedef struct {
    int dev_id;
    const char *id;
    int id_len;
    const char *service_id;
    int service_id_len;
    const char *request;
    int request_len;
} lv_linkkit_adapter_property_s;

/* linkkit适配器类型 */
typedef enum {
    LV_LINKKIT_ADAPTER_TYPE_TSL_PROPERTY = 0, //物模型属性消息
    LV_LINKKIT_ADAPTER_TYPE_TSL_SERVICE, //物模型服务消息
    LV_LINKKIT_ADAPTER_TYPE_LINK_VISUAL, // LinkVisual自定义消息
    LV_LINKKIT_ADAPTER_TYPE_CONNECTED, //上线信息
} lv_linkkit_adapter_type_s;

/* SDK控制类型 */
typedef enum {
    LV_CONTROL_LOG_LEVEL = 0,
    LV_CONTROL_STREAM_AUTO_CHECK,
    LV_CONTROL_STREAM_AUTO_SAVE,
} lv_control_type_e;
#endif // LINK_VISUAL_DEF_H_
