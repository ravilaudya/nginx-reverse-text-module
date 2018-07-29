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


static void *ngx_http_reverse_text_create_cf(ngx_conf_t *cf);
static char *ngx_http_reverse_text_merge_cf(ngx_conf_t *cf, void *parent, void *child);
static char *ngx_http_reverse_text(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_reverse_text_handler(ngx_http_request_t *r);

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

/**
 * This module provided directive: reverse_text.
 *
 */
static ngx_command_t ngx_http_reverse_text_commands[] = {

    { ngx_string("reverse_text"), /* directive */
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS, /* location context and takes no arguments*/
      ngx_http_reverse_text, /* configuration setup function */
      NGX_HTTP_LOC_CONF_OFFSET, /* No offset. Only one context is supported. */
      0, /* No offset when storing the module configuration on struct. */
      NULL},
    { ngx_string("in_memory_buffer_size"), /* directive */
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_reverse_text_loc_conf_t, in_memory_buffer_size),
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

    ngx_http_reverse_text_create_cf, /* create location configuration */
    ngx_http_reverse_text_merge_cf /* merge location configuration */
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
    ngx_int_t    rc, len, buffers_count;
    ngx_chain_t  *in;

    len = 0;
    buffers_count = 0;

    for (in = r->request_body->bufs; in; in = in->next) {
        buffers_count++;
        len += ngx_buf_size(in->buf);
    }
    r->headers_out.content_type.len = sizeof("text/plain") - 1;
    r->headers_out.content_type.data = (u_char *) "text/plain";
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = len;

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Sent content length header with value: %d, total buffers: %d", len, buffers_count);
    rc = ngx_http_send_header(r);

    return rc;
}

/**
 * Inplace string reverse in buffer
 */
static void in_place_buffer_reverse(ngx_http_request_t *r, ngx_buf_t *buffer) {
    unsigned char *lhs = buffer->start, *rhs = buffer->last;
//    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "About to reverse string: %s", lhs);
    if (!lhs || !rhs || !*lhs || !*(lhs+1)) {
        return;
    }

    while (lhs < --rhs) {
        unsigned char tmp = *lhs;
        *lhs++ = *rhs;
        *rhs = tmp;
    }
//    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "After reverse: %s", buffer->start);
}

static ngx_file_chunk_t find_next_file_chunk(ngx_buf_t *buf, ngx_uint_t remaining_size, ngx_uint_t offset, ngx_uint_t in_memory_buffer_size) {
    ngx_file_chunk_t  file_chunk;

    file_chunk.next_chunk_size = ngx_min(remaining_size, in_memory_buffer_size);
    file_chunk.remaining_size = remaining_size - file_chunk.next_chunk_size;
    file_chunk.offset = ngx_max(offset - file_chunk.next_chunk_size, 0);

    return file_chunk;
}

static void build_buffer_to_send(ngx_http_request_t *r, ngx_buf_t *buffer_to_send, u_char *in_memory_buf, ngx_file_chunk_t file_chunk) {
    buffer_to_send->start = buffer_to_send->pos = in_memory_buf;
    buffer_to_send->end = buffer_to_send->last =  in_memory_buf + file_chunk.next_chunk_size;
    in_place_buffer_reverse(r, buffer_to_send);

    buffer_to_send->last_buf = (r == r->main && file_chunk.remaining_size <= 0)? 1: 0;
    buffer_to_send->last_in_chain = (file_chunk.remaining_size <= 0)? 1: 0;
    buffer_to_send->memory = 1;
}

static ngx_int_t build_buffer_and_send_to_client(ngx_http_request_t *r, ngx_buf_t *buffer_to_send, u_char *in_memory_buf, ngx_file_chunk_t file_chunk) {
    ngx_chain_t   out;

    build_buffer_to_send(r, buffer_to_send, in_memory_buf, file_chunk);
    out.buf = buffer_to_send;
    out.next = NULL;

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Sending file buffer with size: %d", file_chunk.next_chunk_size);

    return ngx_http_output_filter(r, &out);
}

static ngx_int_t send_file_chunked_response(ngx_http_request_t *r, ngx_buf_t *buf) {
    ngx_int_t     rc, buf_size;
    u_char        *in_memory_buf;
    ngx_buf_t     *buffer_to_send;
    ngx_file_chunk_t  file_chunk;
    ngx_uint_t    in_memory_buffer_size;
    ngx_http_reverse_text_loc_conf_t *rtcf;


    buf_size = ngx_buf_size(buf);

    rtcf = ngx_http_get_module_loc_conf(r, ngx_http_reverse_text_module);
    if (rtcf == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Can not get context for module");
        return NGX_ERROR;
    }
    in_memory_buffer_size = rtcf->in_memory_buffer_size;

    file_chunk = find_next_file_chunk(buf, buf_size, buf_size, in_memory_buffer_size);

    rc = NGX_OK;

    in_memory_buf = ngx_palloc(r->connection->pool, file_chunk.next_chunk_size);
    if (in_memory_buf == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Can not allocate memory for buffer to read from file: %s, size: %d", buf->file->name.data, buf_size);
        return NGX_ERROR;
    }
    buffer_to_send = ngx_palloc(r->connection->pool, sizeof(ngx_buf_t *));
    if (buffer_to_send == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Can not allocate memory for buffer to send to client, for data from file: %s, size: %d", buf->file->name.data, file_chunk.next_chunk_size);
        return NGX_ERROR;
    }

    while (file_chunk.next_chunk_size > 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Data is in file: %s, size: %d, offset: %d", buf->file->name.data, file_chunk.next_chunk_size, file_chunk.offset);
        rc = ngx_read_file(buf->file, in_memory_buf, file_chunk.next_chunk_size, file_chunk.offset);
        if (rc < NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Error reading from file: %s, size: %d, error: %d", buf->file->name.data, file_chunk.next_chunk_size, rc);
            return NGX_ERROR;
        }
        rc = build_buffer_and_send_to_client(r, buffer_to_send, in_memory_buf, file_chunk);

        if (rc != NGX_OK && rc != NGX_AGAIN) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Error sending data to output filter from file: %s, size: %d, error: %d", buf->file->name.data, file_chunk.next_chunk_size, rc);
            return rc;
        }

        file_chunk = find_next_file_chunk(buf, file_chunk.remaining_size, file_chunk.offset, in_memory_buffer_size);
    }
    return rc;
}

static ngx_int_t send_file_buffer(ngx_http_request_t *r, ngx_buf_t *buf) {
    return send_file_chunked_response(r, buf);
}

/**
 * Reverse the current buffer and send to client
 */
static ngx_int_t send_current_buffer(ngx_http_request_t *r, ngx_buf_t  *buf, ngx_chain_t *parent) {
    ngx_chain_t  out;

    if (buf->in_file == 1) {
        return send_file_buffer(r, buf);
    }
    in_place_buffer_reverse(r, buf);
    buf->last_buf = (r == r->main && parent == NULL) ? 1: 0;
    buf->last_in_chain = 1;
    out.buf = buf;
    out.next = NULL;
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Sending buffer with size: %d", ngx_buf_size(buf));

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
    return send_current_buffer(r, in->buf, parent);
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
    ngx_http_reverse_text_loc_conf_t *rtcf;

    rtcf = ngx_http_get_module_loc_conf(r, ngx_http_reverse_text_module);

//    if (!rtcf->reverse_text) {
//        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "reverse_text module not being used");
//        return NGX_DECLINED;
//    }
    rc = ngx_http_read_client_request_body(r, ngx_http_reverse_text_send_to_client);
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        /* error */
        return rc;
    }
    return NGX_DONE;

} /* ngx_http_reverse_text_handler */


static void * ngx_http_reverse_text_create_cf(ngx_conf_t *cf) {
    ngx_http_reverse_text_loc_conf_t *rtcf;

    rtcf = ngx_palloc(cf->pool, sizeof(ngx_http_reverse_text_loc_conf_t));
    if (rtcf == NULL) {
        return NGX_CONF_ERROR;
    }
    ngx_log_error(NGX_LOG_ERR, cf->log, 0, "setting the config...");
    rtcf->reverse_text = NGX_CONF_UNSET;
    rtcf->in_memory_buffer_size = NGX_CONF_UNSET_UINT;

    return rtcf;
}

static char * ngx_http_reverse_text_merge_cf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_reverse_text_loc_conf_t *prev = parent;
    ngx_http_reverse_text_loc_conf_t *conf = child;

    ngx_conf_merge_uint_value(conf->in_memory_buffer_size, prev->in_memory_buffer_size, DEFAULT_BUFFER_SIZE);
    if (conf->in_memory_buffer_size < 1) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "in_memory_buffer_size must be >= 1");
        return NGX_CONF_ERROR;
    }
    ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "merging the config...[%d, %d]", conf->reverse_text, conf->in_memory_buffer_size);

    return NGX_CONF_OK;
}

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

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_reverse_text_handler;

    return NGX_CONF_OK;
} /* ngx_http_reverse_text */