#define DDEBUG 0
#include "ddebug.h"

#include "ngx_http_beanstalkd_module.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>


typedef struct {
    ngx_http_upstream_t     upstream;
} ngx_http_beanstalkd_loc_conf_t;


static void *ngx_http_beanstalkd_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_beanstalkd_merge_loc_conf(ngx_conf_t *cf, void *parent,
        void *child);
ngx_int_t ngx_http_beanstalkd_init(ngx_conf_t *cf);
static char *ngx_http_beanstalkd_pass(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
ngx_int_t ngx_http_beanstalkd_handler(ngx_http_request_t *r);


static ngx_command_t  ngx_http_beanstalkd_commands[] = {
    { ngx_string("beanstalkd_pass"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_beanstalkd_pass,
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
    ngx_http_beanstalkd_conf_t *conf;

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
    ngx_http_beanstalkd_loc_conf_t *pref = parent;
    ngx_http_beanstalkd_loc_conf_t *conf = child;

    if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF) {
        conf->upstream.next_upstream = NGX_CONF_BITMASK_SET
                                       |NGX_HTTP_UPSTREAM_FT_OFF;
    }

    if (conf->upstream.upstream == NULL) {
        conf->upstream.upstream = prev->upstream.upstream;
    }

    return NGX_CONF_OK;
}


ngx_int_t
ngx_http_beanstalkd_init(ngx_conf_t *cf)
{
    return NGX_OK;
}


static char *
ngx_http_beanstalkd_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_beanstalkd_loc_conf_t  *blcf = conf;
    ngx_http_core_loc_conf_t        *clcf;

    if (blcf->upstream.upstream) {
        return "is duplicated";
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = ngx_http_beanstalkd_handler;

    return NGX_CONF_OK;
}


ngx_int_t
ngx_http_beanstalkd_handler(ngx_http_request_t *r)
{
    dd("beanstalkd handler");
    return NGX_OK;
}
