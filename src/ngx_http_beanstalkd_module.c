#define DDEBUG 1
#include "ddebug.h"

//#include "ngx_http_beanstalkd_module.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>


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
} ngx_http_beanstalkd_ctx_t;

static void *ngx_http_beanstalkd_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_beanstalkd_merge_loc_conf(ngx_conf_t *cf, void *parent,
        void *child);
static ngx_int_t ngx_http_beanstalkd_init(ngx_conf_t *cf);
static char *ngx_http_beanstalkd_pass(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
static char *ngx_http_beanstalkd_query(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);
ngx_int_t ngx_http_beanstalkd_handler(ngx_http_request_t *r);
//static ngx_int_t ngx_http_beanstalkd_add_variable(ngx_conf_t *cf,
//        ngx_str_t *name);
//static ngx_int_t ngx_http_beanstalkd_variable_not_found(ngx_http_request_t *r,
//    ngx_http_variable_value_t *v, uintptr_t data);
ngx_http_upstream_srv_conf_t *ngx_http_beanstalkd_upstream_add(
        ngx_http_request_t *r, ngx_url_t *url);

static ngx_int_t ngx_http_beanstalkd_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_reinit_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_process_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_beanstalkd_filter_init(void *data);
static ngx_int_t ngx_http_beanstalkd_filter(void *data, ssize_t bytes);
static void ngx_http_beanstalkd_abort_request(ngx_http_request_t *r);
static void ngx_http_beanstalkd_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc);

ngx_int_t ngx_http_beanstalkd_build_query(ngx_http_request_t *r,
        ngx_array_t *queries, ngx_buf_t **b);
ngx_array_t *ngx_http_beanstalkd_parse_cmds(ngx_http_request_t *r, ngx_array_t
        *queries);
ngx_int_t ngx_http_beanstalkd_build_put_query(ngx_http_request_t *r,
        ngx_array_t *queries, ngx_buf_t **b);
ngx_http_beanstalkd_cmd_t ngx_http_beanstalkd_parse_cmd(ngx_str_t cmd);

static size_t ngx_get_num_size(uint64_t i);

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

    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

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


ngx_int_t
ngx_http_beanstalkd_handler(ngx_http_request_t *r)
{
    ngx_int_t                            rc;
    ngx_http_upstream_t                 *u;
    ngx_http_beanstalkd_ctx_t           *ctx;
    ngx_http_beanstalkd_loc_conf_t      *blcf;
    ngx_str_t                            target;
    ngx_url_t                            url;

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

    ngx_http_set_ctx(r, ctx, ngx_http_beanstalkd_module);

    u->input_filter_init = ngx_http_beanstalkd_filter_init;
    u->input_filter = ngx_http_beanstalkd_filter;
    u->input_filter_ctx = ctx;

    r->main->count++;

    ngx_http_upstream_init(r);

    return NGX_DONE;
}

/*
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
}*/

/*
static ngx_int_t
ngx_http_beanstalkd_variable_not_found(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    v->not_found = 1;
    return NGX_OK;
}*/

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

static ngx_int_t
ngx_http_beanstalkd_create_request(ngx_http_request_t *r)
{
    ngx_buf_t                           *b = NULL;
    ngx_chain_t                         *cl;
    ngx_http_beanstalkd_loc_conf_t      *blcf;
    //ngx_str_t                            query;
    //ngx_str_t                            query_count;
    ngx_int_t                            rc;
    ngx_http_beanstalkd_ctx_t           *ctx;
    //ngx_int_t                            n;

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

ngx_int_t
ngx_http_beanstalkd_build_query(ngx_http_request_t *r,
        ngx_array_t *queries, ngx_buf_t **b)
{
    ngx_array_t                     *cmds;
    ngx_http_beanstalkd_cmd_t       *cmd;
    ngx_array_t                    **query_args;

    dd("ngx_http_beanstalkd_build_query");
    cmds = ngx_http_beanstalkd_parse_cmds(r, queries);
    if (cmds == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: beanstalkd parse command error");

        return NGX_ERROR;
    }

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

ngx_int_t
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

static size_t
ngx_get_num_size(uint64_t i)
{
    size_t          n = 0;

    do {
        i = i / 10;
        n++;
    } while (i > 0);

    return n;
}

ngx_array_t *
ngx_http_beanstalkd_parse_cmds(ngx_http_request_t *r, ngx_array_t
        *queries)
{
    ngx_int_t                    n, i;
    ngx_array_t                 *cmds, **query;
    ngx_http_complex_value_t   **query_cmd;
    ngx_str_t                    query_cmd_str;
    ngx_http_beanstalkd_cmd_t   *cmd;

    n = queries->nelts;

    cmds = ngx_array_create(r->pool, n, sizeof(ngx_http_beanstalkd_cmd_t));
    if (cmds == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: can't create cmds array, ngx_array_create failed");

        return NULL;
    }

    query = queries->elts;
    for (i = 0; i < n; i++) {
        query_cmd = query[i]->elts;

        dd("i=%d", (int) i);
        cmd = ngx_array_push(cmds);
        if (cmd == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: can't push from cmd array, ngx_array_push failed");
        }

        if (ngx_http_complex_value(r, query_cmd[0], &query_cmd_str) != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "beanstalkd: can't get complex value, ngx_http_complex_value failed");
            return NULL;
        }
        dd("query_cmd_str:%.*s", (int) query_cmd_str.len, query_cmd_str.data);

        *cmd = ngx_http_beanstalkd_parse_cmd(query_cmd_str);
    }

    return cmds;
}

ngx_http_beanstalkd_cmd_t
ngx_http_beanstalkd_parse_cmd(ngx_str_t cmd)
{
    switch (cmd.len) {
        case 3:
            if (ngx_strncmp(cmd.data, "put", 3) == 0) {
                return ngx_http_beanstalkd_cmd_put;
            } else {
                return ngx_http_beanstalkd_cmd_unknown;
            }
            break;

        default:
            return ngx_http_beanstalkd_cmd_unknown;
    }

    return ngx_http_beanstalkd_cmd_unknown;
}
