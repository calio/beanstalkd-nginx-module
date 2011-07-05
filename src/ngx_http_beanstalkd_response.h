
#ifndef NGX_HTTP_BEANSTALKD_RESPONSE_H
#define NGX_HTTP_BEANSTALKD_RESPONSE_H

#include <ngx_core.h>
#include <ngx_http.h>

ngx_int_t ngx_http_beanstalkd_process_simple_header(ngx_http_request_t *r);

ngx_int_t ngx_http_beanstalkd_write_simple_response(ngx_http_request_t *r,
        ngx_http_upstream_t *u, ngx_http_beanstalkd_ctx_t *ctx,
        ngx_uint_t status, ngx_str_t *resp);


#endif
