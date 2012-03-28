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

=== TEST 1: no query
--- config
    location /foo {
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body_like: 500 Internal Server Error
--- error_code: 500



=== TEST 2: simple put query 3, beanstalkd_pass to an upstream
--- http_config
    upstream backend {
        server 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- config
    location /foo {
        set $cmd "put";
        set $dest "backend";
        beanstalkd_query $cmd 1 1 1 "";
        beanstalkd_pass $dest;
    }
--- request
    GET /foo
--- response_body_like: ^INSERTED \d+\r\n$



=== TEST 3: simple put query 3, beanstalkd_pass to multi upstream
--- http_config
    upstream backend {
        server 127.0.0.1:3306;
        server 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- config
    location /foo {
        set $cmd "put";
        set $dest "backend";
        beanstalkd_query $cmd 1 1 1 "";
        beanstalkd_pass $dest;
    }
--- request
    GET /foo
--- response_body_like: ^INSERTED \d+\r\n$



=== TEST 4: simple put query 3, beanstalk_pass destination is empty
--- http_config
    upstream backend {
        server 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- config
    location /foo {
        set $cmd "put";
        set $dest "";
        beanstalkd_query $cmd 1 1 1 "";
        beanstalkd_pass $dest;
    }
--- request
    GET /foo
--- response_body_like: 500 Internal Server Error
--- error_code: 500



=== TEST 5: unknown command query 3
--- config
    location /foo {
        set $cmd "put-cmd";
        beanstalkd_query $cmd 1 1 1 "";
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body_like: 500 Internal Server Error
--- error_code: 500



=== TEST 6: unknown command query 3
--- config
    location /foo {
        set $cmd "put";
        beanstalkd_query "bad $cmd" 1 1 1 "";
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body_like: 500 Internal Server Error
--- error_code: 500



=== TEST 7: unknown command query 3
--- config
    location /foo {
        set $cmd "put";
        beanstalkd_query "bad $cmd" 1 1 1 "";
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body_like: 500 Internal Server Error
--- error_code: 500



=== TEST 8: more than 1 command1 query
--- config
    location /foo {
        beanstalkd_query put 1 1 1 "one";
        beanstalkd_query put 1 1 1 "two";
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body_like: 500 Internal Server Error
--- error_code: 500



=== TEST 9: wrong command arguments
--- config
    location /foo {
        beanstalkd_query put 1 1 "one";
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body_like: 500 Internal Server Error
--- error_code: 500


