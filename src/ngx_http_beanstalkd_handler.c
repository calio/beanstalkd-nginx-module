
#define DDEBUG 0
#include "ddebug.h"

#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_beanstalkd_module.h"
#include "ngx_http_beanstalkd_handler.h"
#include "ngx_http_beanstalkd_query.h"
#include "ngx_http_beanstalkd_response.h"
#include "ngx_http_beanstalkd_utils.h"


static ngx_int_t ngx_http_beanstalkd_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_reinit_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_process_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_filter_init(void *data);
static ngx_int_t ngx_http_beanstalkd_filter(void *data, ssize_t bytes);
static void ngx_http_beanstalkd_abort_request(ngx_http_request_t *r);
static void ngx_http_beanstalkd_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc);


ngx_int_t
ngx_http_beanstalkd_handler(ngx_http_request_t *r)
{
    ngx_int_t                            rc;
    ngx_http_upstream_t                 *u;
    ngx_http_beanstalkd_ctx_t           *ctx;
    ngx_http_beanstalkd_loc_conf_t      *blcf;
    ngx_str_t                            target;
    ngx_url_t                            url;
    ngx_array_t                         *cmds;
    ngx_http_beanstalkd_cmd_t           *cmd;
    ngx_array_t                        **query_args;

    dd("beanstalkd handler");
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    if (ngx_http_set_content_type(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_upstream_create(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    u = r->upstream;

    blcf = ngx_http_get_module_loc_conf(r, ngx_http_beanstalkd_module);
    if (blcf->queries == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "No beanstalkd_query specified");

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (blcf->complex_target) {
        if (ngx_http_complex_value(r, blcf->complex_target, &target)
                != NGX_OK)
        {
            return NGX_ERROR;
        }

        if (target.len == 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "handler: empty \"beanstalkd_pass\" target");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        url.host = target;
        url.port = 0;
        url.no_resolve = 1;

        blcf->upstream.upstream = ngx_http_beanstalkd_upstream_add(r, &url);

        if (blcf->upstream.upstream == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                   "beanstalkd: upstream \"%V\" not found", &target);

            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    dd("check cmd");
    cmds = ngx_http_beanstalkd_parse_cmds(r, blcf->queries);
    if (cmds == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: beanstalkd parse command error");

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    dd("check number of commands");
    /* only one command per query is supported */
    if (cmds->nelts != 1) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: only one command per query is supported");

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    dd("set queries");
    cmd = cmds->elts;
    query_args = blcf->queries->elts;

    dd("begin to check cmd args");
    if (ngx_http_beanstalkd_cmd_num_args[*cmd] != query_args[0]->nelts) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "beanstalkd: number of beanstalkd_query arguments does not equal %uz, actual number:%uz", ngx_http_beanstalkd_cmd_num_args[*cmd], query_args[0]->nelts);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    }

    ngx_str_set(&u->schema, "beanstalkd://");
    u->output.tag = (ngx_buf_tag_t) &ngx_http_beanstalkd_module;

    u->conf = &blcf->upstream;

    u->create_request = ngx_http_beanstalkd_create_request;
    u->reinit_request = ngx_http_beanstalkd_reinit_request;
    u->process_header = ngx_http_beanstalkd_process_header;
    u->abort_request = ngx_http_beanstalkd_abort_request;
    u->finalize_request = ngx_http_beanstalkd_finalize_request;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_beanstalkd_ctx_t));
    if (ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ctx->request = r;
    ctx->state = NGX_ERROR;
    ctx->cmds = cmds;

    ngx_http_set_ctx(r, ctx, ngx_http_beanstalkd_module);

    u->input_filter_init = ngx_http_beanstalkd_filter_init;
    u->input_filter = ngx_http_beanstalkd_filter;
    u->input_filter_ctx = ctx;

    r->main->count++;

    ngx_http_upstream_init(r);

    return NGX_DONE;
}

static ngx_int_t
ngx_http_beanstalkd_create_request(ngx_http_request_t *r)
{
    ngx_buf_t                           *b = NULL;
    ngx_chain_t                         *cl;
    ngx_http_beanstalkd_loc_conf_t      *blcf;
    ngx_int_t                            rc;
    ngx_http_beanstalkd_ctx_t           *ctx;

    dd("ngx_http_beanstalkd_create_request");
    ctx = ngx_http_get_module_ctx(r, ngx_http_beanstalkd_module);

    blcf = ngx_http_get_module_loc_conf(r, ngx_http_beanstalkd_module);

    if (blcf->queries) {
        ctx->query_count = blcf->queries->nelts;

        rc = ngx_http_beanstalkd_build_query(r, blcf->queries, &b);
        if (rc != NGX_OK) {
            return rc;
        }

    } else {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "No beanstalkd_query specified");

        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;
    cl->next = NULL;

    r->upstream->request_bufs = cl;

    return NGX_OK;
}


static ngx_int_t
ngx_http_beanstalkd_reinit_request(ngx_http_request_t *r)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_beanstalkd_process_header(ngx_http_request_t *r)
{
    int rc;

    dd("ngx_http_beanstalkd_process_header");
    rc = ngx_http_beanstalkd_process_simple_header(r);
    return rc;
}


static ngx_int_t
ngx_http_beanstalkd_filter_init(void *data)
{
    ngx_http_beanstalkd_ctx_t   *ctx = data;
    ngx_http_upstream_t         *u;

    dd("filter http beanstalkd filter init");

    u = ctx->request->upstream;

//    if (!ctx->filter_body) {
        u->length = 0;
//    } else {
 //       ctx->rest_body_length += (sizeof("\r\n") -1);
//    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_beanstalkd_filter(void *data, ssize_t bytes)
{
    dd("filter http beanstalkd filter");
    /*
    ngx_http_beanstalkd_ctx_t   *ctx = data;
    ngx_http_request_t          *r;
    ngx_http_upstream_t         *u;
    ngx_str_t                    resp;
    ngx_uint_t                   status;

    dd("filter http beanstalkd filter");

    r = ctx->request;
    u = ctx->request->upstream;

    resp.data = u->buffer.last;
    resp.len = bytes;
    u->buffer.last += bytes;

    if (r->headers_out.status) {
        status = r->headers_out.status;
    } else {
        status = NGX_HTTP_OK;
    }


    if (ngx_http_beanstalkd_write_simple_response(r, u, ctx, status, &resp) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
          "ngx_http_beanstalkd_write_simple_response failed in ngx_http_beanstalkd_filter");
        u->length = 0;

        return NGX_ERROR;
    }

    dd("ctx->rest_body_length = %d, resp.len = %d", (int) ctx->rest_body_length, (int) resp.len);
    ctx->rest_body_length -= (ngx_int_t) resp.len;

    if (ctx->rest_body_length == 0) {
        u->length = 0;
    } else if (ctx->rest_body_length < 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
          "ctx->rest_body_length < 0");
        u->length = 0;

        return NGX_ERROR;
    }
*/
    return NGX_OK;
}


static void
ngx_http_beanstalkd_abort_request(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "abort http beanstalkd request");
    return;
}


static void
ngx_http_beanstalkd_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http beanstalkd request");
    return;
}

ngx_http_upstream_srv_conf_t *
ngx_http_beanstalkd_upstream_add(ngx_http_request_t *r, ngx_url_t *url)
{
    ngx_http_upstream_main_conf_t  *umcf;
    ngx_http_upstream_srv_conf_t  **uscfp;
    ngx_uint_t                      i;

    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {

        if (uscfp[i]->host.len != url->host.len
            || ngx_strncasecmp(uscfp[i]->host.data, url->host.data,
               url->host.len) != 0)
        {
            dd("upstream_add: host not match");
            continue;
        }

        if (uscfp[i]->port != url->port) {
            dd("upstream_add: port not match: %d != %d",
                    (int) uscfp[i]->port, (int) url->port);
            continue;
        }

        if (uscfp[i]->default_port && url->default_port
            && uscfp[i]->default_port != url->default_port)
        {
            dd("upstream_add: default_port not match");
            continue;
        }

        return uscfp[i];
    }

    dd("no upstream found: %.*s", (int) url->host.len, url->host.data);

    return NULL;
}
