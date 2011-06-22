# vi:ft=

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

$ENV{TEST_NGINX_BEANSTALKD_PORT} ||= 11300;

#master_on;
#worker_connections 1024;

#no_diff;

log_level 'warn';

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

=== TEST 2: empty query
--- config
    location /foo {
        beanstalkd_literal_raw_query "";
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_REDIS_PORT;
    }
--- request
    GET /foo
--- response_body_like: 500 Internal Server Error
--- error_code: 500
--- SKIP



=== TEST 3: simple put query
--- config
    location /foo {
        beanstalkd_query put one first;
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_REDIS_PORT;
    }
--- request
    GET /foo
--- response_body eval
"INSERTED 1\r\n"

