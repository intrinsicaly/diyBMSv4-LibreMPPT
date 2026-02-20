#pragma once
/* Mock esp_log.h for native unit test builds */
#include <stdio.h>

/* Log tags */
#define ESP_LOG_NONE    0
#define ESP_LOG_ERROR   1
#define ESP_LOG_WARN    2
#define ESP_LOG_INFO    3
#define ESP_LOG_DEBUG   4
#define ESP_LOG_VERBOSE 5

#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "[E][%s]: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "[W][%s]: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...)  /* suppress info in tests */
#define ESP_LOGD(tag, fmt, ...)  /* suppress debug in tests */
#define ESP_LOGV(tag, fmt, ...)  /* suppress verbose in tests */
