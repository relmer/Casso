#pragma once



#include "../CassoEmuCore/Pch.h"
#include "../CassoCore/Pch.h"
#include "../Casso/Pch.h"

// Opt in to the Dxui umbrella (see the note in Casso/Pch.h). Casso/Pch.h
// above already defines this and pulls Dxui.h, but keep it explicit here
// so this PCH stays correct if that include order ever changes.
#define DXUI_UMBRELLA_VIA_PCH
#include "Dxui.h"

// winnt.h #defines these as intrinsic aliases (_bittest etc.) which
// mangles unrelated test calls like CpuOperations::BitTest into
// CpuOperations::_bittest at link time. We don't use the intrinsics
// from test code, so undefine them right here in the test Pch.
#undef BitTest
#undef BitTestAndSet
#undef BitTestAndReset
#undef BitTestAndComplement

#include <cstdlib>

#include <CppUnitTest.h>
