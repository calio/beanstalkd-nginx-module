#define DDEBUG 0
#include "ddebug.h"

#include "ngx_http_beanstalkd_module.h"
#include "ngx_http_beanstalkd_response.h"


static u_char * parse_beanstalkd_put(int *cs_addr, u_char *p, u_char *pe,
        ngx_uint_t *status_addr, ngx_flag_t *done_addr);
static u_char * parse_beanstalkd_delete(int *cs_addr, u_char *p, u_char *pe,
        ngx_uint_t *status_addr, ngx_flag_t *done_addr);
static u_char * parse_beanstalkd_reserve(int *cs_addr, u_char *p, u_char *pe, ngx_uint_t *status_addr, ngx_flag_t *done_addr, ngx_http_beanstalkd_ctx_t *ctx);


%% machine beanstalkd_put;
%% write data;
%% machine beanstalkd_delete;
%% write data;
%% machine beanstalkd_reserve;
%% write data;


ngx_int_t
ngx_http_beanstalkd_process_simple_header(ngx_http_request_t *r)
{
    ngx_int_t                    i;
    ngx_array_t                 *cmds;
    ngx_http_beanstalkd_cmd_t   *cmd;
    ngx_int_t                    rc;
    int                          cs;
    u_char                      *p;
    u_char                      *pe;
    u_char                      *orig;
    ngx_str_t                    resp;
    ngx_http_upstream_t         *u;
    ngx_http_beanstalkd_ctx_t   *ctx;
    ngx_uint_t                   status;
    ngx_flag_t                   done = 0;
    ngx_int_t                    cmd_index;
    int                          error_state;

    dd("ngx_http_beanstalkd_process_simple_header");

    if (r->headers_out.status) {
        status = r->headers_out.status;
    } else {
        status = NGX_HTTP_OK;
    }

    dd("process simple response header");

    u = r->upstream;

    ctx = ngx_http_get_module_ctx(r, ngx_http_beanstalkd_module);

    cs = ctx->state;

    if (ctx->state == NGX_ERROR) {
        dd("reinit state");

        cmds = ctx->cmds;
        cmd = cmds->elts;

        for (i = 0; i < (int) cmds->nelts; i++) {
            switch (cmd[i]) {
                case ngx_http_beanstalkd_cmd_put:
                    dd("init beanstalkd_put machine...");

                    %% machine beanstalkd_put;
                    %% write init;

                    break;

                case ngx_http_beanstalkd_cmd_delete:
                    dd("init beanstalkd_delete machine...");

                    %% machine beanstalkd_delete;
                    %% write init;

                    break;

                case ngx_http_beanstalkd_cmd_reserve:
                case ngx_http_beanstalkd_cmd_reserve_with_timeout:
                    dd("init beanstalkd_reserve machine...");

                    %% machine beanstalkd_reserve;
                    %% write init;

                    break;

                default:
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "unrecognized beanstalkd command in "
                      "ngx_http_beanstalkd_process_simple_header");

                    u->length = 0;

                    return NGX_ERROR; /* this results in 500 status */
            }
        }
    }

    orig = u->buffer.pos;

    p = u->buffer.pos;
    pe = u->buffer.last;

    dd("buffer len: %d", (int) (pe - p));

    cmds = ctx->cmds;
    cmd = cmds->elts;
    cmd_index = ctx->cmd_index;

    switch (cmd[cmd_index]) {
        case ngx_http_beanstalkd_cmd_put:
            p = parse_beanstalkd_put(&cs, p, pe, &status, &done);

            error_state = beanstalkd_put_error;
            break;

        case ngx_http_beanstalkd_cmd_delete:
            p = parse_beanstalkd_delete(&cs, p, pe, &status, &done);

            error_state = beanstalkd_delete_error;
            break;

        case ngx_http_beanstalkd_cmd_reserve:
        case ngx_http_beanstalkd_cmd_reserve_with_timeout:
            p = parse_beanstalkd_reserve(&cs, p, pe, &status, &done, ctx);

            error_state = beanstalkd_reserve_error;
            break;

        default:
            u->length = 0;
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
                  "beanstalkd sent invalid response for command at pos %O: %V",
                  (off_t) (p - orig), &resp);

        status = NGX_HTTP_BAD_GATEWAY;
        /* u->headers_in.status_n will be the final status */
        u->headers_in.status_n = status;
        u->state->status = status;

        u->length = 0;
        return NGX_OK;
    }

    if (ctx->cmd_index >= (ngx_int_t) cmds->nelts) {

        dd("beanstalkd response parsed (resp.len: %d)", (int) resp.len);

        rc = ngx_http_beanstalkd_write_simple_response(r, u, ctx, status, &resp);

        u->length = 0;

        dd("rc = %d, u->length: %d", (int) rc, (int) u->length);
        return rc;
    }

    return NGX_AGAIN;
}

ngx_int_t
ngx_http_beanstalkd_write_simple_response(ngx_http_request_t *r,
        ngx_http_upstream_t *u, ngx_http_beanstalkd_ctx_t *ctx,
        ngx_uint_t status, ngx_str_t *resp)
{

    dd("ngx_http_beanstalkd_write_simple_response");

    ngx_chain_t             *cl, **ll;

    r->headers_out.content_length_n = resp->len;
    u->headers_in.status_n = status;
    u->state->status = status;

    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next;
    }

    cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf->flush = 1;
    cl->buf->memory = 1;
    cl->buf->pos = resp->data;
    cl->buf->last = cl->buf->pos + resp->len;

    *ll = cl;

    /* for subrequests in memory */
    u->buffer.pos = resp->data;
    u->buffer.last = resp->data + resp->len;
/*   ctx->body_length = resp->len; */

    return NGX_OK;
}

static u_char *
parse_beanstalkd_put(int *cs_addr, u_char *p, u_char *pe, ngx_uint_t *status_addr, ngx_flag_t *done_addr)
{
    int cs = *cs_addr;

    %% machine beanstalkd_put;
    %% include "beanstalkd_put.rl";
    %% write exec;

    *cs_addr = cs;

    return p;
}

static u_char *
parse_beanstalkd_delete(int *cs_addr, u_char *p, u_char *pe, ngx_uint_t *status_addr, ngx_flag_t *done_addr)
{
    int cs = *cs_addr;

    %% machine beanstalkd_delete;
    %% include "beanstalkd_delete.rl";
    %% write exec;

    *cs_addr = cs;

    return p;
}

static u_char *
parse_beanstalkd_reserve(int *cs_addr, u_char *p, u_char *pe, ngx_uint_t *status_addr, ngx_flag_t *done_addr, ngx_http_beanstalkd_ctx_t *ctx)
{
    int cs = *cs_addr;

    %% machine beanstalkd_reserve;
    %% include "beanstalkd_reserve.rl";
    %% write exec;

    *cs_addr = cs;

    return p;
}
