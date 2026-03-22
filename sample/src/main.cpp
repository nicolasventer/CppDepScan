// Main entry: includes allowed, forbidden, unresolved, and (with --std) standard headers.
#include "app/app.hpp"
#include "extra.hpp"	   // From include/ via -I include
#include "lib/private.hpp" // Forbidden when -A src/main src/app -A src/main include
#include "lib/public.hpp"
#include "nonexistent.h" // Unresolved (no such file)
#include <iostream>

int main()
{
	app_hello();
	return 0;
}
