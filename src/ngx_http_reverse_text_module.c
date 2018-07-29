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
#include "ngx_http_reverse_text_module.h"

static void *ngx_http_reverse_text_create_cf(ngx_conf_t *cf);
static char *ngx_http_reverse_text_merge_cf(ngx_conf_t *cf, void *parent, void *child);
static char *ngx_http_reverse_text(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_reverse_text_handler(ngx_http_request_t *r);

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
 * Verify the request, send headers and body
 */
static void ngx_http_reverse_text_send_to_client(ngx_http_request_t *r) {
    ngx_int_t    rc;
    ngx_http_reverse_text_loc_conf_t *rtcf;

    rc = ngx_http_reverse_text_validate_request(r);
    if (rc != NGX_OK) {
        ngx_http_finalize_request(r, rc);
        return;
    }
    rc = reverse_text_send_headers(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        ngx_http_finalize_request(r, rc);
        return;
    }

    rtcf = ngx_http_get_module_loc_conf(r, ngx_http_reverse_text_module);
    if (rtcf == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Can not get context for module: reverse_text");
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }
    rc = reverse_text_send_body(r, rtcf);
    ngx_http_finalize_request(r, rc);
}

/**
 * Reversing Input Text Content handler.
 */
static ngx_int_t ngx_http_reverse_text_handler(ngx_http_request_t *r) {
    ngx_int_t    rc;
    ngx_http_reverse_text_loc_conf_t *rtcf;

    rtcf = ngx_http_get_module_loc_conf(r, ngx_http_reverse_text_module);
    if (rtcf == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Can not get context for module: reverse_text");
        return NGX_ERROR;
    }

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

    rtcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_reverse_text_loc_conf_t));
    if (rtcf == NULL) {
        return NGX_CONF_ERROR;
    }
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