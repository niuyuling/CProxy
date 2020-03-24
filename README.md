### CProxy
    EPOLL多路复用IO, Android/Linux本地二级代理.  
    可以修改HTTP协议消息头(request).  
    可以修改HTTP协议CONNECT方法消息头.  


### Build
    Linux编译:  
    git clone https://github.com/niuyuling/CProxy.git  
    cd CProxy  
    make clean; make  
    
    windows 10子系统交叉编译:  
    apt-get install gcc-aarch64-linux-gnu  
    make clean; CROSS_COMPILE=aarch64-linux-gnu- make  
    
    Android NDK 编译:  
    ndk-build NDK_PROJECT_PATH=. NDK_APPLICATION_MK=Application.mk APP_BUILD_SCRIPT=Android.mk  

### Help Information
    CProxy proxy server
    Author: aixiao@aixiao.me
    Usage: [-?hpt] [-s signal] [-c filename]

    Options:
        -l --local_address     : localip:localport
        -f --remote_address    : remoteip:remote:port
        -p --process           : process number, default: 2
        -t --timeout           : timeout minute, default: no timeout
        -e --coding            : ssl coding, default: [0-128]
        -s --signal            : send signal to a master process: stop, quit, restart, reload, status
        -c --config            : set configuration file, default: CProxy.conf
        -? -h --? --help       : help information

    Mar 22 2020 09:29:11 Compile、link.

    #启动
    ./CProxy -c CProxy.ini
    #关闭
    ./CProxy -s stop
    #重启
    ./CProxy -s reload -c CProxy.ini
    or
    ./CProxy -s restart -c CProxy.ini
    #状态(只打印 Pid)
    ./CProxy -s status
    