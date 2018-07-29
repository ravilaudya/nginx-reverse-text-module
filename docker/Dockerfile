FROM ubuntu


RUN apt-get update -y && \
    apt-get install gcc -y && \
    apt-get install libunwind-dev -y && \
    apt-get install -y curl && \
    apt-get install -y build-essential checkinstall  libssl-dev && \
    apt-get install -y sudo && \
    curl -sL https://deb.nodesource.com/setup_6.x | sudo -E bash - && \
    apt-get install -y nodejs

RUN apt-get install -y wget && \
    apt-get install -y zip && \
    apt-get install -y unzip

RUN mkdir /nginx && mkdir /nginx/nginx-install && mkdir /nginx/nginx-install/cores

RUN cd /nginx && \
    wget http://nginx.org/download/nginx-1.15.2.tar.gz && \
    tar -zxvf nginx-1.15.2.tar.gz  && \
    wget https://github.com/mouryaravi/nginx-reverse-text-module/archive/master.zip -O nginx-reverse-text-module.zip && \
    unzip nginx-reverse-text-module.zip && \
    cd nginx-1.15.2 && \
    CFLAGS="-g -O0" ./configure --with-debug --add-module=../nginx-reverse-text-module-master/ --without-http_rewrite_module --without-http_gzip_module  --prefix=/nginx/nginx-install && \
    make && \
    make install

COPY ./nginx.conf /nginx/nginx-install/conf/nginx.conf
WORKDIR /nginx

EXPOSE 8080

CMD ["/nginx/nginx-install/sbin/nginx", "-g", "daemon off;"]


#docker build -t reverse-text .
#docker run -d -it -p 8080:8080 reverse-text
#docker exec -it <container> bash
