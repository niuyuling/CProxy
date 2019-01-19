# cproxy
    基于mproxy修改后的Android本地代理.  
    可以修改非Http tunnel消息头(request).


# Build
    git clone https://github.com/niuyuling/cproxy.git
    git clone https://github.com/ndevilla/iniparser.git
    cd iniparser
    make
    cd ../cproxy
    make clean; make