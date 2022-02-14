# CProxy

    EPOLL多路复用IO, Android/Linux本地二级代理.  
    可以修改HTTP协议消息头(request).  
    可以修改HTTP协议CONNECT方法消息头.  
    可以修改HTTP协议GET方法消息头.  
    HttpTCP支持IPV4/IPV6.
    支持HttpDNS、HttpUDP代理

## Build

    Linux编译:  
    git clone https://github.com/niuyuling/CProxy.git  
    cd CProxy  
    make clean; make  
    
    windows 10子系统交叉编译:  
    apt-get install gcc-aarch64-linux-gnu  
    make clean; CROSS_COMPILE=aarch64-linux-gnu- make  
    
    Android NDK 编译:  
    ndk-build NDK_PROJECT_PATH=. NDK_APPLICATION_MK=Application.mk APP_BUILD_SCRIPT=Android.mk  

## Help Information

    CProxy proxy server
    Author: aixiao@aixiao.me
    Usage: [-?h] [-s signal] [-c filename]

    Options:
        -s --signal            : send signal to a master process: stop, quit, restart, reload, status
        -c --config            : set configuration file, default: CProxy.conf
        -? -h --? --help       : help information


    #启动
    ./CProxy -c CProxy.conf
    #关闭
    ./CProxy -s stop
    #重启
    ./CProxy -s reload -c CProxy.conf
    or
    ./CProxy -s restart -c CProxy.conf
    #状态(只打印 Pid)
    ./CProxy -s status
