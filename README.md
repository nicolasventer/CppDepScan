# XXXX

# Description

XXXX is a **...** that allows you to **...**.

# Features

- feature 1
- feature 2

# Installation

### Header only

Include the [`xxxx.hpp`](xxxx.hpp) anywhere you want to use it.  
And above only one include, define `XXXX_IMPLEMENTATION` like this:

```cpp
#define XXXX_IMPLEMENTATION
#include "xxxx.hpp"
```

### Header and source

Include the [`xxxx.hpp`](xxxx.hpp) anywhere you want to use it.    
Compile with the [`xxxx.cpp`](xxxx.cpp) file or with the built dll available [here](https://github.com/nicolasventer/cpp-template-repo/releases).

### Build the dll

```bash
g++ -shared -O3 -fPIC -static -o xxxx.dll xxxx.cpp
```

Note: the `-static` flag is required.

### Requirements

c++11 or later required for compilation.  
No external dependencies.

# Example

*Content of [example.cpp](example.cpp):*
```cpp
#include <iostream>

int main() { return 0; }
```

Output:
```
...
```

# Usage

```cpp	
// XXYY ...
```

# Licence

MIT Licence. See [LICENSE file](LICENSE).
Please refer me with:

	Copyright (c) Nicolas VENTER All rights reserved.
