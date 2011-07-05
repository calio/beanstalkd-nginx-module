%%{
    machine beanstalkd_delete;

    include beanstalkd_common "beanstalkd_common.rl";

    response = "DELETED" "\r\n"
             | "NOT_FOUND" "\r\n"
             | error
             ;

    main := response @finalize
         ;

}%%
