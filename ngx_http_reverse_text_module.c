/**
 * @file   ngx_http_reverse_text_module.c
 * @author Ravi
 * @date   27-July-2018
 *
 * @brief  Nginx module to reverse input text
 *
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static char *ngx_http_reverse_text(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_reverse_text_handler(ngx_http_request_t *r);

/**
 * This module provided directive: hello world.
 *
 */
static ngx_command_t ngx_http_reverse_text_commands[] = {

    { ngx_string("reverse_text"), /* directive */
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS, /* location context and takes
                                            no arguments*/
      ngx_http_reverse_text, /* configuration setup function */
      0, /* No offset. Only one context is supported. */
      0, /* No offset when storing the module configuration on struct. */
      NULL},

    ngx_null_command /* command termination */
};

/* The module context. */
static ngx_http_module_t ngx_http_reverse_text_module_ctx = {
    NULL, /* preconfiguration */
    NULL, /* postconfiguration */

    NULL, /* create main configuration */
    NULL, /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    NULL, /* create location configuration */
    NULL /* merge location configuration */
};

/* Module definition. */
ngx_module_t ngx_http_reverse_text_module = {
    NGX_MODULE_V1,
    &ngx_http_reverse_text_module_ctx, /* module context */
    ngx_http_reverse_text_commands, /* module directives */
    NGX_HTTP_MODULE, /* module type */
    NULL, /* init master */
    NULL, /* init module */
    NULL, /* init process */
    NULL, /* init thread */
    NULL, /* exit thread */
    NULL, /* exit process */
    NULL, /* exit master */
    NGX_MODULE_V1_PADDING
};

/**
 * Send content length, content type and status headers
 */
static ngx_int_t reverse_text_send_headers(ngx_http_request_t *r) {
    ngx_int_t    rc, len;
    ngx_chain_t  *in;

    len = 0;

    for (in = r->request_body->bufs; in; in = in->next) {
        len += ngx_buf_size(in->buf);
    }
    r->headers_out.content_type.len = sizeof("text/plain") - 1;
    r->headers_out.content_type.data = (u_char *) "text/plain";
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = len;

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Sent content length header with value: %d", len);
    rc = ngx_http_send_header(r);

    return rc;
}

/**
 * Inplace string reverse in buffer
 */
static void in_place_string_reverse(ngx_http_request_t *r, ngx_buf_t *buffer) {
    unsigned char *lhs = buffer->start, *rhs = buffer->last;
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "About to reverse string: %s", lhs);
    if (!lhs || !rhs || !*lhs || !*(lhs+1)) {
        return;
    }

    while (lhs < --rhs) {
        unsigned char tmp = *lhs;
        *lhs++ = *rhs;
        *rhs = tmp;
    }
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "After reverse: %s", lhs);
}

/**
 * Reverse the current buffer and send to client
 */
static ngx_int_t send_current_buffer(ngx_http_request_t *r, ngx_buf_t  *buf) {
    ngx_chain_t  out;

    if (buf->in_file == 1) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Data is in file.. will send later... %s", buf->file->name.data);
        return NGX_OK;
    }
    in_place_string_reverse(r, buf);
    buf->last_buf = (r == r->main) ? 1: 0;
    buf->last_in_chain = 1;
    out.buf = buf;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}

/**
 * Recursively start at the end of the buffers list, reverse the buffer and send to client
 */
static ngx_int_t send_reverse_text_recursive(ngx_http_request_t *r, ngx_chain_t  *in, ngx_chain_t  *parent) {
    ngx_int_t    rc;

    if (in == NULL) {
      return NGX_OK;
    }
    rc = send_reverse_text_recursive(r, in->next, in);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Sending data for buffer: %p", in->buf);
    if (rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Error sending data to client: %d", rc);
        return rc;
    }
    return send_current_buffer(r, in->buf);
}

/**
 * Send body to client
 */
static ngx_int_t reverse_text_send_body(ngx_http_request_t *r) {
    ngx_int_t    rc;

    rc = send_reverse_text_recursive(r, r->request_body->bufs, NULL);

    return rc;
}

/**
 * Verify the request, send headers and body
 */
static void ngx_http_reverse_text_send_to_client(ngx_http_request_t *r) {
    ngx_int_t    rc;

    if (!(r->method & NGX_HTTP_POST)) {
        ngx_http_finalize_request(r, NGX_HTTP_NOT_ALLOWED);
        return;
    }
    if (r->request_body == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return;
    }
    rc = reverse_text_send_headers(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        ngx_http_finalize_request(r, rc);
        return;
    }

    rc = reverse_text_send_body(r);
    ngx_http_finalize_request(r, rc);
}

/**
 * Reversing Input Text Content handler.
 */
static ngx_int_t ngx_http_reverse_text_handler(ngx_http_request_t *r) {
    ngx_int_t    rc;

    rc = ngx_http_read_client_request_body(r, ngx_http_reverse_text_send_to_client);
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        /* error */
        return rc;
    }
    return NGX_DONE;

} /* ngx_http_reverse_text_handler */

/**
 * Configuration setup function that installs the content handler.
 *
 * @param cf
 *   Module configuration structure pointer.
 * @param cmd
 *   Module directives structure pointer.
 * @param conf
 *   Module configuration structure pointer.
 * @return string
 *   Status of the configuration setup.
 */
static char *ngx_http_reverse_text(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_core_loc_conf_t *clcf; /* pointer to core location configuration */

    /* Install the hello world handler. */
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_reverse_text_handler;

    return NGX_CONF_OK;
} /* ngx_http_reverse_text */