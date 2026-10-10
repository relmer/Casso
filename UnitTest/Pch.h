#pragma once



// CassoEmuCore/Pch.h must come first: it includes <windows.h> before
// Ehm.h, so Ehm.h binds to the real winerror.h HRESULT codes instead
// of its portable-path fallbacks (which winerror.h would then
// redefine, spraying C4005 through every PCH rebuild).
#include "../CassoEmuCore/Pch.h"
#include "../CassoCore/Pch.h"
#include "Pch.h"

// winnt.h #defines these as intrinsic aliases (_bittest etc.) which
// mangles unrelated test calls like CpuOperations::BitTest into
// CpuOperations::_bittest at link time. We don't use the intrinsics
// from test code, so undefine them right here in the test Pch.
#undef BitTest
#undef BitTestAndSet
#undef BitTestAndReset
#undef BitTestAndComplement

#include <cstdlib>
#include <cstring>
#include <exception>
#include <future>
#include <regex>

#include <CppUnitTest.h>

// The AddressSanitizer flavor (CassoSanitize=Address) checks for a report
// and for poisoned memory through the sanitizer's own interface.
#ifdef __SANITIZE_ADDRESS__
#include <sanitizer/asan_interface.h>
#endif

#include "HResultAssert.h"
