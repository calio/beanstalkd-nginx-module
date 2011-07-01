#ifndef NGX_HTTP_BEANSTALKD_MOUDLE_H
#define NGX_HTTP_BEANSTALKD_MOUDLE_H

typedef enum {
    ngx_http_beanstalkd_cmd_put,
    ngx_http_beanstalkd_cmd_unknown,
} ngx_http_beanstalkd_cmd_t;

typedef struct {
    ngx_http_upstream_conf_t     upstream;
    ngx_http_complex_value_t    *complex_target; /* for beanstalkd_pass */
    ngx_array_t                 *queries;
} ngx_http_beanstalkd_loc_conf_t;

typedef struct {
    ngx_uint_t           query_count;
    ngx_http_request_t  *request;
    ngx_int_t            state;
    ngx_array_t         *cmds;
    ngx_int_t            cmd_index;
} ngx_http_beanstalkd_ctx_t;

extern ngx_module_t ngx_http_beanstalkd_module;

#endif
