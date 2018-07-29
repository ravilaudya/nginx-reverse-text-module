/**
 * @file   ngx_http_reverse_text_request_validate.c
 * @author Ravi
 * @date   29-July-2018
 *
 * @brief  Request Validator
 *
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_reverse_text_module.h"

static ngx_int_t is_empty_request_body_input(ngx_http_request_t *r) {
    ngx_chain_t *in;
    ngx_int_t len;

    if (is_empty_request_body(r)) {
        return 1;
    }
    len = 0;
    for (in = r->request_body->bufs; in; in = in->next) {
        len += ngx_buf_size(in->buf);
    }
    return len <= 0? 1 : 0;
}

/**
 * Validate the input request
 */
ngx_int_t ngx_http_reverse_text_validate_request(ngx_http_request_t *r) {
    if (!is_request_post_only(r)) {
        return NGX_HTTP_NOT_ALLOWED;
    }
    if (is_empty_request_body_input(r) == 1) {
        return NGX_HTTP_BAD_REQUEST;
    }
    if (is_content_type_header_set(r)) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "request content type: %s", r->headers_in.content_type->value.data);
        if (!is_request_plain_text_only(r)) {
            return NGX_HTTP_BAD_REQUEST;
        }
    }
    return NGX_OK;
}
