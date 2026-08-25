/*
 * Copyright (c) 2026 Lemon4ksan All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#ifndef HEADER_CURL_AONI_BRIDGE_H
#define HEADER_CURL_AONI_BRIDGE_H

#include "curl_setup.h"
#include "urldata.h"

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

#ifdef __cplusplus
}
#endif

#endif /* HEADER_CURL_AONI_BRIDGE_H */
