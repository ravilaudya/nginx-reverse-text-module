# nginx-reverse-text-module
NGINX module to reverse input text

## Installation
- Clone this repo [reverse_text](https://github.com/mouryaravi/nginx-reverse-text-module)
- Download and extract the [latest nginx](http://nginx.org/en/download.html)
- Configure, compile, and install nginx
```bash
git clone https://github.com/mouryaravi/nginx-reverse-text-module

wget 'http://nginx.org/download/nginx-1.15.2.tar.gz'
tar -xvzf nginx-1.15.2.tar.gz
cd nginx-1.15.2

#configure as static module
./configure --add_module=../nginx-reverse-text-module

#or...

#configure as dynamic module
./configure --add_dynamic_module=../nginx-reverse-text-module

make
make install
```
Note: You may have to pass other arguments during configure as needed

## Configuration
Add following snippet(s) to ```nginx.conf```
  - If you configured as dynamic module, load the module in the beginning of the config
```nginx
  
 ...
 load_module modules/ngx_http_reverse_text_module.so;
 ...
```
- Configuring the location (/url-path)

```nginx
  location = /reverse {
      reverse_text;
      client_body_buffer_size 1k; #nginx directive, optional
      client_body_temp_path /tmp; #nginx directive, optional
      in_memory_buffer_size   2048; #custom directive, optional
  }
```

## Directives
- [client_body_buffer_size](http://nginx.org/en/docs/http/ngx_http_core_module.html#client_body_buffer_size)
- [client_body_temp_path](http://nginx.org/en/docs/http/ngx_http_core_module.html#client_body_temp_path)
- ``in_memory_buffer_size`` (in bytes): If the client input body is larger than ``client_body_buffer_size``, partial or full data can be written to a temporary file. ``in_memory_buffer_size`` defines the maximum chunk size to be read from file and can be kept in memory before sending to client. Defaults to 2048.

## Tests
- Note: Requires [node.js](https://nodejs.org/en/download/) installed
- Assuming nginx is running at ```http://localhost:8080/```
```bash
cd test
npm install
npm test
```

## Sample Test using Curl

Assuming nginx is running at ```http://localhost:8080/```
- Sample Request

```bash
curl -X POST \
     >   http://localhost:8080/reverse \
     >    -H 'content-type: text/plain' \
     >    -d 'Write a Nginx module in C that reverse the content of a post request.'
```

- Expected Response
```bash
.tseuqer tsop a fo tnetnoc eht esrever taht C ni eludom xnigN a etirW     
```

## Limitations
- Supports only "text/plain" content type and POST requests
- Supports only ascii text