/*
 * Copyright (C) 2015-2019 Alibaba Group Holding Limited
 */

#include "iot_export.h"
#include "sdk-impl_internal.h"
#include "iotx_system_internal.h"

#define SYS_GUIDER_MALLOC(size) LITE_malloc(size, MEM_MAGIC, "guider")
#define SYS_GUIDER_FREE(ptr) LITE_free(ptr)

#ifndef CONFIG_GUIDER_AUTH_TIMEOUT
#define CONFIG_GUIDER_AUTH_TIMEOUT (10 * 1000)
#endif

#ifndef CONFIG_GUIDER_DUMP_SECRET
#define CONFIG_GUIDER_DUMP_SECRET (0)
#endif

#define CUSTOME_DOMAIN_LEN_MAX (60)

static char only_use_one_time = 0;

static char *get_secure_mode_str(secure_mode_e secure_mode)
{
    static char *secure_mode_str = NULL;

    switch (secure_mode)
    {
    case MODE_TLS_GUIDER:
    {
        secure_mode_str = "TLS + Guider";
    }
    break;
    case MODE_TCP_GUIDER_PLAIN:
    {
        secure_mode_str = "TCP + Guider + Plain";
    }
    break;
    case MODE_TCP_GUIDER_ID2_ENCRYPT:
    {
        secure_mode_str = "TCP + Guider + ID2";
    }
    break;
    case MODE_TLS_DIRECT:
    {
        secure_mode_str = "TLS + Direct";
    }
    break;
    case MODE_TCP_DIRECT_PLAIN:
    {
        secure_mode_str = "TCP + Direct + Plain";
    }
    break;
    case MODE_TCP_DIRECT_ID2_ENCRYPT:
    {
        secure_mode_str = "TCP + Direct + ID2";
    }
    break;
    case MODE_TLS_GUIDER_ID2_ENCRYPT:
    {
        secure_mode_str = "TLS + Guider + ID2";
    }
    break;
    case MODE_TLS_DIRECT_ID2_ENCRYPT:
    {
        secure_mode_str = "TLS + Guider + ID2";
    }
    break;
    case MODE_ITLS_DNS_ID2:
    {
        secure_mode_str = "ITLS + DNS + ID2";
    }
    break;
    default:
        break;
    }

    return secure_mode_str;
}

const char *domain_mqtt_direct[] = {
    "iot-as-mqtt.cn-shanghai.aliyuncs.com",    /* Shanghai */
    "iot-as-mqtt.ap-southeast-1.aliyuncs.com", /* Singapore */
    "iot-as-mqtt.ap-northeast-1.aliyuncs.com", /* Japan */
    "iot-as-mqtt.us-west-1.aliyuncs.com",      /* America */
    "iot-as-mqtt.eu-central-1.aliyuncs.com"    /* Germany */
};

const char *domain_http_auth[] = {
    "iot-auth.cn-shanghai.aliyuncs.com", /* Shanghai */
#ifdef MQTT_PREAUTH_SUPPORT_HTTPS_CDN
    "ap-southeast-1-auth.link.aliyuncs.com", /* Singapore */
#else
    "iot-auth.ap-southeast-1.aliyuncs.com", /* Singapore */
#endif
    "iot-auth.ap-northeast-1.aliyuncs.com", /* Japan */
    "iot-auth.us-west-1.aliyuncs.com",      /* America */
    "iot-auth.eu-central-1.aliyuncs.com",   /* Germany */
};

#if defined(ON_DAILY)
#define GUIDER_DIRECT_DOMAIN_DAILY "iot-test-daily.iot-as-mqtt.unify.aliyuncs.com"
#define GUIDER_DIRECT_DOMAIN_ITLS_DAILY "10.125.7.82"
#elif defined(ON_PRE)
#define GUIDER_DIRECT_DOMAIN_PRE "100.67.80.75"
#define GUIDER_DIRECT_DOMAIN_ITLS_PRE "106.15.166.168"
const char *domain_http_auth_pre[] = {
    "iot-auth-pre.cn-shanghai.aliyuncs.com",    /* Shanghai */
    "100.67.85.121",                            /* Singapore: iot-auth-pre.ap-southeast-1.aliyuncs.com */
    "iot-auth-pre.ap-northeast-1.aliyuncs.com", /* Japan */
    "iot-auth-pre.us-west-1.aliyuncs.com",      /* America */
    "iot-auth-pre.eu-central-1.aliyuncs.com",   /* Germany */
};
#else //ON_LINE
#define GUIDER_DIRECT_DOMAIN_ITLS "itls.cn-shanghai.aliyuncs.com"
#endif

static char iotx_domain_custom[GUIDER_DOMAIN_MAX][CUSTOME_DOMAIN_LEN_MAX] = {{0}};

static iotx_cloud_region_types_t iotx_guider_get_region(void)
{
    iotx_cloud_region_types_t region_type;

    IOT_Ioctl(IOTX_IOCTL_GET_REGION, &region_type);

    return region_type;
}

static int guider_get_dynamic_mqtt_url(char *p_mqtt_url, int mqtt_url_buff_len)
{
    int len = mqtt_url_buff_len;

    return HAL_Kv_Get(KV_MQTT_URL_KEY, p_mqtt_url, &len);
}

int iotx_guider_set_dynamic_mqtt_url(char *p_mqtt_url)
{
    int len = 0;
    if (!p_mqtt_url)
        return -1;

    len = strlen(p_mqtt_url) + 1;
    if (len > GUIDER_URL_LEN - 2)
    {
        return -1;
    }

    return HAL_Kv_Set(KV_MQTT_URL_KEY, p_mqtt_url, len, 1);
}

int iotx_guider_set_dynamic_https_url(char *p_http_url)
{
    int len = 0;
    if (!p_http_url)
        return -1;

    len = strlen(p_http_url) + 1;
    if (len > GUIDER_URL_LEN - 2)
    {
        return -1;
    }

    return HAL_Kv_Set(KV_HTTP_URL_KEY, p_http_url, len, 1);
}

int iotx_guider_get_dynamic_https_url(char *p_http_url, int url_buff_len)
{
    int len = url_buff_len;

    if (!p_http_url)
        return -1;

    return HAL_Kv_Get(KV_HTTP_URL_KEY, p_http_url, &len);
}

/* return domain of mqtt direct or http auth */
const char *iotx_guider_get_domain(domain_type_t domain_type)
{
    iotx_cloud_region_types_t region_type;

    region_type = iotx_guider_get_region();

    if (IOTX_CLOUD_REGION_CUSTOM == region_type)
    {
        return iotx_domain_custom[domain_type];
    }

    if (GUIDER_DOMAIN_MQTT == domain_type)
    {
        return (char *)domain_mqtt_direct[region_type];
    }
    else if (GUIDER_DOMAIN_HTTP == domain_type)
    {
#if defined(ON_PRE)
        return (char *)domain_http_auth_pre[region_type];
#elif defined(ON_DAILY)
        return "iot-auth.alibaba.net";
#else
        return (char *)domain_http_auth[region_type];
#endif
    }
    else
    {
        sys_err("domain type err");
        return NULL;
    }
}

int iotx_guider_set_custom_domain(int domain_type, const char *domain)
{
    if ((domain_type >= GUIDER_DOMAIN_MAX) || (domain == NULL))
    {
        return FAIL_RETURN;
    }

    int len = strlen(domain);
    if (len >= CUSTOME_DOMAIN_LEN_MAX)
    {
        return FAIL_RETURN;
    }

    memset(iotx_domain_custom[domain_type], 0, CUSTOME_DOMAIN_LEN_MAX);
    memcpy(iotx_domain_custom[domain_type], domain, len);

    return SUCCESS_RETURN;
}

static int _calc_hmac_signature(char *hmac_sigbuf, const int hmac_buflen,
                                const char *timestamp_str, iotx_device_info_t *p_dev_info, ext_params_e ext_params)
{
    int rc = -1;
    char *hmac_source_buf = NULL;

    hmac_source_buf = SYS_GUIDER_MALLOC(GUIDER_SIGN_SOURCE_LEN);
    if (!hmac_source_buf)
    {
        sys_err("no mem");
        return -1;
    }

    memset(hmac_source_buf, 0, GUIDER_SIGN_SOURCE_LEN);

    if (EXT_SMART_ROUTE == ext_params)
    {
        rc = HAL_Snprintf(hmac_source_buf,
                          GUIDER_SIGN_SOURCE_LEN,
                          "clientId%s"
                          "deviceName%s"
                          "ext%d"
                          "productKey%s"
                          "timestamp%s",
                          p_dev_info->device_id,
                          p_dev_info->device_name,
                          ext_params,
                          p_dev_info->product_key,
                          timestamp_str);
    }
    else
    {
        rc = HAL_Snprintf(hmac_source_buf,
                          GUIDER_SIGN_SOURCE_LEN,
                          "clientId%s"
                          "deviceName%s"
                          "productKey%s"
                          "timestamp%s",
                          p_dev_info->device_id,
                          p_dev_info->device_name,
                          p_dev_info->product_key,
                          timestamp_str);
    }

    LITE_ASSERT(rc < GUIDER_SIGN_SOURCE_LEN);

    utils_hmac_sha1(hmac_source_buf, strlen(hmac_source_buf), hmac_sigbuf,
                    p_dev_info->device_secret, strlen(p_dev_info->device_secret));

    if (hmac_source_buf)
    {
        SYS_GUIDER_FREE(hmac_source_buf);
    }

    return 0;
}

static int _http_response(char *response_buf,
                          const int response_buf_len,
                          const char *request_string,
                          const char *url,
                          const int port_num,
                          const char *pkey)
{
    int ret = -1;
    httpclient_t httpc;
    httpclient_data_t httpc_data;

    memset(&httpc, 0, sizeof(httpclient_t));
    memset(&httpc_data, 0, sizeof(httpclient_data_t));

    httpc.header = "Accept: application/json\r\n";

    httpc_data.post_content_type = "application/x-www-form-urlencoded;charset=utf-8";
    httpc_data.post_buf = (char *)request_string;
    httpc_data.post_buf_len = strlen(request_string);
    httpc_data.response_buf = response_buf;
    httpc_data.response_buf_len = GUIDER_PREAUTH_RESPONSE_LEN;

    ret = httpclient_common(&httpc, url, port_num, pkey, HTTPCLIENT_POST,
                            CONFIG_GUIDER_AUTH_TIMEOUT, &httpc_data);

    httpclient_close(&httpc);

    return ret;
}

int _fill_conn_string(char *dst, int len, const char *fmt, ...)
{
    int rc = -1;
    va_list ap;
    char *ptr = NULL;

    va_start(ap, fmt);
    rc = HAL_Vsnprintf(dst, len, fmt, ap);
    va_end(ap);
    LITE_ASSERT(rc <= len);

    ptr = strstr(dst, "||");
    if (ptr)
    {
        *ptr = '\0';
    }

    return 0;
}

static int printf_parting_line(void)
{
    return printf("%s", ".................................\r\n");
}

static void guider_print_conn_info(iotx_conn_info_t *conn)
{
    int pub_key_len = 0;

    LITE_ASSERT(conn);
    printf_parting_line();
    printf("%10s : %-s\r\n", "Host", conn->host_name);
    printf("%10s : %d\r\n", "Port", conn->port);
#if CONFIG_GUIDER_DUMP_SECRET
    printf("%10s : %-s\r\n", "User", conn->username);
    printf("%10s : %-s\r\n", "PW", conn->password);
#endif
    HAL_Printf("%10s : %-s\r\n", "ClientID", conn->client_id);
    if (conn->pub_key)
    {
        pub_key_len = strlen(conn->pub_key);
        if (pub_key_len > 63)
            HAL_Printf("%10s : ('... %.16s ...')\r\n", "CA", conn->pub_key + strlen(conn->pub_key) - 63);
    }

    printf_parting_line();
}

static void guider_print_dev_guider_info(iotx_device_info_t *dev,
                                         char *partner_id,
                                         char *module_id,
                                         char *guider_url,
                                         int secure_mode,
                                         char *time_stamp,
                                         char *guider_sign)
{
    char ds[11];

    memset(ds, 0, sizeof(ds));
    memcpy(ds, dev->device_secret, sizeof(ds) - 1);

    printf_parting_line();
    printf("%5s : %-s\r\n", "PK", dev->product_key);
    printf("%5s : %-s\r\n", "DN", dev->device_name);
    printf("%5s : %-s\r\n", "DS", ds);
    printf("%5s : %-s\r\n", "PID", partner_id);
    printf("%5s : %-s\r\n", "MID", module_id);

    if (guider_url && strlen(guider_url) > 0)
    {
        HAL_Printf("%5s : %s\r\n", "URL", guider_url);
    }

    printf("%5s : %s\r\n", "SM", get_secure_mode_str(secure_mode));
    printf("%5s : %s\r\n", "TS", time_stamp);
#if CONFIG_GUIDER_DUMP_SECRET
    printf("%5s : %s\r\n", "Sign", guider_sign);
#endif
    printf_parting_line();

    return;
}

static void guider_get_timestamp_str(char *buf, int len)
{
    HAL_Snprintf(buf, len, "%s", GUIDER_DEFAULT_TS_STR);

    return;
}

static connect_method_e get_connect_method(const char *p_mqtt_url)
{
    connect_method_e connect_method = CONNECT_PREAUTH;

    if (p_mqtt_url && strlen(p_mqtt_url) > 0)
    {
        connect_method = CONNECT_DYNAMIC_DIRECT;
    }
    else if (IOTX_CLOUD_REGION_SHANGHAI == iotx_guider_get_region())
    {
        connect_method = CONNECT_STATIC_DIRECT;
    }

    return connect_method;
}

static secure_mode_e guider_get_secure_mode(connect_method_e connect_method)
{
    secure_mode_e secure_mode = MODE_TLS_GUIDER;

    if (CONNECT_PREAUTH == connect_method)
    {
#ifdef SUPPORT_TLS
        secure_mode = MODE_TLS_GUIDER;
#else
        secure_mode = MODE_TCP_GUIDER_PLAIN;
#endif
    }
    else
    {
#ifdef SUPPORT_ITLS
        secure_mode = MODE_ITLS_DNS_ID2;
#else
#ifdef SUPPORT_TLS
        secure_mode = MODE_TLS_DIRECT;
#else
        secure_mode = MODE_TCP_DIRECT_PLAIN;
#endif
#endif
    }

    return secure_mode;
}

static int guider_set_auth_req_str(char *request_buf, iotx_device_info_t *p_dev, char sign[], char ts[], ext_params_e ext_param)
{
    int rc = -1;

    rc = HAL_Snprintf(request_buf, GUIDER_PREAUTH_REQUEST_LEN,
                      "productKey=%s&"
                      "deviceName=%s&"
                      "signmethod=%s&"
                      "sign=%s&"
                      "version=default&"
                      "clientId=%s&"
                      "timestamp=%s&"
                      "resources=mqtt&ext=%d",
                      p_dev->product_key, p_dev->device_name, SHA_METHOD, sign, p_dev->device_id, ts, ext_param);

    LITE_ASSERT(rc < GUIDER_PREAUTH_REQUEST_LEN);

    return 0;
}

static int guider_get_iotId_iotToken(
    const char *guider_addr,
    const char *request_string,
    char *iot_id,
    char *iot_token,
    char *host,
    uint16_t *pport)
{
    int ret = -1;
    int iotx_port = 443;
    int ret_code = 0;
    char port_str[6];
    char *iotx_payload = NULL;
    const char *pvalue;

#ifndef SUPPORT_TLS
    iotx_port = 80;
#endif

#if defined(TEST_OTA_PRE)
    iotx_port = 80;
#endif

    /*
    {
        "code": 200,
        "data": {
            "iotId":"030VCbn30334364bb36997f44cMYTBAR",
            "iotToken":"e96d15a4d4734a73b13040b1878009bc",
            "resources": {
                "mqtt": {
                        "host":"iot-as-mqtt.cn-shanghai.aliyuncs.com",
                        "port":1883
                    }
                }
        },
        "message":"success"
    }
    */
    iotx_payload = SYS_GUIDER_MALLOC(GUIDER_PREAUTH_RESPONSE_LEN);
    LITE_ASSERT(iotx_payload);
    memset(iotx_payload, 0, GUIDER_PREAUTH_RESPONSE_LEN);
    _http_response(iotx_payload,
                   GUIDER_PREAUTH_RESPONSE_LEN,
                   request_string,
                   guider_addr,
                   iotx_port,
#if defined(TEST_OTA_PRE)
                   NULL
#else
                   iotx_ca_get()
#endif
    );
    sys_info("MQTT URL:");
    iotx_facility_json_print(iotx_payload, LOG_INFO_LEVEL, '>');

    pvalue = LITE_json_value_of("code", iotx_payload, MEM_MAGIC, "sys.preauth");
    if (!pvalue)
    {
        dump_http_status(STATE_HTTP_PREAUTH_TIMEOUT_FAIL, "no code in resp");
        goto EXIT;
    }

    ret_code = atoi(pvalue);
    LITE_free(pvalue);
    pvalue = NULL;

    if (200 != ret_code)
    {
        dump_http_status(STATE_HTTP_PREAUTH_IDENT_AUTH_FAIL, "ret_code = %d (!= 200), abort!", ret_code);
        goto EXIT;
    }

    pvalue = LITE_json_value_of("data.iotId", iotx_payload, MEM_MAGIC, "sys.preauth");
    if (NULL == pvalue)
    {
        dump_http_status(STATE_HTTP_PREAUTH_TIMEOUT_FAIL, "no iotId in resp");
        goto EXIT;
    }
    strcpy(iot_id, pvalue);
    LITE_free(pvalue);
    pvalue = NULL;

    pvalue = LITE_json_value_of("data.iotToken", iotx_payload, MEM_MAGIC, "sys.preauth");
    if (NULL == pvalue)
    {
        dump_http_status(STATE_HTTP_PREAUTH_TIMEOUT_FAIL, "no iotToken in resp");
        goto EXIT;
    }
    strcpy(iot_token, pvalue);
    LITE_free(pvalue);
    pvalue = NULL;

    pvalue = LITE_json_value_of("data.resources.mqtt.host", iotx_payload, MEM_MAGIC, "sys.preauth");
    if (NULL == pvalue)
    {
        dump_http_status(STATE_HTTP_PREAUTH_TIMEOUT_FAIL, "no resources.mqtt.host in resp");
        goto EXIT;
    }
    strcpy(host, pvalue);
    LITE_free(pvalue);
    pvalue = NULL;

    pvalue = LITE_json_value_of("data.resources.mqtt.port", iotx_payload, MEM_MAGIC, "sys.preauth");
    if (NULL == pvalue)
    {
        dump_http_status(STATE_HTTP_PREAUTH_TIMEOUT_FAIL, "no resources.mqtt.port in resp");
        goto EXIT;
    }
    strcpy(port_str, pvalue);
    LITE_free(pvalue);
    pvalue = NULL;
    *pport = atoi(port_str);

    ret = 0;

EXIT:
    if (iotx_payload)
    {
        LITE_free(iotx_payload);
        iotx_payload = NULL;
    }
    if (pvalue)
    {
        LITE_free(pvalue);
        pvalue = NULL;
    }

    return ret;
}

#if defined(SUPPORT_ITLS)
static int get_itls_host_and_port(char *product_key, iotx_conn_info_t *conn)
{
#if defined(ON_DAILY)
    conn->port = 1885;
    conn->host_name = SYS_GUIDER_MALLOC(sizeof(GUIDER_DIRECT_DOMAIN_ITLS_DAILY));
    if (conn->host_name == NULL)
    {
        return -1;
    }
    _fill_conn_string(conn->host_name, sizeof(GUIDER_DIRECT_DOMAIN_ITLS_DAILY),
                      GUIDER_DIRECT_DOMAIN_ITLS_DAILY);
#elif defined(ON_PRE)
    conn->port = 1885;
    conn->host_name = SYS_GUIDER_MALLOC(sizeof(GUIDER_DIRECT_DOMAIN_ITLS_PRE));
    if (conn->host_name == NULL)
    {
        return -1;
    }
    _fill_conn_string(conn->host_name, sizeof(GUIDER_DIRECT_DOMAIN_ITLS_PRE),
                      GUIDER_DIRECT_DOMAIN_ITLS_PRE);
#else
    int host_name_len = 0;

    conn->port = 1883;
    host_name_len = strlen(product_key) + 1 + sizeof(GUIDER_DIRECT_DOMAIN_ITLS);
    conn->host_name = SYS_GUIDER_MALLOC(host_name_len);
    if (conn->host_name == NULL)
    {
        return -1;
    }
    _fill_conn_string(conn->host_name, host_name_len,
                      "%s.%s",
                      product_key,
                      GUIDER_DIRECT_DOMAIN_ITLS);
#endif

    return 0;
}
#else
static int get_non_itls_host_and_port(sdk_impl_ctx_t *ctx, char *product_key, iotx_conn_info_t *conn, const char *p_dynamic_mqtt_url)
{
#if defined(ON_DAILY)
    conn->port = 1883;
    conn->host_name = SYS_GUIDER_MALLOC(sizeof(GUIDER_DIRECT_DOMAIN_DAILY));
    if (conn->host_name == NULL)
    {
        return -1;
    }
    _fill_conn_string(conn->host_name, sizeof(GUIDER_DIRECT_DOMAIN_DAILY),
                      GUIDER_DIRECT_DOMAIN_DAILY);
#elif defined(ON_PRE)
    conn->port = 80;

    conn->host_name = SYS_GUIDER_MALLOC(sizeof(GUIDER_DIRECT_DOMAIN_PRE));
    if (conn->host_name == NULL)
    {
        return -1;
    }
    _fill_conn_string(conn->host_name, sizeof(GUIDER_DIRECT_DOMAIN_PRE),
                      GUIDER_DIRECT_DOMAIN_PRE);
#else

    int port_num = 0;
    int host_name_len = 0;

    if (NULL != ctx)
    {
        port_num = ctx->mqtt_port_num;
        if (0 != port_num)
        {
            conn->port = port_num;
        }
    }

    if (0 == conn->port)
    {
#ifdef SUPPORT_TLS
        conn->port = 443;
#else
        conn->port = 1883;
#endif
    }

    if (p_dynamic_mqtt_url && strlen(p_dynamic_mqtt_url) > 0)
    {
        host_name_len = strlen(product_key) + strlen(p_dynamic_mqtt_url) + 2;
        conn->host_name = SYS_GUIDER_MALLOC(host_name_len);
        if (conn->host_name == NULL)
        {
            return -1;
        }

        _fill_conn_string(conn->host_name, host_name_len,
                          "%s.%s", product_key, p_dynamic_mqtt_url);
    }
    else
    {
        host_name_len = strlen(product_key) + 2 + strlen(iotx_guider_get_domain(GUIDER_DOMAIN_MQTT));
        conn->host_name = SYS_GUIDER_MALLOC(host_name_len);
        if (conn->host_name == NULL)
        {
            return -1;
        }

        _fill_conn_string(conn->host_name, host_name_len,
                          "%s.%s", product_key,
                          iotx_guider_get_domain(GUIDER_DOMAIN_MQTT));
    }

#endif

    return 0;
}
#endif

static int direct_get_conn_info(iotx_conn_info_t *conn, iotx_device_info_t *p_dev,
                                char *p_pid, char *p_mid, char *p_guider_url,
                                char *p_guider_sign, secure_mode_e secure_mode, const char *p_dynamic_mqtt_url)
{
    int rc = -1;
    int len = 0;
    int gw = 0;
    int ext = 0;

    char timestamp_str[GUIDER_TS_LEN] = {0};
    sdk_impl_ctx_t *ctx = sdk_impl_get_ctx();

    guider_get_timestamp_str(timestamp_str, sizeof(timestamp_str));

#if defined(SUPPORT_ITLS)
    rc = get_itls_host_and_port(p_dev->product_key, conn);
    if (rc < 0)
    {
        sys_err("host info err");
        return -1;
    }
#else
    rc = get_non_itls_host_and_port(ctx, p_dev->product_key, conn, p_dynamic_mqtt_url);
    if (rc < 0)
    {
        sys_err("host info err");
        return -1;
    }
#endif

    _calc_hmac_signature(p_guider_sign, GUIDER_SIGN_LEN, timestamp_str, p_dev, EXT_PLAIN_ROUTE);

    guider_print_dev_guider_info(p_dev, p_pid, p_mid, p_guider_url, secure_mode,
                                 timestamp_str, p_guider_sign);

    len = strlen(p_dev->device_name) + strlen(p_dev->product_key) + 2;
    conn->username = SYS_GUIDER_MALLOC(len);
    if (conn->username == NULL)
    {
        goto FAILED;
    }

    /* fill up username and password */
    _fill_conn_string(conn->username, len,
                      "%s&%s",
                      p_dev->device_name,
                      p_dev->product_key);

    len = GUIDER_SIGN_LEN + 1;
    conn->password = SYS_GUIDER_MALLOC(len);
    if (conn->password == NULL)
    {
        goto FAILED;
    }

    _fill_conn_string(conn->password, len,
                      "%s", p_guider_sign);

    conn->pub_key = iotx_ca_get();

    conn->client_id = SYS_GUIDER_MALLOC(CLIENT_ID_LEN);
    if (conn->client_id == NULL)
    {
        goto FAILED;
    }
    _fill_conn_string(conn->client_id, CLIENT_ID_LEN,
                      "%s"
                      "|securemode=%d"
                      ",_v=sdk-c-" LINKKIT_VERSION
                      ",timestamp=%s"
                      ",signmethod=" SHA_METHOD ",lan=C"
                      ",gw=%d"
                      ",ext=%d"
                      ",pid=%s"
                      ",mid=%s"
#ifdef SUPPORT_ITLS
                      ",authtype=id2"
#endif
                      ",_fy=%s"
#ifdef MQTT_AUTO_SUBSCRIBE
                      ",_ss=1"
#endif
                      "|",
                      p_dev->device_id, secure_mode, timestamp_str, gw, ext, p_pid, p_mid, LIVING_SDK_VERSION);

    guider_print_conn_info(conn);

    return 0;

FAILED:
    if (conn->username)
    {
        SYS_GUIDER_FREE(conn->username);
    }
    if (conn->password)
    {
        SYS_GUIDER_FREE(conn->password);
    }
    if (conn->client_id)
    {
        SYS_GUIDER_FREE(conn->client_id);
    }

    return -1;
}

static int guider_get_conn_info(iotx_conn_info_t *conn, iotx_device_info_t *p_dev,
                                char *p_pid, char *p_mid, char *p_guider_url,
                                char *p_guider_sign, secure_mode_e secure_mode)
{
    int gw = 0;
    int ext = 0;
    int len = 0;
    int rc = -1;
    char *p_request_buf = NULL;
    char timestamp_str[GUIDER_TS_LEN] = {0};
    sdk_impl_ctx_t *ctx = sdk_impl_get_ctx();

    const char *p_domain = NULL;
    uint16_t iotx_conn_port = 1883;

    char *p_iotx_id = NULL;
    char *p_iotx_token = NULL;
    char *p_iotx_conn_host = NULL;

    guider_get_timestamp_str(timestamp_str, sizeof(timestamp_str));
    p_domain = iotx_guider_get_domain(GUIDER_DOMAIN_HTTP);
    HAL_Snprintf(p_guider_url, GUIDER_URL_LEN, GUIDER_PREAUTH_URL_FMT, p_domain);
    _calc_hmac_signature(p_guider_sign, GUIDER_SIGN_LEN, timestamp_str, p_dev, EXT_SMART_ROUTE);

    guider_print_dev_guider_info(p_dev, p_pid, p_mid, p_guider_url, secure_mode,
                                 timestamp_str, p_guider_sign);

    p_request_buf = SYS_GUIDER_MALLOC(GUIDER_PREAUTH_REQUEST_LEN);
    if (!p_request_buf)
    {
        rc = -1;
        goto EXIT;
    }
    memset(p_request_buf, 0, GUIDER_PREAUTH_REQUEST_LEN);

    guider_set_auth_req_str(p_request_buf, p_dev, p_guider_sign, timestamp_str, EXT_SMART_ROUTE);

    sys_debug("req_str = '%s'", p_request_buf);

    p_iotx_id = SYS_GUIDER_MALLOC(GUIDER_IOT_ID_LEN);
    if (!p_iotx_id)
    {
        rc = -1;
        goto EXIT;
    }
    p_iotx_token = SYS_GUIDER_MALLOC(GUIDER_IOT_TOKEN_LEN);
    if (!p_iotx_token)
    {
        rc = -1;
        goto EXIT;
    }
    p_iotx_conn_host = SYS_GUIDER_MALLOC(HOST_ADDRESS_LEN);
    if (!p_iotx_conn_host)
    {
        rc = -1;
        goto EXIT;
    }

    /* http auth */
    if (iotx_redirect_get_flag())
    {
        if (iotx_redirect_get_iotId_iotToken(p_iotx_id, p_iotx_token, p_iotx_conn_host, &iotx_conn_port) != 0)
        {
            iotx_redirect_set_flag(0);
            goto EXIT;
        }
    }
    else
    {
        if (0 != guider_get_iotId_iotToken(p_guider_url, p_request_buf, p_iotx_id, p_iotx_token,
                                           p_iotx_conn_host, &iotx_conn_port))
        {
            sys_err("Request MQTT URL failed");
            goto EXIT;
        }
    }
    /* setup the hostname and port */
    conn->port = iotx_conn_port;
    len = strlen(p_iotx_conn_host) + 1;
    conn->host_name = SYS_GUIDER_MALLOC(len);
    if (conn->host_name == NULL)
    {
        goto EXIT;
    }
    _fill_conn_string(conn->host_name, len,
                      "%s", p_iotx_conn_host);

    /* fill up username and password */
    len = strlen(p_iotx_id) + 1;
    conn->username = SYS_GUIDER_MALLOC(len);
    if (conn->username == NULL)
    {
        goto EXIT;
    }
    _fill_conn_string(conn->username, len, "%s", p_iotx_id);
    len = strlen(p_iotx_token) + 1;
    conn->password = SYS_GUIDER_MALLOC(len);
    if (conn->password == NULL)
    {
        goto EXIT;
    }
    _fill_conn_string(conn->password, len, "%s", p_iotx_token);

    conn->pub_key = iotx_ca_get();

    len = CLIENT_ID_LEN;
    conn->client_id = SYS_GUIDER_MALLOC(len);
    if (conn->client_id == NULL)
    {
        goto EXIT;
    }
    _fill_conn_string(conn->client_id, len,
                      "%s"
                      "|securemode=%d"
                      ",_v=sdk-c-" LINKKIT_VERSION
                      ",timestamp=%s"
                      ",signmethod=" SHA_METHOD ",lan=C"
                      ",gw=%d"
                      ",ext=%d"
                      ",pid=%s"
                      ",mid=%s"
#ifdef SUPPORT_ITLS
                      ",authtype=id2"
#endif
                      ",_fy=%s"
#ifdef MQTT_AUTO_SUBSCRIBE
                      ",_ss=1"
#endif
                      "|",
                      p_dev->device_id, secure_mode, timestamp_str, gw, ext, p_pid, p_mid, LIVING_SDK_VERSION);

    guider_print_conn_info(conn);

    if (p_request_buf)
    {
        SYS_GUIDER_FREE(p_request_buf);
    }
    if (p_iotx_id != NULL)
    {
        SYS_GUIDER_FREE(p_iotx_id);
    }
    if (p_iotx_token != NULL)
    {
        SYS_GUIDER_FREE(p_iotx_token);
    }
    if (p_iotx_conn_host != NULL)
    {
        SYS_GUIDER_FREE(p_iotx_conn_host);
    }

    return 0;

EXIT:
    if (conn->host_name != NULL)
    {
        LITE_free(conn->host_name);
        conn->host_name = NULL;
    }
    if (conn->username != NULL)
    {
        LITE_free(conn->username);
        conn->username = NULL;
    }
    if (conn->password != NULL)
    {
        LITE_free(conn->password);
        conn->password = NULL;
    }
    if (conn->client_id != NULL)
    {
        LITE_free(conn->client_id);
        conn->client_id = NULL;
    }

    if (p_request_buf)
    {
        SYS_GUIDER_FREE(p_request_buf);
    }
    if (p_iotx_id != NULL)
    {
        SYS_GUIDER_FREE(p_iotx_id);
    }
    if (p_iotx_token != NULL)
    {
        SYS_GUIDER_FREE(p_iotx_token);
    }
    if (p_iotx_conn_host != NULL)
    {
        SYS_GUIDER_FREE(p_iotx_conn_host);
    }

    return -1;
}

int iotx_guider_authenticate(iotx_conn_info_t *conn_info)
{
    int ret = -1;
    char *p_pid = NULL;
    char *p_mid = NULL;
    char *p_guider_url = NULL;
    char *p_guider_sign = NULL;
    char *p_dynamic_mqtt_url = NULL;

    iotx_device_info_t *p_dev = NULL;
    secure_mode_e secure_mode;
    connect_method_e connect_method;

    LITE_ASSERT(conn_info);

    if (conn_info->init != 0)
    {
        sys_warning("conn info inited!");
        return 0;
    }

    p_pid = SYS_GUIDER_MALLOC(PID_STRLEN_MAX);
    if (!p_pid)
    {
        ret = -1;
        goto EXIT;
    }
    HAL_GetPartnerID(p_pid);

    p_mid = SYS_GUIDER_MALLOC(MID_STRLEN_MAX);
    if (!p_mid)
    {
        ret = -1;
        goto EXIT;
    }
    HAL_GetModuleID(p_mid);

    p_dev = SYS_GUIDER_MALLOC(sizeof(iotx_device_info_t));
    if (!p_dev)
    {
        ret = -1;
        goto EXIT;
    }
    memset(p_dev, '\0', sizeof(iotx_device_info_t));
    ret = iotx_device_info_get(p_dev);
    if (ret < 0)
    {
        ret = -1;
        goto EXIT;
    }

    p_guider_sign = SYS_GUIDER_MALLOC(GUIDER_SIGN_LEN);
    if (!p_guider_sign)
    {
        ret = -1;
        goto EXIT;
    }

    if (0 == only_use_one_time)
    {
        p_dynamic_mqtt_url = SYS_GUIDER_MALLOC(GUIDER_URL_LEN);
        if (!p_dynamic_mqtt_url)
        {
            ret = -1;
            goto EXIT;
        }

        memset(p_dynamic_mqtt_url, '\0', GUIDER_URL_LEN);

        ret = guider_get_dynamic_mqtt_url(p_dynamic_mqtt_url, GUIDER_URL_LEN - 1);
        if (ret != 0)
        {
            memset(p_dynamic_mqtt_url, '\0', GUIDER_URL_LEN);
        }

        sys_info("kv mqtt url:%s", p_dynamic_mqtt_url);

        only_use_one_time = 1;
    }

    connect_method = get_connect_method(p_dynamic_mqtt_url);
    secure_mode = guider_get_secure_mode(connect_method);
    if (CONNECT_PREAUTH == connect_method)
    {
        p_guider_url = SYS_GUIDER_MALLOC(GUIDER_URL_LEN);
        if (!p_guider_url)
        {
            ret = -1;
            goto EXIT;
        }

        ret = guider_get_conn_info(conn_info, p_dev, p_pid, p_mid, p_guider_url, p_guider_sign, secure_mode);
    }
    else
    {
        ret = direct_get_conn_info(conn_info, p_dev, p_pid, p_mid, NULL, p_guider_sign, secure_mode, p_dynamic_mqtt_url);
    }

EXIT:
    if (p_pid)
    {
        SYS_GUIDER_FREE(p_pid);
    }
    if (p_mid)
    {
        SYS_GUIDER_FREE(p_mid);
    }
    if (p_dev)
    {
        SYS_GUIDER_FREE(p_dev);
    }
    if (p_guider_sign)
    {
        SYS_GUIDER_FREE(p_guider_sign);
    }
    if (p_guider_url)
    {
        SYS_GUIDER_FREE(p_guider_url);
    }

    if (ret < 0)
    {
        sys_err("guider auth fail");
    }
    else
    {
        conn_info->init = 1;
    }

    return ret;
}
