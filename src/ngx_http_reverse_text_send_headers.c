/**
 * @file   ngx_http_reverse_text_send_headers.c
 * @author Ravi
 * @date   29-July-2018
 *
 * @brief  Headers Sender
 *
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_reverse_text_module.h"

#define DEAULT_CONTENT_TYPE  "text/plain"

/**
 * Send content length, content type and status headers
 */
ngx_int_t reverse_text_send_headers(ngx_http_request_t *r) {
    ngx_int_t    rc, len, buffers_count;
    ngx_chain_t  *in;

    len = 0;
    buffers_count = 0;

    for (in = r->request_body->bufs; in; in = in->next) {
        buffers_count++;
        len += ngx_buf_size(in->buf);
    }
    r->headers_out.content_type.len = sizeof(DEAULT_CONTENT_TYPE) - 1;
    r->headers_out.content_type.data = (u_char *) DEAULT_CONTENT_TYPE;
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = len;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "Sent content length header with value: %d, total buffers: %d", len, buffers_count);
    rc = ngx_http_send_header(r);

    return rc;
}

