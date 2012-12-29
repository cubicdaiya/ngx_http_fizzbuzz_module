/*
 * Copyright (C) 2012 Tatsuhiko Kubo <cubicdaiya@gmail.com>
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define HTML_TEMPLATE(qualifier)                \
    "<!DOCTYPE html>\n"                         \
    "<html>\n"                                  \
    "<head>\n"                                  \
    "<title>FizzBuzz with nginx!</title>\n"     \
    "</head>\n"                                 \
    "<body>\n"                                  \
    "<p>FizzBuzz(%d) = "                        \
    qualifier                                   \
    "</p>\n"                                    \
    "</body>\n"                                 \
    "</html>\n"
    

typedef enum ngx_http_fizzbuzz_result_t {
    NGX_HTTP_FIZZBUZZ_RESULT_FIZZ,
    NGX_HTTP_FIZZBUZZ_RESULT_BUZZ,
    NGX_HTTP_FIZZBUZZ_RESULT_FIZZBUZZ,
    NGX_HTTP_FIZZBUZZ_RESULT_MAX
} ngx_http_fizzbuzz_result_t;

static const char *ngx_http_fizzbuzz_results[] = {
    [NGX_HTTP_FIZZBUZZ_RESULT_FIZZ]      = "Fizz",
    [NGX_HTTP_FIZZBUZZ_RESULT_BUZZ]      = "Buzz",
    [NGX_HTTP_FIZZBUZZ_RESULT_FIZZBUZZ]  = "FizBuzz",
};

typedef struct {
    ngx_uint_t number;
    ngx_http_complex_value_t *ncv;
} ngx_http_fizzbuzz_conf_t;

static ngx_int_t  ngx_http_fizzbuzz_handler(ngx_http_request_t *r);
static void      *ngx_http_fizzbuzz_create_loc_conf(ngx_conf_t *cf);
static char      *ngx_http_fizzbuzz(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_uint_t ngx_http_fizzbuzz_atoi(ngx_str_t *value);
static ngx_uint_t ngx_http_fizzbuzz_get_number(ngx_http_request_t *r, ngx_http_complex_value_t *cv, ngx_uint_t v);
static ngx_http_fizzbuzz_result_t ngx_http_fizzbuzz_result(ngx_uint_t n);

static ngx_command_t ngx_http_fizzbuzz_commands[] = {
    { 
        ngx_string("fizzbuzz"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_fizzbuzz,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_fizzbuzz_module_ctx = {
    NULL,                              /* preconfiguration */
    NULL,                              /* postconfiguration */

    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

    ngx_http_fizzbuzz_create_loc_conf, /* create location configuration */
    NULL                               /* merge location configuration */
};

ngx_module_t ngx_http_fizzbuzz_module = {
    NGX_MODULE_V1,
    &ngx_http_fizzbuzz_module_ctx, /* module context */
    ngx_http_fizzbuzz_commands,    /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_fizzbuzz_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_fizzbuzz_conf_t  *loc_conf;
    ngx_uint_t                 number;
    ngx_http_fizzbuzz_result_t result;
    ngx_chain_t                out;
    ngx_buf_t                 *b;
    u_char                    *buf;
    ngx_uint_t                 clen;

    if (r->method != NGX_HTTP_GET) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (r->headers_in.if_modified_since) {
        return NGX_HTTP_NOT_MODIFIED;
    }

    loc_conf = ngx_http_get_module_loc_conf(r, ngx_http_fizzbuzz_module);

    number = ngx_http_fizzbuzz_get_number(r, loc_conf->ncv, loc_conf->number);
    if (number == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "fizzbuzz: n > 0");
        return NGX_HTTP_BAD_REQUEST;
    }
    result = ngx_http_fizzbuzz_result(number);

    buf = ngx_pcalloc(r->pool, BUFSIZ);
    if (buf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    if (result == NGX_HTTP_FIZZBUZZ_RESULT_MAX) {
        ngx_sprintf(buf, HTML_TEMPLATE("%d"), number, number);
    } else {
        ngx_sprintf(buf, HTML_TEMPLATE("%s"), number, ngx_http_fizzbuzz_results[result]);
    }

    clen = ngx_strlen(buf); 

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    b->pos      = buf;
    b->last     = buf + clen;
    b->memory   = 1;
    b->last_buf = 1;
    out.buf     = b;
    out.next    = NULL;

    r->headers_out.content_type.len  = ngx_strlen("text/html");
    r->headers_out.content_type.data = (u_char *) "text/html";
    r->headers_out.status            = NGX_HTTP_OK;
    r->headers_out.content_length_n  = clen;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}
 

static void *ngx_http_fizzbuzz_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_fizzbuzz_conf_t *loc_conf;
    loc_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_fizzbuzz_conf_t));
    if (loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }
    loc_conf->number = NGX_CONF_UNSET_UINT;
    return loc_conf;
}

static char *ngx_http_fizzbuzz(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_fizzbuzz_conf_t         *fbcf;
    ngx_http_core_loc_conf_t         *clcf;
    ngx_str_t                        *values;
    ngx_uint_t                        number;
    ngx_http_complex_value_t          cv;
    ngx_http_compile_complex_value_t  ccv;

    values = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf            = cf;
    ccv.value         = &values[1];
    ccv.complex_value = &cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    fbcf = conf;
    if (cv.lengths == NULL) {
        number       = ngx_http_fizzbuzz_atoi(&values[1]);
        fbcf->number = number;
    } else {
        fbcf->ncv = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
        if (fbcf->ncv == NULL) {
            return NGX_CONF_ERROR;
        }

        *fbcf->ncv = cv;
    }

    clcf          = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_fizzbuzz_handler;
    
    return NGX_CONF_OK;
}

static ngx_uint_t ngx_http_fizzbuzz_atoi(ngx_str_t *value)
{
    ngx_int_t n;
    n = ngx_atoi(value->data, value->len);
    if (n == NGX_ERROR) {
        return 0;
    }
    return (ngx_uint_t) n;
}

static ngx_uint_t ngx_http_fizzbuzz_get_number(ngx_http_request_t *r, ngx_http_complex_value_t *cv, ngx_uint_t v)
{
    ngx_str_t val;

    if (cv == NULL) {
        return v;
    }

    if (ngx_http_complex_value(r, cv, &val) != NGX_OK) {
        return 0;
    }

    return ngx_http_fizzbuzz_atoi(&val);
}

static ngx_http_fizzbuzz_result_t ngx_http_fizzbuzz_result(ngx_uint_t n)
{
    if ((n % 15) == 0) {
        return NGX_HTTP_FIZZBUZZ_RESULT_FIZZBUZZ;
    } else if ((n % 5) == 0) {
        return NGX_HTTP_FIZZBUZZ_RESULT_BUZZ;
    } else if ((n % 3) == 0) {
        return NGX_HTTP_FIZZBUZZ_RESULT_FIZZ;
    }
    return NGX_HTTP_FIZZBUZZ_RESULT_MAX;
}
