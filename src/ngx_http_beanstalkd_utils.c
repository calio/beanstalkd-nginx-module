
#define DDEBUG 1
#include "ddebug.h"
#include "ngx_http_beanstalkd_utils.h"


size_t
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
