# Hash Set

[![Build Status](https://travis-ci.org/bbondy/hashset-cpp.svg?branch=master)](https://travis-ci.org/bbondy/hashset-cpp)

Implements a simple HashSet for strings in environments where you don't have the std lib available.
You should probably not be using this. Instead consider using `hash_set` which is a more generic implementation with templates.
This is only useful for very specific use cases having specific memory layout requirements.

## Setup

```
npm install --save hashset-cpp
```

## Sample

```c++
#include <iostream>
#include "hash_set.h"
#include "test/example_data.h"

using std::cout;
using std::endl;

int main(int argc, char **argv) {
  // Bucket size is 256 and don't allow multiple items per item hash.
  HashSet<ExampleData> set(256, false);
  set.Add(ExampleData("test"));

  // Prints true
  cout << "test exists: " << (set.Exists(ExampleData("test"))
      ? "true" : "false") << endl;
  // Prints false
  cout << "test2 exists: " << (set.Exists(ExampleData("test2"))
      ? "true" : "false") << endl;

  uint32_t len;
  char * buffer = set.Serialize(&len);
  HashSet<ExampleData> set2(0, false);
  set2.Deserialize(buffer, len);
  // Prints true
  cout << "test exists: " << (set2.Exists(ExampleData("test"))
      ? "true" : "false") << endl;
  // Prints false
  cout << "test2 exists: " << (set2.Exists(ExampleData("test2"))
      ? "true" : "false") << endl;

  delete[] buffer;
  return 0;
}
```

## Build everything in release

```
make
```

## Running sample

```
make sample
```

## Running tests

```
make test
```

## Clearing build files
```
make clean
```

## Linting
```
npm run lint
```
