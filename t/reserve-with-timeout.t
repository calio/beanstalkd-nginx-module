
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
--- pre
system("killall beanstalkd");
system("beanstalkd -d");
--- config
    location /foo {
        beanstalkd_query reserve-with-timeout 4;
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body_like: ^TIMED_OUT\r\n$
--- timeout: 10


=== TEST 2:
--- pre
system("killall beanstalkd");
system("beanstalkd -d");
--- config
    location /bar {
        beanstalkd_query put 0 0 10 "\r";
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }

    location /foo {
        access_by_lua '
            ngx.location.capture("/bar")
        ';
        beanstalkd_query reserve-with-timeout 10;
        beanstalkd_pass 127.0.0.1:$TEST_NGINX_BEANSTALKD_PORT;
    }
--- request
    GET /foo
--- response_body_like: ^RESERVED \d+ 1\r\n\r\r\n$
