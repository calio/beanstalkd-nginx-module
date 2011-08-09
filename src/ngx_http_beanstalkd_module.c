#define DDEBUG 0
#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

#include "ngx_http_beanstalkd_module.h"
#include "ngx_http_beanstalkd_handler.h"
#include "ngx_http_beanstalkd_response.h"
#include "ngx_http_beanstalkd_query.h"
#include "ngx_http_beanstalkd_utils.h"


static void *ngx_http_beanstalkd_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_beanstalkd_merge_loc_conf(ngx_conf_t *cf, void *parent,
        void *child);
static ngx_int_t ngx_http_beanstalkd_init(ngx_conf_t *cf);

static char *ngx_http_beanstalkd_pass(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
static char *ngx_http_beanstalkd_query(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);


ngx_uint_t ngx_http_beanstalkd_cmd_num_args[] = {
    5,
    0,
    1,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};


static ngx_flag_t ngx_http_beanstalkd_enabled = 0;


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
    return NGX_OK;
}


static char *
ngx_http_beanstalkd_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_beanstalkd_loc_conf_t  *blcf = conf;

    ngx_str_t                   *value;
    ngx_http_core_loc_conf_t    *clcf;
    ngx_uint_t                   n;
    ngx_url_t                    url;

    ngx_http_compile_complex_value_t    ccv;

    if (blcf->upstream.upstream) {
        return "is duplicate";
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = ngx_http_beanstalkd_handler;

    ngx_http_beanstalkd_enabled = 1;

    value = cf->args->elts;

    n = ngx_http_script_variables_count(&value[1]);
    if (n) {
        blcf->complex_target = ngx_palloc(cf->pool,
                sizeof(ngx_http_complex_value_t));

        if (blcf->complex_target == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
        ccv.cf = cf;
        ccv.value = &value[1];
        ccv.complex_value = blcf->complex_target;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        return NGX_CONF_OK;
    }

    blcf->complex_target = NULL;

    ngx_memzero(&url, sizeof(ngx_url_t));

    url.url = value[1];
    url.no_resolve = 1;

    blcf->upstream.upstream = ngx_http_upstream_add(cf, &url, 0);
    if (blcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }


    return NGX_CONF_OK;
}


static char *
ngx_http_beanstalkd_query(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_beanstalkd_loc_conf_t  *blcf = conf;
    ngx_str_t                       *value;
    ngx_array_t                    **query;
    ngx_uint_t                       n;
    ngx_http_complex_value_t       **arg;
    ngx_uint_t                       i;

    ngx_http_compile_complex_value_t        ccv;

    if (blcf->queries == NULL) {
        blcf->queries = ngx_array_create(cf->pool, 1, sizeof(ngx_array_t *));

        if (blcf->queries == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    query = ngx_array_push(blcf->queries);
    if (query == NULL) {
        return NGX_CONF_ERROR;
    }

    n = cf->args->nelts - 1;

    *query = ngx_array_create(cf->pool, n, sizeof(ngx_http_complex_value_t *));

    if (*query == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    for (i = 1; i <= n; i++) {
        arg = ngx_array_push(*query);
        if (arg == NULL) {
            return NGX_CONF_ERROR;
        }

        *arg = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
        if (*arg == NULL) {
            return NGX_CONF_ERROR;
        }

        if (value[i].len == 0) {
            ngx_memzero(*arg, sizeof(ngx_http_complex_value_t));
            continue;
        }

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
        ccv.cf = cf;
        ccv.value = &value[i];
        ccv.complex_value = *arg;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

