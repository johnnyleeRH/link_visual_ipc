/*
 * Copyright (C) 2015-2019 Alibaba Group Holding Limited
 */

#ifndef __GUIDER_INTERNAL_H__
#define __GUIDER_INTERNAL_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "iot_import.h"

#include "iotx_log.h"
#include "iotx_utils.h"
#include "utils_md5.h"
#include "utils_base64.h"
#include "utils_hmac.h"
#include "utils_httpc.h"
#include "iotx_system.h"

#define GUIDER_IOT_ID_LEN (64)
#define GUIDER_IOT_TOKEN_LEN (64)
#define GUIDER_DEFAULT_TS_STR "2524608000000"

#define GUIDER_SIGN_LEN (48)
#define GUIDER_SIGN_SOURCE_LEN (256)

#define GUIDER_TS_LEN (16)
#define GUIDER_URL_LEN (128)

#define GUIDER_PREAUTH_REQUEST_LEN (256)
#define GUIDER_PREAUTH_RESPONSE_LEN (512)

#define SHA_METHOD "hmacsha1"
#define MD5_METHOD "hmacmd5"

#define GUIDER_PREAUTH_URL_FMT "https://%s/auth/devicename"

#define sys_emerg(...) log_emerg("sys", __VA_ARGS__)
#define sys_crit(...) log_crit("sys", __VA_ARGS__)
#define sys_err(...) log_err("sys", __VA_ARGS__)
#define sys_warning(...) log_warning("sys", __VA_ARGS__)
#define sys_info(...) log_info("sys", __VA_ARGS__)
#define sys_debug(...) log_debug("sys", __VA_ARGS__)

#define KV_MQTT_URL_KEY "mqtt_url"
#define KV_HTTP_URL_KEY "http_url"

typedef enum _secure_mode_e
{
    MODE_TLS_GUIDER = -1,
    MODE_TCP_GUIDER_PLAIN = 0,
    MODE_TCP_GUIDER_ID2_ENCRYPT = 1,
    MODE_TLS_DIRECT = 2,
    MODE_TCP_DIRECT_PLAIN = 3,
    MODE_TCP_DIRECT_ID2_ENCRYPT = 4,
    MODE_TLS_GUIDER_ID2_ENCRYPT = 5,
    MODE_TLS_DIRECT_ID2_ENCRYPT = 7,
    MODE_ITLS_DNS_ID2 = 8,
} secure_mode_e;

typedef enum _connect_method_e
{
    CONNECT_PREAUTH = 0,
    CONNECT_STATIC_DIRECT,
    CONNECT_DYNAMIC_DIRECT,
    CONNECT_METHOD_MAX
} connect_method_e;

typedef enum _ext_params_e
{
    EXT_PLAIN_ROUTE = 0,
    EXT_SMART_ROUTE,
    EXT_PARAM_MAX
} ext_params_e;

int _fill_conn_string(char *dst, int len, const char *fmt, ...);


extern int iotx_guider_set_dynamic_mqtt_url(char *p_mqtt_url);
extern int iotx_guider_set_dynamic_https_url(char *p_http_url);
extern int iotx_guider_get_dynamic_https_url(char *p_http_url, int url_buff_len);

int iotx_redirect_region_subscribe(void);
int iotx_redirect_set_flag(int flag);
int iotx_redirect_get_flag(void);
int iotx_redirect_get_iotId_iotToken(char *iot_id, char *iot_token, char *host, uint16_t *pport);

#endif /* __GUIDER_INTERNAL_H__ */
