#ifndef NGX_HTTP_BEANSTALKD_QUERY_H
#define NGX_HTTP_BEANSTALKD_QUERY_H

#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_beanstalkd_module.h"


ngx_int_t ngx_http_beanstalkd_build_query(ngx_http_request_t *r,
        ngx_array_t *queries, ngx_buf_t **b);

#endif
