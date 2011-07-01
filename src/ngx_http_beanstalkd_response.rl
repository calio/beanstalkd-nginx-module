#define DDEBUG 0
#include "ddebug.h"


%% machine beanstalkd_put;
%% write data;

ngx_int_t
ngx_http_beanstalkd_process_simple_header(ngx_http_request_t *r)
{
    ngx_uint_t                   status;
    ngx_int_t                    i;
    ngx_array_t                 *cmds;
    ngx_http_beanstalkd_cmd_t   *cmd;

    ngx_int_t                rc;
    int                      cs;
    u_char                  *p;
    u_char                  *pe;
    u_char                  *orig;
    ngx_str_t                resp;
    ngx_http_upstream_t     *u;
    ngx_http_memc_ctx_t     *ctx;
    ngx_uint_t               status;
    ngx_flag_t               done = 0;
    int                      error_state;
    int                      final_state;

    int                      cmd_index;

    if (r->headers_out.status) {
        status = r->headers_out.status;
    } else {
        status = NGX_HTTP_OK;
    }

    dd("process simple response heander");

    ctx = ngx_http_get_module_ctx(r, ngx_http_beanstalkd_module);

    if (ctx->state == NGX_ERROR) {
        dd("reinit state");

        cmds = ctx->cmds;
        cmd = cmds->elts;

        for (i = 0; i < cmds->nelts; i++) {
            switch (cmd[i]) {
                case ngx_http_beanstalkd_cmd_put:
                    dd("init beanstalkd_put machine...");

                    %% machine beanstalkd_put;
                    %% write init;

                    error_state = beanstalkd_put_error;
                    final_state = beanstalkd_put_final;

                    break;

                default:
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "unrecognized beanstalkd command in "
                      "ngx_http_beanstalkd_process_simple_header");

                    return NGX_ERROR; /* this results in 500 status */
            }
        }
    } else {
        cs = ctx->state;
    }

    u = r->upstream;

    orig = u->buffer.pos;

    p = u->buffer.pos;
    pe = u->buffer.last;

    dd("buffer len: %d", pe - p);

    cmd = cmds->elts;
    cmd_index = ctx->cmd_index;

    switch (cmd[cmd_index]) {
        case ngx_http_beanstalkd_cmd_put:
            p = parse_beanstalkd_put(&cs, p, pe, &status, &done);
            break;

        default:
            return NGX_ERROR;
    }

    ctx->state = cs;

    resp.data = u->buffer.start;
    resp.len  = p - resp.data;

    u->buffer.pos = p;

    if (done) {
        ctx->cmd_index++;
    }

    if (cs  == error_state) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "beanstalkd sent invalid response for command \"%V\" at pos %O: %V",
                  &ctx->cmd_str, (off_t) (p - orig), &resp);

        status = NGX_HTTP_BAD_GATEWAY;
        u->headers_in.status_n = status;
        u->state->status = status;

        /* u->headers_in.status_n will be the final status */
        return NGX_OK;
    }

    if (ctx->cmd_index >= cmd->nelts) {

        dd("beanstalkd response parsed (resp.len: %d)", resp.len);

        rc = ngx_http_beanstalkd_write_simple_response(r, u, ctx, status, &resp);

        return rc;
    }

    return NGX_AGAIN;
}

static ngx_int_t
ngx_http_beanstalkd_write_simple_response(ngx_http_request_t *r,
        ngx_http_upstream_t *u, ngx_http_beanstalkd_ctx_t *ctx,
        ngx_uint_t status, ngx_str_t *resp)
{
    return NGX_OK;
}

u_char *
beanstalkd_parse_put(int *cs_addr, u_char *p, u_char *pe, ngx_uint_t *status_addr, ngx_flag_t *done_addr)
{
    int cs = *cs_addr;

    %% machine beanstalkd_put;
    %% include "beanstalkd_put.rl";
    %% write exec;

    *cs_addr = cs;

    return p;
}
