
#define DDEBUG 0
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
        if (*cmd == ngx_http_beanstalkd_cmd_unknown) {
            dd("unkonwn command");
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "meet unkonwn command %V", &query_cmd_str);
            return NULL;
        }
        dd("parse command '%.*s' done", (int) query_cmd_str.len, query_cmd_str.data);
    }

    return cmds;
}

ngx_http_beanstalkd_cmd_t
ngx_http_beanstalkd_parse_cmd(ngx_str_t cmd)
{
    dd("ngx_http_beanstalkd_parse_cmd");

    switch (cmd.len) {
        case 3:
            if (ngx_strncmp(cmd.data, "put", 3) == 0) {
                return ngx_http_beanstalkd_cmd_put;
            } else if (ngx_strncmp(cmd.data, "use", 3) == 0 ) {
                return ngx_http_beanstalkd_cmd_use;
            } else {
                return ngx_http_beanstalkd_cmd_unknown;
            }
            break;

        case 4:
            if (ngx_strncmp(cmd.data, "bury", 4) == 0) {
                return ngx_http_beanstalkd_cmd_use;
            } else if (ngx_strncmp(cmd.data, "peek", 4) == 0) {
                return ngx_http_beanstalkd_cmd_peek;
            } else if (ngx_strncmp(cmd.data, "kick", 4) == 0) {
                return ngx_http_beanstalkd_cmd_kick;
            } else if (ngx_strncmp(cmd.data, "quit", 4) == 0) {
                return ngx_http_beanstalkd_cmd_quit;
            } else {
                return ngx_http_beanstalkd_cmd_unknown;
            }

        case 5:
            if (ngx_strncmp(cmd.data, "touch", 5) == 0) {
                return ngx_http_beanstalkd_cmd_touch;
            } else if (ngx_strncmp(cmd.data, "watch", 5) == 0) {
                return ngx_http_beanstalkd_cmd_watch;
            } else if (ngx_strncmp(cmd.data, "stats", 5) == 0) {
                return ngx_http_beanstalkd_cmd_stats;
            } else {
                return ngx_http_beanstalkd_cmd_unknown;
            }

        case 6:
            if (ngx_strncmp(cmd.data, "delete", 6) == 0) {
                return ngx_http_beanstalkd_cmd_delete;
            } else if (ngx_strncmp(cmd.data, "ignore", 6) == 0) {
                return ngx_http_beanstalkd_cmd_ignore;
            } else {
                return ngx_http_beanstalkd_cmd_unknown;
            }
            break;

        case 7:
            if (ngx_strncmp(cmd.data, "release", 7) == 0) {
                return ngx_http_beanstalkd_cmd_release;
            } else if (ngx_strncmp(cmd.data, "reserve", 7) == 0) {
                return ngx_http_beanstalkd_cmd_reserve;
            } else {
                return ngx_http_beanstalkd_cmd_unknown;
            }
            break;

        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 14:
        case 18:
        case 20:
            if (ngx_strncmp(cmd.data, "reserve-with-timeout", 20) == 0) {
                return ngx_http_beanstalkd_cmd_reserve_with_timeout;
            } else if (ngx_strncmp(cmd.data, "peek-ready", 10) == 0) {
                return ngx_http_beanstalkd_cmd_peek_ready;
            } else if (ngx_strncmp(cmd.data, "peek-delayed", 12) == 0) {
                return ngx_http_beanstalkd_cmd_peek_delayed;
            } else if (ngx_strncmp(cmd.data, "peek-buried",11) == 0) {
                return ngx_http_beanstalkd_cmd_peek_buried;
            } else if (ngx_strncmp(cmd.data, "stats-job", 9) == 0) {
                return ngx_http_beanstalkd_cmd_stats_job;
            } else if (ngx_strncmp(cmd.data, "stats-tube", 10) == 0) {
                return ngx_http_beanstalkd_cmd_stats_tube;
            } else if (ngx_strncmp(cmd.data, "list-tubes", 10) == 0) {
                return ngx_http_beanstalkd_cmd_list_tubes;
            } else if (ngx_strncmp(cmd.data, "list-tube-used", 14) == 0) {
                return ngx_http_beanstalkd_cmd_list_tube_used;
            } else if (ngx_strncmp(cmd.data, "list-tube-watched", 18) == 0) {
                return ngx_http_beanstalkd_cmd_list_tubes_watched;
            } else {
                return ngx_http_beanstalkd_cmd_unknown;
            }
            break;

        default:
            dd("unkonwn command:%.*s", (int) cmd.len, cmd.data);
            return ngx_http_beanstalkd_cmd_unknown;
    }

    dd("ngx_http_beanstalkd_parse_cmd done");
    return ngx_http_beanstalkd_cmd_unknown;
}
