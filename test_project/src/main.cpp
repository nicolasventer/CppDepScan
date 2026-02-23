#include "foo.hpp"
#include "bar.hpp"
#include <iostream>

int main() {
    test::foo();
    test::bar("hello");
    std::cout << "version " << test::version() << "\n";
    return 0;
}
