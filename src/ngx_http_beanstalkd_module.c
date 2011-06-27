#define DDEBUG 1
#include "ddebug.h"

//#include "ngx_http_beanstalkd_module.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>


typedef struct {
    ngx_http_upstream_conf_t     upstream;
} ngx_http_beanstalkd_loc_conf_t;


static void *ngx_http_beanstalkd_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_beanstalkd_merge_loc_conf(ngx_conf_t *cf, void *parent,
        void *child);
static ngx_int_t ngx_http_beanstalkd_init(ngx_conf_t *cf);
static char *ngx_http_beanstalkd_pass(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
static char *ngx_http_beanstalkd_query(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
ngx_int_t ngx_http_beanstalkd_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_add_variable(ngx_conf_t *cf,
        ngx_str_t *name);
static ngx_int_t ngx_http_beanstalkd_variable_not_found(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_beanstalkd_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_reinit_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_process_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_filter_init(void *data);
static ngx_int_t ngx_http_beanstalkd_filter(void *data, ssize_t bytes);
static void ngx_http_beanstalkd_abort_request(ngx_http_request_t *r);
static void ngx_http_beanstalkd_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc);


static ngx_flag_t ngx_http_beanstalkd_enabled = 0;
static ngx_int_t  ngx_http_beanstalkd_cmd_index;


static ngx_str_t  ngx_http_beanstalkd_cmd = ngx_string("beanstalkd_cmd");


static ngx_command_t  ngx_http_beanstalkd_commands[] = {
    { ngx_string("beanstalkd_pass"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_beanstalkd_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("beanstalkd_query"),
      NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_beanstalkd_query,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};


static ngx_http_module_t  ngx_http_beanstalkd_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_beanstalkd_init,              /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */
    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_beanstalkd_create_loc_conf,   /* create location configration */
    ngx_http_beanstalkd_merge_loc_conf     /* merge location configration */
};


ngx_module_t  ngx_http_beanstalkd_module = {
    NGX_MODULE_V1,
    &ngx_http_beanstalkd_module_ctx,       /* module context */
    ngx_http_beanstalkd_commands,          /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_http_beanstalkd_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_beanstalkd_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_beanstalkd_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /* set by ngx_pcalloc():
     *
     *     conf->upstream.bufs.num = 0;
     *     conf->upstream.next_upstream = 0;
     *     conf->upstream.temp_path = NULL;
     *     conf->upstream.uri = { 0, NULL };
     *     conf->upstream.location = NULL;
     */

    conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;

    conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;

    /* the hardcoded values */
    conf->upstream.cyclic_temp_file = 0;
    conf->upstream.buffering = 0;
    conf->upstream.ignore_client_abort = 0;
    conf->upstream.send_lowat = 0;
    conf->upstream.bufs.num = 0;
    conf->upstream.busy_buffers_size = 0;
    conf->upstream.max_temp_file_size = 0;
    conf->upstream.temp_file_write_size = 0;
    conf->upstream.intercept_errors = 1;
    conf->upstream.intercept_404 = 1;
    conf->upstream.pass_request_headers = 0;
    conf->upstream.pass_request_body = 0;
    return conf;
}


static char *
ngx_http_beanstalkd_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_beanstalkd_loc_conf_t *prev = parent;
    ngx_http_beanstalkd_loc_conf_t *conf = child;

    ngx_conf_merge_msec_value(conf->upstream.connect_timeout,
                              prev->upstream.connect_timeout, 60000);

    ngx_conf_merge_msec_value(conf->upstream.send_timeout,
                              prev->upstream.send_timeout, 60000);

    ngx_conf_merge_msec_value(conf->upstream.read_timeout,
                              prev->upstream.read_timeout, 60000);

    ngx_conf_merge_size_value(conf->upstream.buffer_size,
                              prev->upstream.buffer_size,
                              (size_t) ngx_pagesize);

    ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
                              prev->upstream.next_upstream,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_UPSTREAM_FT_ERROR
                               |NGX_HTTP_UPSTREAM_FT_TIMEOUT));


    if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF) {
        conf->upstream.next_upstream = NGX_CONF_BITMASK_SET
                                       |NGX_HTTP_UPSTREAM_FT_OFF;
    }

    if (conf->upstream.upstream == NULL) {
        conf->upstream.upstream = prev->upstream.upstream;
    }

    return NGX_CONF_OK;
}


static
ngx_int_t ngx_http_beanstalkd_init(ngx_conf_t *cf)
{
    /*
    if (!ngx_http_beanstalkd_enabled) {
        return NGX_OK;
    }

    if ((ngx_http_beanstalkd_cmd_index = ngx_http_beanstalkd_add_variable(
        cf, &ngx_http_beanstalkd_cmd)) == NGX_ERROR)
    {
        return NGX_ERROR;
    }
    */
    return NGX_OK;
}


static char *
ngx_http_beanstalkd_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_beanstalkd_loc_conf_t  *blcf = conf;

    ngx_str_t                 *value;
    ngx_url_t                  u;
    ngx_http_core_loc_conf_t  *clcf;

    if (blcf->upstream.upstream) {
        return "is duplicate";
    }

    ngx_http_beanstalkd_enabled = 1;

    value = cf->args->elts;

    dd("cf->args->elts[1]:%.*s", (int)value->len, value->data);

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url = value[1];
    u.no_resolve = 1;

    blcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
    if (blcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = ngx_http_beanstalkd_handler;

    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

    return NGX_CONF_OK;

}


static char *
ngx_http_beanstalkd_query(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_beanstalkd_loc_conf_t  *blcf = conf;
    ngx_str_t                       *value;
    ngx_array_t                    **
    return NGX_CONF_OK;
}


ngx_int_t
ngx_http_beanstalkd_handler(ngx_http_request_t *r)
{
    ngx_int_t                            rc;
    ngx_http_upstream_t                 *u;
    ngx_http_beanstalkd_loc_conf_t      *blcf;

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

    ngx_str_set(&u->schema, "beanstalkd://");
    u->output.tag = (ngx_buf_tag_t) &ngx_http_beanstalkd_module;

    blcf = ngx_http_get_module_loc_conf(r, ngx_http_beanstalkd_module);

    u->conf = &blcf->upstream;

    u->create_request = ngx_http_beanstalkd_create_request;
    u->reinit_request = ngx_http_beanstalkd_reinit_request;
    u->process_header = ngx_http_beanstalkd_process_header;
    u->abort_request = ngx_http_beanstalkd_abort_request;
    u->finalize_request = ngx_http_beanstalkd_finalize_request;

    u->input_filter_init = ngx_http_beanstalkd_filter_init;
    u->input_filter = ngx_http_beanstalkd_filter;

    r->main->count++;

    ngx_http_upstream_init(r);

    return NGX_DONE;

    return NGX_OK;
}


static ngx_int_t
ngx_http_beanstalkd_add_variable(ngx_conf_t *cf, ngx_str_t *name)
{
    ngx_http_variable_t *v;

    v =ngx_http_add_variable(cf, name, NGX_HTTP_VAR_CHANGEABLE);
    if (v == NULL) {
        return NGX_ERROR;
    }

    v->get_handler = ngx_http_beanstalkd_variable_not_found;

    return ngx_http_get_variable_index(cf, name);
}


static ngx_int_t
ngx_http_beanstalkd_variable_not_found(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    v->not_found = 1;
    return NGX_OK;
}


static ngx_int_t
ngx_http_beanstalkd_create_request(ngx_http_request_t *r)
{
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
    return NGX_OK;
}


static ngx_int_t
ngx_http_beanstalkd_filter_init(void *data)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_beanstalkd_filter(void *data, ssize_t bytes)
{
    return NGX_OK;
}


static void
ngx_http_beanstalkd_abort_request(ngx_http_request_t *r)
{
}


static void
ngx_http_beanstalkd_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc)
{
}
