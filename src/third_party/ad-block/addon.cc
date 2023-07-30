/* Copyright (c) 2015 Brian R. Bondy. Distributed under the MPL2 license.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <node.h>
#include "./hash_set_wrap.h"

namespace HashSetWrap {

using v8::Local;
using v8::Object;

void InitAll(Local<Object> exports) {
  HashSetWrap::Init(exports);
}

NODE_MODULE(addon, InitAll)

}  // namespace HashSetWrap
