/* Copyright (c) 2015 Brian R. Bondy. Distributed under the MPL2 license.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "./CppUnitLite/TestHarness.h"
#include "./CppUnitLite/Test.h"
#include "./hash_set.h"
#include "./example_data.h"
#include "./hashFn.h"

TEST(hash_set, not_multi_set) {
  {
    HashSet<ExampleData> hash_set(2, false);
    hash_set.Add(ExampleData("test"));
    uint32_t len;
    char *buffer = hash_set.Serialize(&len);
    HashSet<ExampleData> hash_set2(0, false);
    hash_set2.Deserialize(buffer, len);
    hash_set2.Exists(ExampleData("test"));
  }

  HashSet<ExampleData> hash_sets[] = {HashSet<ExampleData>(1, false),
    HashSet<ExampleData>(2, false), HashSet<ExampleData>(500, false)};
  for (unsigned int i = 0; i < sizeof(hash_sets) / sizeof(hash_sets[0]); i++) {
    HashSet<ExampleData> &hash_set = hash_sets[i];
    CHECK(!hash_set.IsMultiSet());
    LONGS_EQUAL(0, hash_set.GetSize());
    hash_set.Add(ExampleData("test"));
    LONGS_EQUAL(1, hash_set.GetSize());
    CHECK(hash_set.Exists(ExampleData("test")));
    hash_set.Add(ExampleData("test"));
    CHECK(hash_set.Exists(ExampleData("test")));
    LONGS_EQUAL(1, hash_set.GetSize());
    hash_set.Add(ExampleData("test2"));
    CHECK(hash_set.Exists(ExampleData("test2")));
    LONGS_EQUAL(2, hash_set.GetSize());
    hash_set.Add(ExampleData("test3"));
    CHECK(hash_set.Exists(ExampleData("test3")));
    hash_set.Add(ExampleData("test4"));
    CHECK(hash_set.Exists(ExampleData("test4")));

    // Check that a smaller substring of something that exists, doesn't exist
    CHECK(!hash_set.Exists(ExampleData("tes")));
    // Check that a longer string of something that exists, doesn't exist
    CHECK(!hash_set.Exists(ExampleData("test22")));
    CHECK(!hash_set.Exists(ExampleData("test5")));

    LONGS_EQUAL(4, hash_set.GetSize());
    hash_set.Add(ExampleData("a\0b\0\0c", 6));
    LONGS_EQUAL(5, hash_set.GetSize());
    CHECK(!hash_set.Exists(ExampleData("a")));
    CHECK(!hash_set.Exists(ExampleData("a", 1)));
    CHECK(hash_set.Exists(ExampleData("a\0b\0\0c", 6)));
    CHECK(!hash_set.Exists(ExampleData("a\0b\0\0c", 7)));

    // Test that remove works
    LONGS_EQUAL(5, hash_set.GetSize());
    CHECK(hash_set.Exists(ExampleData("test2")));
    CHECK(hash_set.Remove(ExampleData("test2")));
    LONGS_EQUAL(4, hash_set.GetSize());
    CHECK(!hash_set.Exists(ExampleData("test2")));
    CHECK(!hash_set.Remove(ExampleData("test2")));
    LONGS_EQUAL(4, hash_set.GetSize());
    CHECK(hash_set.Add(ExampleData("test2")));
    LONGS_EQUAL(5, hash_set.GetSize());

    // Try to find something that doesn't exist
    CHECK(hash_set.Find(ExampleData("fdsafasd")) == nullptr);

    // Show how extra data works
    ExampleData item("ok");
    item.extra_data_ = 1;
    hash_set.Add(item);

    ExampleData *p = hash_set.Find(ExampleData("ok"));
    LONGS_EQUAL(1, p->extra_data_);

    item.extra_data_ = 2;
    hash_set.Add(item);
    LONGS_EQUAL(6, hash_set.GetSize());
    // ExampleData is configuredd to merge extra_data_ on updates
    LONGS_EQUAL(3, p->extra_data_);
  }

  uint32_t len = 0;
  for (unsigned int i = 0; i < sizeof(hash_sets) / sizeof(hash_sets[0]); i++) {
    HashSet<ExampleData> &hs1 = hash_sets[i];
    char *buffer = hs1.Serialize(&len);
    HashSet<ExampleData> dhs(0, false);
    // Deserializing some invalid data should fail
    CHECK(!dhs.Deserialize(const_cast<char*>("31131"), 2));
    CHECK(dhs.Deserialize(buffer, len));
    CHECK(dhs.Exists(ExampleData("test")));
    CHECK(dhs.Exists(ExampleData("test2")));
    CHECK(dhs.Exists(ExampleData("test3")));
    CHECK(dhs.Exists(ExampleData("test4")));
    CHECK(!dhs.Exists(ExampleData("tes")));
    CHECK(!dhs.Exists(ExampleData("test22")));
    CHECK(!dhs.Exists(ExampleData("test5")));
    CHECK(!dhs.Exists(ExampleData("a")));
    CHECK(!dhs.Exists(ExampleData("a", 1)));
    CHECK(dhs.Exists(ExampleData("a\0b\0\0c", 6)));
    CHECK(!dhs.Exists(ExampleData("a\0b\0\0c", 7)));
    LONGS_EQUAL(6, dhs.GetSize());

    // Make sure  HashSet clears correctly
    CHECK(dhs.Exists(ExampleData("test")));
    dhs.Clear();
    CHECK(!dhs.Exists(ExampleData("test")));

    delete[] buffer;
  }

  // Make sure HashFn produces the correct hash
  HashFn h(19, false);
  HashFn h2(19, true);
  const char *sz = "facebook.com";
  const char *sz2 = "abcde";
  LONGS_EQUAL(h(sz, strlen(sz)), 12510474367240317);
  LONGS_EQUAL(h2(sz, strlen(sz)), 12510474367240317);
  LONGS_EQUAL(h(sz2, strlen(sz2)), 13351059);
  LONGS_EQUAL(h2(sz2, strlen(sz2)), 13351059);
}

TEST(hash_set, multi_set) {
  {
    HashSet<ExampleData> hash_set(2, true);
    hash_set.Add(ExampleData("test"));
    uint32_t len;
    char *buffer = hash_set.Serialize(&len);
    HashSet<ExampleData> hash_set2(0, true);
    hash_set2.Deserialize(buffer, len);
    hash_set2.Exists(ExampleData("test"));
  }

  HashSet<ExampleData> hash_sets[] = {HashSet<ExampleData>(1, true),
    HashSet<ExampleData>(2, true), HashSet<ExampleData>(500, true)};
  for (unsigned int i = 0; i < sizeof(hash_sets) / sizeof(hash_sets[0]); i++) {
    HashSet<ExampleData> &hash_set = hash_sets[i];
    CHECK(hash_set.IsMultiSet());
    LONGS_EQUAL(0, hash_set.GetSize());
    hash_set.Add(ExampleData("test"), false);
    LONGS_EQUAL(1, hash_set.GetSize());
    CHECK(hash_set.Exists(ExampleData("test")));
    hash_set.Add(ExampleData("test"), false);
    CHECK(hash_set.Exists(ExampleData("test")));
    LONGS_EQUAL(2, hash_set.GetSize());
    hash_set.Add(ExampleData("test2"), false);
    CHECK(hash_set.Exists(ExampleData("test2")));
    LONGS_EQUAL(3, hash_set.GetSize());
    hash_set.Add(ExampleData("test3"), false);
    CHECK(hash_set.Exists(ExampleData("test3")));
    hash_set.Add(ExampleData("test4"), false);
    CHECK(hash_set.Exists(ExampleData("test4")));

    // Make sure multi set has 2 items for test's hash
    LONGS_EQUAL(2, hash_set.GetMatchingCount(ExampleData("test")))

    // Check that a smaller substring of something that exists, doesn't exist
    CHECK(!hash_set.Exists(ExampleData("tes")));
    // Check that a longer string of something that exists, doesn't exist
    CHECK(!hash_set.Exists(ExampleData("test22")));
    CHECK(!hash_set.Exists(ExampleData("test5")));

    LONGS_EQUAL(5, hash_set.GetSize());
    hash_set.Add(ExampleData("a\0b\0\0c", 6), false);
    LONGS_EQUAL(6, hash_set.GetSize());
    CHECK(!hash_set.Exists(ExampleData("a")));
    CHECK(!hash_set.Exists(ExampleData("a", 1)));
    CHECK(hash_set.Exists(ExampleData("a\0b\0\0c", 6)));
    CHECK(!hash_set.Exists(ExampleData("a\0b\0\0c", 7)));

    // Test that remove works
    LONGS_EQUAL(6, hash_set.GetSize());
    CHECK(hash_set.Exists(ExampleData("test2")));
    CHECK(hash_set.Remove(ExampleData("test2")));
    LONGS_EQUAL(5, hash_set.GetSize());
    CHECK(!hash_set.Exists(ExampleData("test2")));
    CHECK(!hash_set.Remove(ExampleData("test2")));
    LONGS_EQUAL(5, hash_set.GetSize());
    CHECK(hash_set.Add(ExampleData("test2"), false));
    LONGS_EQUAL(6, hash_set.GetSize());

    // Try to find something that doesn't exist
    CHECK(hash_set.Find(ExampleData("fdsafasd")) == nullptr);

    // Show how extra data works
    ExampleData item("ok");
    item.extra_data_ = 1;
    hash_set.Add(item, false);

    ExampleData item2("ok");
    item2.extra_data_ = 2;
    hash_set.Add(item2, true);

    ExampleData item3("ok");
    item3.extra_data_ = 4;
    hash_set.Add(item3, false);

    ExampleData *p = hash_set.Find(ExampleData("ok"));
    // ExampleData is configuredd to merge extra_data_ on updates
    LONGS_EQUAL(3, p->extra_data_);

    std::vector<ExampleData *> items;
    hash_set.FindAll(ExampleData("ok"), &items);
    LONGS_EQUAL(2, items.size());
    LONGS_EQUAL(3, items[0]->extra_data_);
    LONGS_EQUAL(4, items[1]->extra_data_);
    LONGS_EQUAL(8, hash_set.GetSize());
  }

  uint32_t len = 0;
  for (unsigned int i = 0; i < sizeof(hash_sets) / sizeof(hash_sets[0]); i++) {
    HashSet<ExampleData> &hs1 = hash_sets[i];
    char *buffer = hs1.Serialize(&len);
    HashSet<ExampleData> dhs(0, true);
    // Deserializing some invalid data should fail
    CHECK(!dhs.Deserialize(const_cast<char*>("31131"), 2));
    CHECK(dhs.Deserialize(buffer, len));
    CHECK(dhs.Exists(ExampleData("test")));
    CHECK(dhs.Exists(ExampleData("test2")));
    CHECK(dhs.Exists(ExampleData("test3")));
    CHECK(dhs.Exists(ExampleData("test4")));
    CHECK(!dhs.Exists(ExampleData("tes")));
    CHECK(!dhs.Exists(ExampleData("test22")));
    CHECK(!dhs.Exists(ExampleData("test5")));
    CHECK(!dhs.Exists(ExampleData("a")));
    CHECK(!dhs.Exists(ExampleData("a", 1)));
    CHECK(dhs.Exists(ExampleData("a\0b\0\0c", 6)));
    CHECK(!dhs.Exists(ExampleData("a\0b\0\0c", 7)));
    LONGS_EQUAL(8, dhs.GetSize());

    // Make sure  HashSet clears correctly
    CHECK(dhs.Exists(ExampleData("test")));
    dhs.Clear();
    CHECK(!dhs.Exists(ExampleData("test")));

    delete[] buffer;
  }

  // Make sure HashFn produces the correct hash
  HashFn h(19, true);
  HashFn h2(19, true);
  const char *sz = "facebook.com";
  const char *sz2 = "abcde";
  LONGS_EQUAL(h(sz, strlen(sz)), 12510474367240317);
  LONGS_EQUAL(h2(sz, strlen(sz)), 12510474367240317);
  LONGS_EQUAL(h(sz2, strlen(sz2)), 13351059);
  LONGS_EQUAL(h2(sz2, strlen(sz2)), 13351059);
}
