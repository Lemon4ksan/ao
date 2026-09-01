/*
 * Copyright (c) 2026 Lemon4ksan All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#ifndef HEADER_CURL_TOOL_BENCH_H
#define HEADER_CURL_TOOL_BENCH_H

#include "tool_setup.h"
#include "tool_cfgable.h"

/*
 * Execute a multi-threaded benchmark load test for the current operation config.
 */
CURLcode bench_transfers(CURLSH *share);

#endif /* HEADER_CURL_TOOL_BENCH_H */
