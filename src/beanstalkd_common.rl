%%{
    machine beanstalkd_common;

    action catch_err {
        dd("caught error...");
        dd("machine state: %d", cs);

        *status_addr = NGX_HTTP_BAD_GATEWAY;
    }

    msg = any* -- "\r\n";

    id = digit+;

    error_helper = "OUT_OF_MEMORY\r\n"
                 | "INTERNAL_ERROR\r\n"
                 | "BAD_FORMAT\r\n"
                 | "UNKNOWN_COMMAND\r\n"
                 ;

    error = error_helper @catch_err
          ;

    action finalize {
        dd("done it!");
        *done_addr = 1;
    }

    action check {
        dd("state %d, left %d, reading char '%c'", cs,
        (int) (pe - p), *p);
    }

    action handle_stored {
        dd("status set to 201");

        *status_addr = NGX_HTTP_CREATED;
    }

    action handle_not_found {
        dd("status set to 404");

        *status_addr = NGX_HTTP_NOT_FOUND;
    }

}%%

