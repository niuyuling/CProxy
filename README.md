### CProxy
    EPOLL多路复用IO, Android/Linux本地二级代理.  
    可以修改HTTP协议消息头(request).  
    可以修改HTTP协议CONNECT方法消息头.  


### Build
    git clone https://github.com/niuyuling/cproxy.git  
    git clone https://github.com/ndevilla/iniparser.git  
    cd iniparser  
    make  
    cd ../cproxy  
    make clean; make  
    
    windows 10子系统交叉编译:  
    apt-get install gcc-aarch64-linux-gnu  
    make clean; CROSS_COMPILE=aarch64-linux-gnu- make  

### Help Information
    ./CProxy -h
    CProxy proxy server
    Author: aixiao@aixiao.me
    Usage: [-?hpt] [-s signal] [-c filename]

    Options:
        -?,-h       : help information
        -p          : process number, default 2 process
        -t          : timeout minute, default is no timeout
        -s signal   : send signal to a master process: stop, quit, restart, reload, status
        -c filename : set configuration file (default: cproxy.ini)

    Jan 13 2020 19:56:06 Compile、link.

    #启动
    ./CProxy -c CProxy.ini
    #关闭
    ./CProxy -s stop
    #重启
    ./CProxy -s reload -c CProxy.ini
    or
    ./CProxy -s restart -c CProxy.ini
    #状态
    ./CProxy -s status
    