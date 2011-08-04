# vi:ft=

use lib 'lib';
use Test::Nginx::Socket;

#repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

$ENV{TEST_NGINX_BEANSTALKD_PORT} ||= 11300;

#master_on;
#worker_connections 1024;

#no_diff;

#log_level 'warn';

run_tests();

__DATA__

=== TEST 1:
--- config
    location /foo {
        beanstalkd_query delete -1;
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body eval
"NOT_FOUND\r\n"



=== TEST 2:
--- config
    location /bar {
        beanstalkd_query put 0 0 0 "hello";
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
    location /foo {
        set $id "";
        rewrite_by_lua '
            local res = ngx.location.capture("/bar")
            if (res.status == 200) then
                id = string.match(res.body, "%d+")
                ngx.var.id = id
            end
        ';
        beanstalkd_query delete $id;
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body eval
"DELETED\r\n"
