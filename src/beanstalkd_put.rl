%%{
    machine beanstalkd_storage;

    include beanstalkd_common "beanstalkd_common.rl";

    response = "INSERTED " id "\r\n" @handle_inserted
             | "BURIED " id "\r\n" @handle_buried
             | "EXPECTED_CRLF\r\n"
             | "JOB_TOO_BIG\r\n"
             | "DRAINING\r\n"
             | error
             ;

    main := response @finalize
         ;

}%%

