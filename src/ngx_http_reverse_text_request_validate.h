#ifndef _NGX_HTTP_REVERSE_TEXT_REQUEST_VALIDATE_H_
#define _NGX_HTTP_REVERSE_TEXT_REQUEST_VALIDATE_H_

#define is_request_plain_text_only(r)                           \
      (ngx_strcasecmp(r->headers_in.content_type->value.data, (u_char *) "text/plain") == 0)

#define is_request_post_only(r) (r->method & NGX_HTTP_POST)

#define is_empty_request_body(r)   (r->request_body == NULL || r->request_body->bufs == NULL)

#define is_content_type_header_set(r)               \
      (r->headers_in.content_type != NULL && r->headers_in.content_type->value.data != NULL)

ngx_int_t ngx_http_reverse_text_validate_request(ngx_http_request_t *r);

#endif
