#ifndef DIYBMS_WEBSERVER_JSON_MPPT_H_
#define DIYBMS_WEBSERVER_JSON_MPPT_H_

#include <esp_http_server.h>

esp_err_t content_handler_mppt(httpd_req_t *req);
esp_err_t post_savemppt_json_handler(httpd_req_t *req, bool urlEncoded);
esp_err_t post_mpptcontrol_json_handler(httpd_req_t *req, bool urlEncoded);

#endif
