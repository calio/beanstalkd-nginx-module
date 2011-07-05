%%{
    machine beanstalkd_reserve;

    include beanstalkd_common "beanstalkd_common.rl";

    action check_data_complete {
        ctx->body_read == ctx->body_length + (ngx_int_t) (sizeof("\n") -1)
    }

    response = "DEADLINE_SOON" "\r\n"
             | "TIMED_OUT" "\r\n"
             | "RESERVED " id " " num @add_digit "\r\n" data "\r" when check_data_complete "\n" @filter_body
             | error
             ;

    main := response @finalize
         ;

}%%
