/**
 * @file   ngx_http_reverse_text_send_body.c
 * @author Ravi
 * @date   29-July-2018
 *
 * @brief  Body Sender
 *
 * Iterates through the list of buffers (chains), sends the response to the client
 * in reverse. i.e. If the input buffers are a list: [ab]->[cd]->[ef]->[gh],
 * recursively go to the end of the list and send the reversed text of each buffer.
 * In this example, process [gh] first (i.e. send "hg" to client), [ef] next,
 * [cd] next and finally [ab].
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_reverse_text_module.h"

/**
 * Inplace string reverse in buffer
 */
static void in_place_buffer_reverse(ngx_http_request_t *r, ngx_buf_t *buffer) {
    unsigned char *lhs = buffer->start, *rhs = buffer->last;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "About to reverse string: %s", lhs);
    if (!lhs || !rhs || !*lhs || !*(lhs+1)) {
        return;
    }

    while (lhs < --rhs) {
        unsigned char tmp = *lhs;
        *lhs++ = *rhs;
        *rhs = tmp;
    }
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "After reverse string: %s", buffer->start);
}

/**
 * Find the next chunk to be retrieved from the file
 */
static ngx_file_chunk_t find_next_file_chunk(ngx_buf_t *buf, ngx_uint_t remaining_size, ngx_uint_t offset, ngx_uint_t in_memory_buffer_size) {
    ngx_file_chunk_t  file_chunk;

    file_chunk.next_chunk_size = ngx_min(remaining_size, in_memory_buffer_size);
    file_chunk.remaining_size = remaining_size - file_chunk.next_chunk_size;
    file_chunk.offset = ngx_max(offset - file_chunk.next_chunk_size, 0);

    return file_chunk;
}

/**
 * Build the buffer to be sent to client
 */
static void build_buffer_to_send(ngx_http_request_t *r, ngx_buf_t *buffer_to_send, u_char *in_memory_buf, ngx_file_chunk_t file_chunk) {
    buffer_to_send->start = buffer_to_send->pos = in_memory_buf;
    buffer_to_send->end = buffer_to_send->last =  in_memory_buf + file_chunk.next_chunk_size;
    in_place_buffer_reverse(r, buffer_to_send);

    buffer_to_send->in_file = 0;
    buffer_to_send->file = NULL;
    buffer_to_send->recycled = 1;
    buffer_to_send->last_buf = (r == r->main && file_chunk.remaining_size <= 0)? 1: 0;
    buffer_to_send->last_in_chain = (file_chunk.remaining_size <= 0)? 1: 0;
    buffer_to_send->memory = 1;
}

/**
 * Build the buffer and send it to output filter
 */
static ngx_int_t build_buffer_and_send_to_client(ngx_http_request_t *r, ngx_buf_t *buffer_to_send, u_char *in_memory_buf, ngx_file_chunk_t file_chunk) {
    ngx_chain_t   out;

    build_buffer_to_send(r, buffer_to_send, in_memory_buf, file_chunk);
    out.buf = buffer_to_send;
    out.next = NULL;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "Sending file buffer with size: %d", file_chunk.next_chunk_size);

    return ngx_http_output_filter(r, &out);
}

/**
 * Split the file into chunks [starting from end], reverse each chunk and send to client each chunk
 */
static ngx_int_t send_file_chunked_response(ngx_http_request_t *r, ngx_buf_t *buf, ngx_http_reverse_text_loc_conf_t *rtcf) {
    ngx_int_t     rc, buf_size;
    u_char        *in_memory_buf;
    ngx_buf_t     *buffer_to_send;
    ngx_file_chunk_t  file_chunk;
    ngx_uint_t    in_memory_buffer_size;

    buf_size = ngx_buf_size(buf);
    in_memory_buffer_size = rtcf->in_memory_buffer_size;
    file_chunk = find_next_file_chunk(buf, buf_size, buf_size, in_memory_buffer_size);

    rc = NGX_OK;

    in_memory_buf = ngx_pcalloc(r->connection->pool, file_chunk.next_chunk_size);
    if (in_memory_buf == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Can not allocate memory for buffer to read from file: %s, size: %d", buf->file->name.data, buf_size);
        return NGX_ERROR;
    }
    buffer_to_send = ngx_pcalloc(r->connection->pool, sizeof(ngx_buf_t *));
    if (buffer_to_send == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Can not allocate memory for buffer to send to client, for data from file: %s, size: %d", buf->file->name.data, file_chunk.next_chunk_size);
        return NGX_ERROR;
    }

    while (file_chunk.next_chunk_size > 0) {
        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "Data is in file: %s, size: %d, offset: %d", buf->file->name.data, file_chunk.next_chunk_size, file_chunk.offset);
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

static ngx_int_t send_file_buffer(ngx_http_request_t *r, ngx_buf_t *buf, ngx_http_reverse_text_loc_conf_t *rtcf) {
    return send_file_chunked_response(r, buf, rtcf);
}

/**
 * Reverse the current buffer and send to client [in memory or in file]
 */
static ngx_int_t send_current_buffer(ngx_http_request_t *r, ngx_buf_t  *buf, ngx_chain_t *parent, ngx_http_reverse_text_loc_conf_t *rtcf) {
    ngx_chain_t  out;

    if (buf->in_file == 1) {
        return send_file_buffer(r, buf, rtcf);
    }
    in_place_buffer_reverse(r, buf);
    buf->last_buf = (r == r->main && parent == NULL) ? 1: 0;
    buf->last_in_chain = 1;
    out.buf = buf;
    out.next = NULL;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "Sending buffer with size: %d", ngx_buf_size(buf));

    return ngx_http_output_filter(r, &out);
}

/**
 * Recursively start at the end of the buffers list, reverse the buffer and send to client
 */
static ngx_int_t send_reverse_text_recursive(ngx_http_request_t *r, ngx_chain_t  *in, ngx_chain_t  *parent, ngx_http_reverse_text_loc_conf_t *rtcf) {
    ngx_int_t    rc;

    if (in == NULL) {
      return NGX_OK;
    }
    rc = send_reverse_text_recursive(r, in->next, in, rtcf);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "Sending data for buffer: %p", in->buf);
    if (rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Error sending data to client: %d", rc);
        return rc;
    }
    return send_current_buffer(r, in->buf, parent, rtcf);
}

/**
 * Send body to client
 */
ngx_int_t reverse_text_send_body(ngx_http_request_t *r, ngx_http_reverse_text_loc_conf_t *rtcf) {
    ngx_int_t    rc;

    rc = send_reverse_text_recursive(r, r->request_body->bufs, NULL, rtcf);

    return rc;
}

