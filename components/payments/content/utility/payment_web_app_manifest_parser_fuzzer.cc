// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/logging.h"
#include "components/payments/content/utility/payment_manifest_parser.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

Environment* env = new Environment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  payments::PaymentManifestParser::ParseWebAppManifestIntoVector(
      std::string(reinterpret_cast<const char*>(data), size));
  return 0;
}
