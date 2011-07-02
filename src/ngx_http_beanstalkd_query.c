#include "ddebug.h"
#include "ngx_http_beanstalkd_query.h"
#include "ngx_http_beanstalkd_utils.h"


static ngx_int_t ngx_http_beanstalkd_build_put_query(ngx_http_request_t *r,
        ngx_array_t *queries, ngx_buf_t **b);


ngx_int_t
ngx_http_beanstalkd_build_query(ngx_http_request_t *r,
        ngx_array_t *queries, ngx_buf_t **b)
{
    ngx_array_t                     *cmds;
    ngx_http_beanstalkd_cmd_t       *cmd;
    ngx_array_t                    **query_args;
    ngx_http_beanstalkd_ctx_t       *ctx;

    dd("ngx_http_beanstalkd_build_query");
    cmds = ngx_http_beanstalkd_parse_cmds(r, queries);
    if (cmds == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: beanstalkd parse command error");

        return NGX_ERROR;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_beanstalkd_module);

    ctx->cmds = cmds;

    /* only one command per query is supported */
    if (cmds->nelts != 1) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: only one command per query is supported");

        return NGX_ERROR;
    }

    cmd = cmds->elts;
    query_args = queries->elts;

    switch (*cmd) {
        case ngx_http_beanstalkd_cmd_put:
            if (ngx_http_beanstalkd_build_put_query(r, query_args[0], b) != NGX_OK) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "beanstalkd: ngx_http_beanstalkd_build_put_query failed");

                return NGX_ERROR;
            }
            break;
        default:
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: parsed query command not recognized");

            return NGX_ERROR;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_beanstalkd_build_put_query(ngx_http_request_t *r,
        ngx_array_t *query_args, ngx_buf_t **b)
{
    ngx_uint_t                       i;
    ngx_str_t                       *arg;
    ngx_array_t                     *args;
    ngx_http_complex_value_t       **complex_arg;
    size_t                           len = 0;
    u_char                          *p;

    dd("ngx_http_beanstalkd_build_put_query");

    dd("query_args->nelts:%d", (int) query_args->nelts);

    args = ngx_array_create(r->pool, query_args->nelts, sizeof(ngx_str_t));

    if (args == NULL) {
        return NGX_ERROR;
    }

    complex_arg = query_args->elts;

    /* beanstalkd_query put <pri> <delay> <ttr> $job */
    if (query_args->nelts != 5) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "beanstalkd: number of beanstalkd_query arguments does not equal 5, actual number:%uz", query_args->nelts);

        return NGX_ERROR;
    }

    /* calculate the length of bufer needed to store the request commands */
    for (i = 0; i < query_args->nelts; i++) {
        arg = ngx_array_push(args);
        if (arg == NULL) {
            return NGX_ERROR;
        }

        if (ngx_http_complex_value(r, complex_arg[i], arg) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    arg = args->elts;

    len += arg[0].len
         + arg[1].len
         + arg[2].len
         + arg[3].len
         + ngx_get_num_size(arg[4].len)
         + arg[4].len
         + (sizeof(" ") - 1) * 4
         + (sizeof("\r\n") - 1) * 2;

    dd("len = %d", (int)len);

    *b = ngx_create_temp_buf(r->pool, len);
    if (*b == NULL) {
        return NGX_ERROR;
    }

    p = (*b)->pos;

    /* fill the buffer with each command */
    for (i = 0; i < 4; i++) {
        p = ngx_copy(p, arg[i].data, arg[i].len);
        *p++ = ' ';
    }

    p = ngx_sprintf(p, "%uz", arg[4].len);
    *p++ = '\r'; *p++ = '\n';

    p = ngx_copy(p, arg[4].data, arg[4].len);
    *p++ = '\r'; *p++ = '\n';

    dd("query:%.*s", (int) (p - (*b)->pos), (*b)->pos);

    if (p - (*b)->pos != (ssize_t) len) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: beanstalkd_query buffer error %uz != %uz",
                (size_t) (p - (*b)->pos), len);

        return NGX_ERROR;
    }

    (*b)->last = p;

    return NGX_OK;
}

