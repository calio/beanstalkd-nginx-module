#ifndef NGX_HTTP_BEANSTALKD_MOUDLE_H
#define NGX_HTTP_BEANSTALKD_MOUDLE_H

typedef enum {
    ngx_http_beanstalkd_cmd_put = 0,
    ngx_http_beanstalkd_cmd_use,
    ngx_http_beanstalkd_cmd_reserve,
    ngx_http_beanstalkd_cmd_reserve_with_timeout,
    ngx_http_beanstalkd_cmd_delete,
    ngx_http_beanstalkd_cmd_release,
    ngx_http_beanstalkd_cmd_bury,
    ngx_http_beanstalkd_cmd_touch,
    ngx_http_beanstalkd_cmd_watch,
    ngx_http_beanstalkd_cmd_ignore,
    ngx_http_beanstalkd_cmd_peek,
    ngx_http_beanstalkd_cmd_peek_ready,
    ngx_http_beanstalkd_cmd_peek_delayed,
    ngx_http_beanstalkd_cmd_peek_buried,
    ngx_http_beanstalkd_cmd_kick,
    ngx_http_beanstalkd_cmd_stats_job,
    ngx_http_beanstalkd_cmd_stats_tube,
    ngx_http_beanstalkd_cmd_stats,
    ngx_http_beanstalkd_cmd_list_tubes,
    ngx_http_beanstalkd_cmd_list_tube_used,
    ngx_http_beanstalkd_cmd_list_tubes_watched,
    ngx_http_beanstalkd_cmd_quit,
    ngx_http_beanstalkd_cmd_unknown,
} ngx_http_beanstalkd_cmd_t;

extern ngx_uint_t ngx_http_beanstalkd_cmd_num_args[];

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
    ngx_flag_t           filter_body;
    ngx_int_t            body_length;
    ngx_int_t            rest_body_length;
    ngx_int_t            body_read;
} ngx_http_beanstalkd_ctx_t;

extern ngx_module_t ngx_http_beanstalkd_module;

#endif
