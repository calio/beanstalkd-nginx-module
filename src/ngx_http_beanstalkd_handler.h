#ifndef NGX_HTTP_BEANSTALKD_HANDLER_H
#define NGX_HTTP_BEANSTALKD_HANDLER_H

ngx_int_t ngx_http_beanstalkd_handler(ngx_http_request_t *r);
ngx_http_upstream_srv_conf_t *ngx_http_beanstalkd_upstream_add(
        ngx_http_request_t *r, ngx_url_t *url);


#endif

