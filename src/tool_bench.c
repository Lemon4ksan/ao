/*
 * Copyright (c) 2026 Lemon4ksan All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include "tool_setup.h"
#include "tool_cfgable.h"
#include "tool_msgs.h"
#include "tool_bench.h"
#include "tool_main.h"

#include <aoni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <xmmintrin.h>
#endif

#define AO_BUCKET_RESOLUTION_US 50 /* 50 microseconds (0.05ms) per bucket */
#define AO_HISTOGRAM_BUCKETS 100000 /* 0 .. 5000.0 ms (5s) with 50us resolution */
#define AO_BATCH_SIZE 512
#define AO_TASK_RESP_SCRATCH_SIZE 512
#define AO_TASK_HDRS_SCRATCH_SIZE 256

typedef struct {
  uint64_t requests_completed;
  uint64_t bytes_transferred;
  uint64_t status_2xx;
  uint64_t status_3xx;
  uint64_t status_4xx;
  uint64_t status_5xx;
  uint64_t errors_timeout;
  uint64_t errors_network;

  uint64_t sum_dns_ns;
  uint64_t sum_tls_ns;
  uint64_t sum_ttfb_ns;
  uint64_t sum_total_ns;

  uint64_t min_total_ns;
  uint64_t max_total_ns;

  uint32_t histogram[AO_HISTOGRAM_BUCKETS];
} ao_bench_stats_t;

#if defined(__GNUC__) || defined(__clang__)
#define AO_CACHE_ALIGN __attribute__((aligned(64)))
#define AO_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define AO_CACHE_ALIGN __declspec(align(64))
#define AO_INLINE __forceinline
#else
#define AO_CACHE_ALIGN
#define AO_INLINE inline
#endif

typedef struct AO_CACHE_ALIGN {
  int worker_id;
  int num_threads;
  uint32_t concurrency;
  uint32_t rate_limit_rps;
  uint64_t max_requests;
  timediff_t duration_ms;

  const char *url;
  const char *method;
  const uint8_t *headers_raw;
  size_t headers_len;
  const uint8_t *body_ptr;
  size_t body_len;

  uint8_t browser_profile;
  uint8_t enable_http2;
  uint8_t enable_http3;
  uint8_t enable_pipeline;
  const char *proxy_url;

  volatile int *stop_flag;
  uint64_t *global_req_counter;

  ao_bench_stats_t stats;

#ifdef _WIN32
  HANDLE thread_handle;
#else
  pthread_t thread_id;
#endif
} ao_bench_worker_t;

#ifdef _WIN32
static uint64_t get_time_ns(void)
{
  static LARGE_INTEGER freq;
  static int init = 0;
  LARGE_INTEGER counter;
  uint64_t result;
  if(!init) {
    QueryPerformanceFrequency(&freq);
    init = 1;
  }
  QueryPerformanceCounter(&counter);
  result = (uint64_t)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
  return result;
}
static void ao_sleep_ms(uint32_t ms)
{
  Sleep(ms);
}
static int get_cpu_cores(void)
{
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return (si.dwNumberOfProcessors > 0) ? (int)si.dwNumberOfProcessors : 4;
}
#else
static uint64_t get_time_ns(void)
{
  struct timespec ts;
  uint64_t result;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  result = ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
  return result;
}
static void ao_sleep_ms(uint32_t ms)
{
  usleep(ms * 1000);
}
static int get_cpu_cores(void)
{
  long cores = sysconf(_SC_NPROCESSORS_ONLN);
  return (cores > 0) ? (int)cores : 4;
}
#endif

static AO_INLINE void record_latency(ao_bench_stats_t *stats, uint64_t total_ns)
{
  uint64_t us = total_ns / 1000;
  size_t bucket = (size_t)(us / AO_BUCKET_RESOLUTION_US); /* 10us per bucket */
  if(bucket >= AO_HISTOGRAM_BUCKETS)
    bucket = AO_HISTOGRAM_BUCKETS - 1;
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
  _mm_prefetch((const char *)&stats->histogram[bucket], _MM_HINT_T0);
#endif
  stats->histogram[bucket]++;

  if(total_ns < stats->min_total_ns || stats->min_total_ns == 0)
    stats->min_total_ns = total_ns;
  if(total_ns > stats->max_total_ns)
    stats->max_total_ns = total_ns;
}

#ifdef _WIN32
static unsigned int __stdcall bench_worker_proc(void *arg)
#else
static void *bench_worker_proc(void *arg)
#endif
{
  ao_bench_worker_t *w = (ao_bench_worker_t *)arg;
  aoni_config_t cfg;
  aoni_client_t client;
  aoni_task_t tasks[AO_BATCH_SIZE];
  uint8_t scratch_resps[AO_BATCH_SIZE][AO_TASK_RESP_SCRATCH_SIZE];
  uint8_t scratch_hdrs[AO_BATCH_SIZE][AO_TASK_HDRS_SCRATCH_SIZE];
  size_t batch_size;
  size_t i;
  uint64_t rate_interval_ns = 0;
  uint64_t next_tick_ns = 0;

#ifdef _WIN32
  if(w->worker_id < 64) {
    DWORD_PTR mask = ((DWORD_PTR)1) << (w->worker_id % get_cpu_cores());
    SetThreadAffinityMask(GetCurrentThread(), mask);
  }
#elif defined(__linux__)
  {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(w->worker_id % get_cpu_cores(), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  }
#endif

  batch_size = w->concurrency ? (size_t)w->concurrency : 16;
  if(batch_size > AO_BATCH_SIZE)
    batch_size = AO_BATCH_SIZE;
  if(batch_size == 0)
    batch_size = 1;

  memset(&cfg, 0, sizeof(cfg));
  cfg.max_conns_per_host = w->concurrency ? w->concurrency : 512;
  cfg.concurrency = w->concurrency ? w->concurrency : 512;
  cfg.timeout_ms = 5000;
  cfg.browser_profile = w->browser_profile;
  cfg.enable_http2 = w->enable_http2;
  cfg.enable_http3 = w->enable_http3;
  cfg.proxy_url = w->proxy_url;

  client = aoni_client_create(&cfg);
  if(!client) {
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
  }

  if(w->rate_limit_rps > 0) {
    rate_interval_ns = (1000000000ULL * (uint64_t)batch_size) / (uint64_t)w->rate_limit_rps;
    next_tick_ns = get_time_ns();
  }

  /* Pre-initialize task descriptors */
  for(i = 0; i < batch_size; i++) {
    memset(&tasks[i], 0, sizeof(aoni_task_t));
    tasks[i].task_id = (uint64_t)(w->worker_id * 1000000 + i);
    tasks[i].method = w->method;
    tasks[i].method_len = strlen(w->method);
    tasks[i].url = w->url;
    tasks[i].url_len = strlen(w->url);
    tasks[i].headers_raw = w->headers_raw;
    tasks[i].headers_len = w->headers_len;
    tasks[i].body_ptr = w->body_ptr;
    tasks[i].body_len = w->body_len;

    tasks[i].resp_buf_ptr = scratch_resps[i];
    tasks[i].resp_buf_cap = sizeof(scratch_resps[i]);
    tasks[i].resp_headers_ptr = scratch_hdrs[i];
    tasks[i].resp_headers_cap = sizeof(scratch_hdrs[i]);
  }

  while(!*w->stop_flag) {
    /* Rate limiting check */
    if(rate_interval_ns > 0) {
      uint64_t now_ns = get_time_ns();
      if(now_ns < next_tick_ns) {
        uint64_t diff_ns = next_tick_ns - now_ns;
        if(diff_ns > 1000000)
          ao_sleep_ms((uint32_t)(diff_ns / 1000000));
      }
      next_tick_ns = get_time_ns() + rate_interval_ns;
    }

    /* Execute batch */
    if(w->enable_pipeline)
      aoni_client_pipeline_do(client, tasks, batch_size);
    else
      aoni_client_batch_do(client, tasks, batch_size);

    for(i = 0; i < batch_size; i++) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
      if(i + 2 < batch_size) {
        _mm_prefetch((const char *)&tasks[i + 2], _MM_HINT_T0);
        _mm_prefetch((const char *)scratch_resps[i + 2], _MM_HINT_T0);
        _mm_prefetch((const char *)scratch_hdrs[i + 2], _MM_HINT_T0);
      }
#endif
      if(tasks[i].error_code == AONI_OK) {
        int code = tasks[i].status_code;
        if(code >= 200 && code < 300)
          w->stats.status_2xx++;
        else if(code >= 300 && code < 400)
          w->stats.status_3xx++;
        else if(code >= 400 && code < 500)
          w->stats.status_4xx++;
        else if(code >= 500)
          w->stats.status_5xx++;
        else
          w->stats.status_2xx++;

        w->stats.bytes_transferred += (tasks[i].resp_buf_len + tasks[i].resp_headers_len);
        w->stats.sum_dns_ns += tasks[i].dns_time_ns;
        w->stats.sum_tls_ns += tasks[i].tls_time_ns;
        w->stats.sum_ttfb_ns += tasks[i].ttfb_ns;
        w->stats.sum_total_ns += tasks[i].total_time_ns;

        record_latency(&w->stats, tasks[i].total_time_ns);
      }
      else {
        if(tasks[i].error_code == AONI_ERR_TIMEOUT)
          w->stats.errors_timeout++;
        else
          w->stats.errors_network++;
      }

      w->stats.requests_completed++;

      /* Reset buffers for next round */
      tasks[i].resp_buf_ptr = scratch_resps[i];
      tasks[i].resp_buf_cap = sizeof(scratch_resps[i]);
      tasks[i].resp_headers_ptr = scratch_hdrs[i];
      tasks[i].resp_headers_cap = sizeof(scratch_hdrs[i]);
    }

    if(w->max_requests > 0 && w->global_req_counter) {
#ifdef _WIN32
      LONG64 cur = InterlockedAdd64((LONG64 *)w->global_req_counter, (LONG64)batch_size);
      if((uint64_t)cur >= w->max_requests) {
        *w->stop_flag = 1;
        break;
      }
#else
      uint64_t cur = __atomic_add_fetch(w->global_req_counter, (uint64_t)batch_size, __ATOMIC_RELAXED);
      if(cur >= w->max_requests) {
        *w->stop_flag = 1;
        break;
      }
#endif
    }
  }

  aoni_client_destroy(client);

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

static double get_percentile(const uint32_t *hist, uint64_t total_count, double pct)
{
  double target_d;
  uint64_t target;
  uint64_t running = 0;
  size_t i;

  if(total_count == 0)
    return 0.0;

  target_d = ceil((pct / 100.0) * (double)total_count);
  target = (uint64_t)target_d;

  for(i = 0; i < AO_HISTOGRAM_BUCKETS; i++) {
    running += hist[i];
    if(running >= target) {
      /* bucket * 0.01 ms */
      return (double)i * ((double)AO_BUCKET_RESOLUTION_US / 1000.0);
    }
  }
  return (double)(AO_HISTOGRAM_BUCKETS - 1) * ((double)AO_BUCKET_RESOLUTION_US / 1000.0);
}

CURLcode bench_transfers(CURLSH *share)
{
  struct OperationConfig *config = global->first;
  const char *url;
  const char *method = "GET";
  char *headers_buf = NULL;
  size_t headers_cap = 8192;
  size_t headers_len = 0;
  struct curl_slist *h;
  int num_threads;
  uint32_t total_concurrency;
  uint32_t thread_concurrency;
  uint32_t total_rate;
  uint32_t thread_rate;
  timediff_t duration_ms;
  uint64_t max_requests;
  uint8_t browser_profile;
  const char *proxy_url = NULL;
  ao_bench_worker_t *workers = NULL;
  volatile int stop_flag = 0;
  uint64_t global_req_counter = 0;
  uint64_t start_time_ns, end_time_ns;
  int t;
  ao_bench_stats_t agg;
  uint64_t prev_reqs = 0;
  uint64_t prev_bytes = 0;
  uint64_t prev_time_ns;

  (void)share;

  if(!config || !config->url_list || !config->url_list->url) {
    helpf("No URL specified for benchmark");
    return CURLE_URL_MALFORMAT;
  }

  url = config->url_list->url;

  /* Determine method */
  if(config->customrequest && *config->customrequest)
    method = config->customrequest;
  else if(config->postfields)
    method = "POST";
  else if(config->no_body)
    method = "HEAD";

  /* Determine browser profile */
  if(config->aoni_browser && *config->aoni_browser) {
    if(curl_strequal(config->aoni_browser, "chrome"))
      browser_profile = AONI_BROWSER_CHROME;
    else if(curl_strequal(config->aoni_browser, "firefox"))
      browser_profile = AONI_BROWSER_FIREFOX;
    else if(curl_strequal(config->aoni_browser, "safari"))
      browser_profile = AONI_BROWSER_SAFARI;
    else if(curl_strequal(config->aoni_browser, "none"))
      browser_profile = AONI_BROWSER_NONE;
    else
      browser_profile = AONI_BROWSER_CHROME;
  }
  else if(curl_strnequal(url, "https://", 8) || curl_strnequal(url, "wss://", 6))
    browser_profile = AONI_BROWSER_CHROME;
  else
    browser_profile = AONI_BROWSER_NONE;

  if(config->proxy && *config->proxy)
    proxy_url = config->proxy;

  /* Serialize headers */
  headers_buf = (char *)malloc(headers_cap);
  if(!headers_buf)
    return CURLE_OUT_OF_MEMORY;
  headers_buf[0] = '\0';

  for(h = config->headers; h; h = h->next) {
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

  /* Configuration parameters */
  num_threads = (int)global->bench_threads;
  if(num_threads <= 0)
    num_threads = get_cpu_cores();
  if(num_threads > 128)
    num_threads = 128;

  total_concurrency = global->bench_concurrency ? global->bench_concurrency : 500;
  thread_concurrency = total_concurrency / (uint32_t)num_threads;
  if(thread_concurrency < 1)
    thread_concurrency = 1;

  total_rate = global->bench_rate;
  thread_rate = total_rate > 0 ? (total_rate / (uint32_t)num_threads) : 0;

  duration_ms = global->bench_duration_ms > 0 ? global->bench_duration_ms : 10000;
  max_requests = (uint64_t)global->bench_requests;

  workers = (ao_bench_worker_t *)calloc((size_t)num_threads, sizeof(ao_bench_worker_t));
  if(!workers) {
    free(headers_buf);
    return CURLE_OUT_OF_MEMORY;
  }

  /* Banner */
  curl_mprintf("\n================================================================================\n");
  curl_mprintf(" ao-bench: Stealth Silicon Load Engine (powered by libaoni)\n");
  curl_mprintf(" Target:      %s\n", url);
  curl_mprintf(" Method:      %s\n", method);
  curl_mprintf(" Setup:       %d Worker Threads | %u Concurrency | Duration: %.1fs | Pipeline: %s\n",
               num_threads, total_concurrency, (double)duration_ms / 1000.0,
               global->bench_pipeline ? "RFC 9112 §9.3.2 (Active)" : "Off");
  curl_mprintf(" uTLS Engine: %s\n", (browser_profile == AONI_BROWSER_CHROME) ? "Chrome (JA4 + ECH RFC 9460)" : "Plaintext");
  if(total_rate > 0)
    curl_mprintf(" Rate Limit:  %u RPS\n", total_rate);
  curl_mprintf("================================================================================\n\n");

  start_time_ns = get_time_ns();
  prev_time_ns = start_time_ns;

  /* Spawn workers */
  for(t = 0; t < num_threads; t++) {
#ifdef _WIN32
    uintptr_t th;
#endif
    workers[t].worker_id = t;
    workers[t].num_threads = num_threads;
    workers[t].concurrency = thread_concurrency;
    workers[t].rate_limit_rps = thread_rate;
    workers[t].max_requests = max_requests;
    workers[t].duration_ms = duration_ms;
    workers[t].url = url;
    workers[t].method = method;
    workers[t].headers_raw = (const uint8_t *)headers_buf;
    workers[t].headers_len = headers_len;
    workers[t].body_ptr = (const uint8_t *)config->postfields;
    workers[t].body_len = config->postfields ? strlen(config->postfields) : 0;
    workers[t].browser_profile = browser_profile;
    workers[t].enable_http2 = (config->httpversion >= CURL_HTTP_VERSION_2_0 || config->httpversion == CURL_HTTP_VERSION_NONE) ? 1 : 0;
    workers[t].enable_http3 = (config->httpversion >= CURL_HTTP_VERSION_3) ? 1 : 0;
    workers[t].enable_pipeline = global->bench_pipeline ? 1 : 0;
    workers[t].proxy_url = proxy_url;
    workers[t].stop_flag = &stop_flag;
    workers[t].global_req_counter = &global_req_counter;

#ifdef _WIN32
    th = _beginthreadex(NULL, 0, bench_worker_proc, &workers[t], 0, NULL);
    workers[t].thread_handle = (HANDLE)th;
#else
    pthread_create(&workers[t].thread_id, NULL, bench_worker_proc, &workers[t]);
#endif
  }

  /* Live Console Dashboard loop (10 Hz) */
  while(!stop_flag) {
    uint64_t now_ns;
    double elapsed_sec;
    double total_dur_sec;
    double progress;
    uint64_t cur_reqs = 0;
    uint64_t cur_bytes = 0;
    double live_rps = 0.0;
    double live_mbps = 0.0;
    int bar_width = 25;
    int pos;
    char bar[32];
    double dt;

    ao_sleep_ms(100);
    now_ns = get_time_ns();
    elapsed_sec = (double)(now_ns - start_time_ns) / 1000000000.0;
    total_dur_sec = (double)duration_ms / 1000.0;
    progress = (total_dur_sec > 0.0) ? (elapsed_sec / total_dur_sec) : 0.0;

    if(progress > 1.0)
      progress = 1.0;

    for(t = 0; t < num_threads; t++) {
      cur_reqs += workers[t].stats.requests_completed;
      cur_bytes += workers[t].stats.bytes_transferred;
    }

    dt = (double)(now_ns - prev_time_ns) / 1000000000.0;
    if(dt > 0.05) {
      live_rps = (double)(cur_reqs - prev_reqs) / dt;
      live_mbps = (double)(cur_bytes - prev_bytes) * 8.0 / (dt * 1000000.0);
      prev_reqs = cur_reqs;
      prev_bytes = cur_bytes;
      prev_time_ns = now_ns;
    }

    pos = (int)(progress * (double)bar_width);
    memset(bar, '=', (size_t)pos);
    if(pos < bar_width) {
      bar[pos] = '>';
      memset(bar + pos + 1, ' ', (size_t)(bar_width - pos - 1));
    }
    bar[bar_width] = '\0';

    curl_mprintf("\r [%s] %4.1fs / %4.1fs (%4.1f%%) | %8.0f RPS | %7.1f Mbps",
                 bar, elapsed_sec, total_dur_sec, progress * 100.0, live_rps, live_mbps);
    fflush(stdout);

    if((now_ns - start_time_ns) >= ((uint64_t)duration_ms * 1000000ULL)) {
      stop_flag = 1;
      break;
    }
  }

  stop_flag = 1;

  /* Join threads */
  for(t = 0; t < num_threads; t++) {
#ifdef _WIN32
    if(workers[t].thread_handle) {
      WaitForSingleObject(workers[t].thread_handle, INFINITE);
      CloseHandle(workers[t].thread_handle);
    }
#else
    pthread_join(workers[t].thread_id, NULL);
#endif
  }

  end_time_ns = get_time_ns();
  curl_mprintf("\n\n");

  /* Aggregate metrics */
  memset(&agg, 0, sizeof(agg));
  for(t = 0; t < num_threads; t++) {
    size_t i;
    agg.requests_completed += workers[t].stats.requests_completed;
    agg.bytes_transferred += workers[t].stats.bytes_transferred;
    agg.status_2xx += workers[t].stats.status_2xx;
    agg.status_3xx += workers[t].stats.status_3xx;
    agg.status_4xx += workers[t].stats.status_4xx;
    agg.status_5xx += workers[t].stats.status_5xx;
    agg.errors_timeout += workers[t].stats.errors_timeout;
    agg.errors_network += workers[t].stats.errors_network;
    agg.sum_dns_ns += workers[t].stats.sum_dns_ns;
    agg.sum_tls_ns += workers[t].stats.sum_tls_ns;
    agg.sum_ttfb_ns += workers[t].stats.sum_ttfb_ns;
    agg.sum_total_ns += workers[t].stats.sum_total_ns;

    if(workers[t].stats.min_total_ns < agg.min_total_ns || agg.min_total_ns == 0)
      agg.min_total_ns = workers[t].stats.min_total_ns;
    if(workers[t].stats.max_total_ns > agg.max_total_ns)
      agg.max_total_ns = workers[t].stats.max_total_ns;

    for(i = 0; i < AO_HISTOGRAM_BUCKETS; i++)
      agg.histogram[i] += workers[t].stats.histogram[i];
  }

  /* Print Final Report */
  {
    double total_sec = (double)(end_time_ns - start_time_ns) / 1000000000.0;
    double avg_rps = (total_sec > 0.0) ? ((double)agg.requests_completed / total_sec) : 0.0;
    double avg_mbps = (total_sec > 0.0) ? (((double)agg.bytes_transferred * 8.0) / (total_sec * 1000000.0)) : 0.0;
    double avg_dns_ms = agg.requests_completed ? ((double)agg.sum_dns_ns / (double)agg.requests_completed / 1000000.0) : 0.0;
    double avg_tls_ms = agg.requests_completed ? ((double)agg.sum_tls_ns / (double)agg.requests_completed / 1000000.0) : 0.0;
    double avg_ttfb_ms = agg.requests_completed ? ((double)agg.sum_ttfb_ns / (double)agg.requests_completed / 1000000.0) : 0.0;
    double avg_total_ms = agg.requests_completed ? ((double)agg.sum_total_ns / (double)agg.requests_completed / 1000000.0) : 0.0;

    double p50 = get_percentile(agg.histogram, agg.requests_completed, 50.0);
    double p75 = get_percentile(agg.histogram, agg.requests_completed, 75.0);
    double p90 = get_percentile(agg.histogram, agg.requests_completed, 90.0);
    double p95 = get_percentile(agg.histogram, agg.requests_completed, 95.0);
    double p99 = get_percentile(agg.histogram, agg.requests_completed, 99.0);
    double p999 = get_percentile(agg.histogram, agg.requests_completed, 99.9);
    double p9999 = get_percentile(agg.histogram, agg.requests_completed, 99.99);

    curl_mprintf("============================= Benchmark Results ===============================\n");
    curl_mprintf(" Completed Requests: %" CURL_FORMAT_CURL_OFF_T "\n", (curl_off_t)agg.requests_completed);
    curl_mprintf(" Total Transferred:  %.2f MB\n", (double)agg.bytes_transferred / (1024.0 * 1024.0));
    curl_mprintf(" Actual Duration:    %.3f seconds\n", total_sec);
    curl_mprintf(" Peak Throughput:    %.0f RPS  (%.2f Mbps wire speed)\n\n", avg_rps, avg_mbps);

    curl_mprintf(" Status Codes:\n");
    curl_mprintf("   2xx Success:      %" CURL_FORMAT_CURL_OFF_T " (%.1f%%)\n",
                 (curl_off_t)agg.status_2xx, agg.requests_completed ? ((double)agg.status_2xx * 100.0 / (double)agg.requests_completed) : 0.0);
    if(agg.status_3xx)
      curl_mprintf("   3xx Redirects:    %" CURL_FORMAT_CURL_OFF_T "\n", (curl_off_t)agg.status_3xx);
    if(agg.status_4xx)
      curl_mprintf("   4xx Client Err:   %" CURL_FORMAT_CURL_OFF_T "\n", (curl_off_t)agg.status_4xx);
    if(agg.status_5xx)
      curl_mprintf("   5xx Server Err:   %" CURL_FORMAT_CURL_OFF_T "\n", (curl_off_t)agg.status_5xx);
    if(agg.errors_timeout || agg.errors_network) {
      curl_mprintf("   Timeouts:         %" CURL_FORMAT_CURL_OFF_T "\n", (curl_off_t)agg.errors_timeout);
      curl_mprintf("   Network Errors:   %" CURL_FORMAT_CURL_OFF_T "\n", (curl_off_t)agg.errors_network);
    }

    curl_mprintf("\n Latency Profile (HDR Nanosecond Telemetry):\n");
    if(avg_total_ms < 1.0) {
      curl_mprintf("   Phase Breakdown:   DNS: %.2f ms (%.0f µs) | TLS: %.2f ms (%.0f µs) | TTFB: %.2f ms (%.0f µs) | Total: %.2f ms (%.0f µs)\n",
                   avg_dns_ms, avg_dns_ms * 1000.0,
                   avg_tls_ms, avg_tls_ms * 1000.0,
                   avg_ttfb_ms, avg_ttfb_ms * 1000.0,
                   avg_total_ms, avg_total_ms * 1000.0);
    }
    else {
      curl_mprintf("   Phase Breakdown:   DNS: %.2f ms | TLS: %.2f ms | TTFB: %.2f ms | Total: %.2f ms\n",
                   avg_dns_ms, avg_tls_ms, avg_ttfb_ms, avg_total_ms);
    }
    curl_mprintf("   --------------------------------------------------------\n");
    if(p50 < 1.0)
      curl_mprintf("   p50:     %7.2f ms (%4.0f µs)\n", p50, p50 * 1000.0);
    else
      curl_mprintf("   p50:     %7.2f ms\n", p50);

    if(p75 < 1.0)
      curl_mprintf("   p75:     %7.2f ms (%4.0f µs)\n", p75, p75 * 1000.0);
    else
      curl_mprintf("   p75:     %7.2f ms\n", p75);

    if(p90 < 1.0)
      curl_mprintf("   p90:     %7.2f ms (%4.0f µs)\n", p90, p90 * 1000.0);
    else
      curl_mprintf("   p90:     %7.2f ms\n", p90);

    if(p95 < 1.0)
      curl_mprintf("   p95:     %7.2f ms (%4.0f µs)\n", p95, p95 * 1000.0);
    else
      curl_mprintf("   p95:     %7.2f ms\n", p95);

    if(p99 < 1.0)
      curl_mprintf("   p99:     %7.2f ms (%4.0f µs)\n", p99, p99 * 1000.0);
    else
      curl_mprintf("   p99:     %7.2f ms\n", p99);

    if(p999 < 1.0)
      curl_mprintf("   p99.9:   %7.2f ms (%4.0f µs)\n", p999, p999 * 1000.0);
    else
      curl_mprintf("   p99.9:   %7.2f ms\n", p999);

    if(p9999 < 1.0)
      curl_mprintf("   p99.99:  %7.2f ms (%4.0f µs)\n", p9999, p9999 * 1000.0);
    else
      curl_mprintf("   p99.99:  %7.2f ms\n", p9999);

    {
      double min_ms = (double)agg.min_total_ns / 1000000.0;
      double max_ms = (double)agg.max_total_ns / 1000000.0;
      if(min_ms < 1.0)
        curl_mprintf("   Min:     %7.2f ms (%4.0f µs)\n", min_ms, min_ms * 1000.0);
      else
        curl_mprintf("   Min:     %7.2f ms\n", min_ms);

      if(max_ms < 1.0)
        curl_mprintf("   Max:     %7.2f ms (%4.0f µs)\n", max_ms, max_ms * 1000.0);
      else
        curl_mprintf("   Max:     %7.2f ms\n", max_ms);
    }
    curl_mprintf("================================================================================\n\n");
  }

  free(workers);
  free(headers_buf);

  return CURLE_OK;
}
