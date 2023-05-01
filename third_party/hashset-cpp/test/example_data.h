/* Copyright (c) 2015 Brian R. Bondy. Distributed under the MPL2 license.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_EXAMPLE_DATA_H_
#define TEST_EXAMPLE_DATA_H_

#include <math.h>
#include <string.h>
#include "../hashFn.h"

static HashFn h(19);

class ExampleData {
 public:
  uint64_t GetHash() const {
    return h(data_, data_len_);
  }

  ~ExampleData() {
    if (data_ && !borrowed_memory_) {
      delete[] data_;
    }
  }
  explicit ExampleData(const char *data) {
    data_len_ = static_cast<uint32_t>(strlen(data)) + 1;
    data_ = new char[data_len_];
    memcpy(data_, data, data_len_);
    borrowed_memory_ = false;
    extra_data_ = 0;
  }

  ExampleData(const char *data, int data_len) {
    data_len_ = data_len;
    data_ = new char[data_len];
    memcpy(data_, data, data_len);
    borrowed_memory_ = false;
    extra_data_ = 0;
  }

  ExampleData(const ExampleData &rhs) {
    data_len_ = rhs.data_len_;
    data_ = new char[data_len_];
    memcpy(data_, rhs.data_, data_len_);
    borrowed_memory_ = rhs.borrowed_memory_;
    extra_data_ = rhs.extra_data_;
  }

  ExampleData() : extra_data_(0), data_(nullptr), data_len_(0),
    borrowed_memory_(false) {
  }

  bool operator==(const ExampleData &rhs) const {
    if (data_len_ != rhs.data_len_) {
      return false;
    }

    return !memcmp(data_, rhs.data_, data_len_);
  }

  bool operator!=(const ExampleData &rhs) const {
    return !(*this == rhs);
  }

  void Update(const ExampleData &other) {
    extra_data_ = extra_data_ | other.extra_data_;
  }

  uint32_t Serialize(char *buffer) {
    uint32_t total_size = 0;
    char sz[32];
    uint32_t data_len_size = 1 + snprintf(sz, sizeof(sz), "%x", data_len_);
    if (buffer) {
      memcpy(buffer + total_size, sz, data_len_size);
    }
    total_size += data_len_size;
    if (buffer) {
      memcpy(buffer + total_size, data_, data_len_);
    }
    total_size += data_len_;

    if (buffer) {
      buffer[total_size] = extra_data_;
    }
    total_size++;

    return total_size;
  }

  uint32_t Deserialize(char *buffer, uint32_t buffer_size) {
    data_len_ = 0;
    if (!HasNewlineBefore(buffer, buffer_size)) {
      return 0;
    }
    sscanf(buffer, "%x", &data_len_);
    uint32_t consumed = static_cast<uint32_t>(strlen(buffer)) + 1;
    if (consumed + data_len_ >= buffer_size) {
      return 0;
    }
    data_ = buffer + consumed;
    borrowed_memory_ = true;
    memcpy(data_, buffer + consumed, data_len_);
    consumed += data_len_;

    extra_data_ = buffer[consumed];
    consumed++;

    return consumed;
  }

  // Just an example which is not used in comparisons but
  // is used for serializing / deserializing, showing the
  // need for find vs exists.
  char extra_data_;

 private:
  bool HasNewlineBefore(char *buffer, uint32_t buffer_size) {
    char *p = buffer;
    for (uint32_t i = 0; i < buffer_size; ++i) {
      if (*p == '\0')
        return true;
      p++;
    }
    return false;
  }

  char *data_;
  uint32_t data_len_;
  bool borrowed_memory_;
};

#endif  // TEST_EXAMPLE_DATA_H_
