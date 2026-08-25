/*
 * Copyright (c) 2026 Lemon4ksan All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include "curl_setup.h"
#include "urldata.h"
#include "sendf.h"
#include "progress.h"
#include "strcase.h"
#include "aoni_bridge.h"

#include <aoni.h>

bool Curl_aoni_is_supported_url(const char *url)
{
  if(!url)
    return FALSE;

  if(curl_strnequal(url, "http://", 7) ||
     curl_strnequal(url, "https://", 8) ||
     curl_strnequal(url, "ws://", 5) ||
     curl_strnequal(url, "wss://", 6)) {
    return TRUE;
  }

  return FALSE;
}

static CURLcode ao_deliver_headers(struct Curl_easy *data, const uint8_t *buf, size_t len)
{
  if(!len || !buf)
    return CURLE_OK;

  if(data->set.fwrite_header) {
    size_t written = data->set.fwrite_header((char *)(uintptr_t)buf, 1, len, data->set.writeheader);
    if(written != len)
      return CURLE_WRITE_ERROR;
  }
  else if(data->set.writeheader) {
    if(data->set.fwrite_func) {
      size_t written = data->set.fwrite_func((char *)(uintptr_t)buf, 1, len, data->set.writeheader);
      if(written != len)
        return CURLE_WRITE_ERROR;
    }
    else {
      size_t written = fwrite(buf, 1, len, (FILE *)data->set.writeheader);
      if(written != len)
        return CURLE_WRITE_ERROR;
    }
  }

  return CURLE_OK;
}

static CURLcode ao_deliver_body(struct Curl_easy *data, const uint8_t *buf, size_t len)
{
  if(!len || !buf)
    return CURLE_OK;

  if(data->set.fwrite_func) {
    size_t written = data->set.fwrite_func((char *)(uintptr_t)buf, 1, len, data->set.out);
    if(written != len)
      return CURLE_WRITE_ERROR;
  }
  else {
    FILE *out = data->set.out ? (FILE *)data->set.out : stdout;
    size_t written = fwrite(buf, 1, len, out);
    if(written != len)
      return CURLE_WRITE_ERROR;
    fflush(out);
  }

  return CURLE_OK;
}

static aoni_client_t g_shared_http_client = NULL;
static aoni_client_t g_shared_tls_client = NULL;
static pthread_mutex_t g_client_pool_lock = PTHREAD_MUTEX_INITIALIZER;

static aoni_client_t ao_get_client(const aoni_config_t *cfg, bool is_tls, bool *is_reused)
{
  *is_reused = false;
  if(!cfg->proxy_url) {
    pthread_mutex_lock(&g_client_pool_lock);
    if(is_tls) {
      if(!g_shared_tls_client) {
        g_shared_tls_client = aoni_client_create(cfg);
      }
      pthread_mutex_unlock(&g_client_pool_lock);
      if(g_shared_tls_client) {
        *is_reused = true;
        return g_shared_tls_client;
      }
    }
    else {
      if(!g_shared_http_client) {
        g_shared_http_client = aoni_client_create(cfg);
      }
      pthread_mutex_unlock(&g_client_pool_lock);
      if(g_shared_http_client) {
        *is_reused = true;
        return g_shared_http_client;
      }
    }
  }
  return aoni_client_create(cfg);
}

CURLcode Curl_aoni_perform(struct Curl_easy *data)
{
  const char *url;
  const char *method = "GET";
  const char *custom_req;
  const char *proxy_url;
  struct curl_slist *h;
  char *headers_buf = NULL;
  size_t headers_cap = 4096;
  size_t headers_len = 0;
  uint8_t *resp_hdr_buf = NULL;
  size_t resp_hdr_cap = 8192;
  aoni_config_t cfg;
  aoni_client_t client;
  aoni_task_t task;
  int32_t status;
  bool is_tls = false;
  bool is_reused = false;
  CURLcode result = CURLE_OK;

  if(!data)
    return CURLE_BAD_FUNCTION_ARGUMENT;

  url = CURL_EASY_STR(data, STRING_SET_URL);
  if(!url || !*url)
    return CURLE_URL_MALFORMAT;

  /* 1. Initialize aoni client configuration with uTLS Chrome evasion */
  memset(&cfg, 0, sizeof(cfg));
  cfg.max_conns_per_host = (uint32_t)data->set.maxconnects;
  if(!cfg.max_conns_per_host)
    cfg.max_conns_per_host = 1024;
  cfg.concurrency = 4096;

  if(data->set.timeout > 0)
    cfg.timeout_ms = (uint32_t)data->set.timeout;
  else
    cfg.timeout_ms = 30000;

  is_tls = (curl_strnequal(url, "https://", 8) || curl_strnequal(url, "wss://", 6));
  if(is_tls)
    cfg.browser_profile = AONI_BROWSER_CHROME; /* Default to Chrome uTLS Profile for TLS */
  else
    cfg.browser_profile = AONI_BROWSER_NONE;   /* Plaintext for HTTP */

  proxy_url = CURL_EASY_STR(data, STRING_PROXY);
  if(proxy_url && *proxy_url) {
    cfg.proxy_url = (char *)(uintptr_t)proxy_url;
  }

  client = ao_get_client(&cfg, is_tls, &is_reused);
  if(!client)
    return CURLE_OUT_OF_MEMORY;

  /* 2. Determine HTTP Method */
  custom_req = CURL_EASY_STR(data, STRING_CUSTOMREQUEST);
  if(custom_req && *custom_req)
    method = custom_req;
  else if(data->set.postfields)
    method = "POST";
  else if(data->set.opt_no_body)
    method = "HEAD";

  /* 3. Serialize headers from curl_slist into raw CRLF stream */
  headers_buf = (char *)malloc(headers_cap);
  if(!headers_buf) {
    aoni_client_destroy(client);
    return CURLE_OUT_OF_MEMORY;
  }
  headers_buf[0] = '\0';

  for(h = data->set.headers; h; h = h->next) {
    if(h->data && *h->data) {
      size_t line_len = strlen(h->data);
      if(headers_len + line_len + 4 >= headers_cap) {
        size_t new_cap = (headers_cap + line_len + 4) * 2;
        char *new_buf = (char *)realloc(headers_buf, new_cap);
        if(!new_buf)
          break;
        headers_buf = new_buf;
        headers_cap = new_cap;
      }
      memcpy(headers_buf + headers_len, h->data, line_len);
      headers_len += line_len;
      memcpy(headers_buf + headers_len, "\r\n", 2);
      headers_len += 2;
      headers_buf[headers_len] = '\0';
    }
  }

  /* 4. Allocate response headers buffer */
  resp_hdr_buf = (uint8_t *)malloc(resp_hdr_cap);
  if(!resp_hdr_buf) {
    free(headers_buf);
    aoni_client_destroy(client);
    return CURLE_OUT_OF_MEMORY;
  }

  /* 5. Populate aoni task descriptor */
  memset(&task, 0, sizeof(task));
  task.task_id = 1;
  task.method = (char *)(uintptr_t)method;
  task.method_len = strlen(method);
  task.url = (char *)(uintptr_t)url;
  task.url_len = strlen(url);
  task.headers_raw = (uint8_t *)headers_buf;
  task.headers_len = headers_len;

  /* Request body */
  if(data->set.postfields) {
    task.body_ptr = (uint8_t *)data->set.postfields;
    if(data->set.postfieldsize >= 0)
      task.body_len = (size_t)data->set.postfieldsize;
    else
      task.body_len = strlen((const char *)data->set.postfields);
  }

  /* Mode 2: Dynamic Off-Heap auto-allocation (resp_buf_ptr = NULL) */
  task.resp_buf_ptr = NULL;
  task.resp_buf_cap = 0;
  task.resp_headers_ptr = resp_hdr_buf;
  task.resp_headers_cap = resp_hdr_cap;

  /* 6. Execute request over aoni reactor */
  status = aoni_client_do(client, &task);

  if(status > 0) {
    data->info.httpcode = task.status_code;

    /* Deliver response headers to curl callbacks / file */
    if(task.resp_headers_len > 0 && task.resp_headers_ptr) {
      result = ao_deliver_headers(data, task.resp_headers_ptr, task.resp_headers_len);
    }

    /* Deliver response body to curl callbacks / file */
    if(!result && task.resp_buf_len > 0 && task.resp_buf_ptr) {
      result = ao_deliver_body(data, task.resp_buf_ptr, task.resp_buf_len);
    }
  }
  else {
    if(task.error_code == AONI_ERR_TIMEOUT)
      result = CURLE_OPERATION_TIMEDOUT;
    else if(task.error_code == AONI_ERR_BUFFER_OVERFLOW)
      result = CURLE_OUT_OF_MEMORY;
    else
      result = CURLE_COULDNT_CONNECT;
  }

  /* 7. Cleanup offheap memory & resources */
  aoni_task_free(&task);
  free(headers_buf);
  free(resp_hdr_buf);
  if(!is_reused)
    aoni_client_destroy(client);

  return result;
}
