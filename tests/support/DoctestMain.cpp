// The one translation unit that holds doctest's implementation and main(). Every test
// executable links it, so each one accepts the doctest command line (--test-case=, --list-test-cases).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
