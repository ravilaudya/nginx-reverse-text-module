

#define is_request_plain_text_only(r)                           \
      (ngx_strcasecmp(r->headers_in.content_type->value.data, (u_char *) "text/plain") == 0)
