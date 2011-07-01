%%{
    machine beanstalkd_put;

    include beanstalkd_common "beanstalkd_common.rl";

    response = "INSERTED " id "\r\n"
             | "BURIED " id "\r\n"
             | "EXPECTED_CRLF\r\n"
             | "JOB_TOO_BIG\r\n"
             | "DRAINING\r\n"
             | error
             ;

    main := response @finalize
         ;

}%%

