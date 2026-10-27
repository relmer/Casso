#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


//  The build property and the compiler flag move together: the property sets
//  /fsanitize=address, which defines __SANITIZE_ADDRESS__, and defines
//  CASSO_SANITIZE_ADDRESS beside it.
#if defined(CASSO_SANITIZE_ADDRESS) != defined(__SANITIZE_ADDRESS__)
static_assert (false, "CASSO_SANITIZE_ADDRESS is defined exactly when __SANITIZE_ADDRESS__ is");
#endif


static constexpr size_t  s_kCanaryBytes = 64;





////////////////////////////////////////////////////////////////////////////////
//
//  SanitizerCanaryTests
//
//  SC-002 depends on the AddressSanitizer build, so this test checks that
//  build: freed memory is poisoned and live memory is not. Outside the
//  sanitizer build the test checks nothing at run time.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SanitizerCanaryTests)
{
public:

    TEST_METHOD (TheSanitizerBuildPoisonsFreedMemory)
    {
#ifdef __SANITIZE_ADDRESS__
        auto                    live  = std::make_unique<Byte[]> (s_kCanaryBytes);
        auto                    freed = std::make_unique<Byte[]> (s_kCanaryBytes);
        const volatile Byte   * alias = freed.get();



        freed.reset();

        Assert::AreEqual (1, __asan_address_is_poisoned (alias),      L"a freed block is poisoned");
        Assert::AreEqual (0, __asan_address_is_poisoned (live.get()), L"a live block is not");
#endif
    }
};
