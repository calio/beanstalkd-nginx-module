#ifndef NGX_HTTP_BEANSTALKD_UTILS_H
#define NGX_HTTP_BEANSTALKD_UTILS_H


#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_beanstalkd_module.h"

size_t ngx_get_num_size(uint64_t i);
ngx_array_t *ngx_http_beanstalkd_parse_cmds(ngx_http_request_t *r, ngx_array_t
        *queries);
ngx_http_beanstalkd_cmd_t ngx_http_beanstalkd_parse_cmd(ngx_str_t cmd);

#endif
