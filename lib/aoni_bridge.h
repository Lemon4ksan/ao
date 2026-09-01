/*
 * Copyright (c) 2026 Lemon4ksan All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#ifndef HEADER_CURL_AONI_BRIDGE_H
#define HEADER_CURL_AONI_BRIDGE_H

#include "curl_setup.h"
#include "urldata.h"
#include <aoni.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Check if the given transfer should be routed through the aoni silicon reactor.
 */
bool Curl_aoni_is_supported_url(const char *url);

/*
 * Execute a transfer using the aoni uTLS engine and zero-alloc Off-Heap reactor.
 */
CURLcode Curl_aoni_perform(struct Curl_easy *data);

/*
 * Execute a batch of transfers in parallel over the silicon reactor.
 */
CURLcode Curl_aoni_batch_perform(struct Curl_easy **data_array, size_t count);

/*
 * Get a thread-local or isolated aoni client instance for worker threads.
 */
aoni_client_t Curl_aoni_get_thread_client(aoni_config_t *cfg, bool is_tls);

#ifdef __cplusplus
}
#endif

#endif /* HEADER_CURL_AONI_BRIDGE_H */
