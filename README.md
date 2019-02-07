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