// Main entry: includes allowed, forbidden, unresolved, and (with --std) standard headers.
#include "app/app.hpp"
#include "lib/public.hpp"
#include "lib/private.hpp"  // Forbidden when -A src/main src/app -A src/main include
#include "extra.hpp"        // From include/ via -I include
#include <iostream>
#include "nonexistent.h"    // Unresolved (no such file)

int main() {
    app_hello();
    return 0;
}
