# cproxy
    Android本地二级代理.  
    可以修改HTTP协议消息头(request).  
    可以修改HTTP协议CONNECT方法消息头.  


# Build
    git clone https://github.com/niuyuling/cproxy.git
    git clone https://github.com/ndevilla/iniparser.git
    cd iniparser
    make
    cd ../cproxy
    make clean; make
    
# Help Information
    cproxy proxy server
    Author: aixiao@aixiao.me
    Usage: [-?hdsc] [-s signal] [-c filename]

    Options:
        -?,-h       : help information
        -d          : daemon
        -s signal   : send signal to a master process: stop
        -c filename : set configuration file (default: conf/cproxy.ini)

    May  7 2019 18:48:10 Compile、link.
