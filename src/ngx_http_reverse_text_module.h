#ifndef _NGX_HTTP_REVERSE_TEXT_MODULE_H_
#define _NGX_HTTP_REVERSE_TEXT_MODULE_H_

#include "ngx_http_reverse_text_request_validate.h"

#define DEFAULT_BUFFER_SIZE     (4 * 1024)

typedef struct {
    ngx_flag_t reverse_text;
    ngx_uint_t  in_memory_buffer_size;
} ngx_http_reverse_text_loc_conf_t;

typedef struct {
    ngx_uint_t next_chunk_size;
    ngx_uint_t remaining_size;
    ngx_uint_t offset;
} ngx_file_chunk_t;


ngx_int_t reverse_text_send_headers(ngx_http_request_t *r);
ngx_int_t reverse_text_send_body(ngx_http_request_t *r, ngx_http_reverse_text_loc_conf_t *rtcf);

#endif