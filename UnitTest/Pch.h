#pragma once



#include "../CassoEmuCore/Pch.h"
#include "../CassoCore/Pch.h"

// winnt.h #defines these as intrinsic aliases (_bittest etc.) which
// mangles unrelated test calls like CpuOperations::BitTest into
// CpuOperations::_bittest at link time. We don't use the intrinsics
// from test code, so undefine them right here in the test Pch.
#undef BitTest
#undef BitTestAndSet
#undef BitTestAndReset
#undef BitTestAndComplement

#include <crtdbg.h>
#include <cstdlib>

// Tests that exercise Casso/Ui/* (which depend on D3D11 + WRL)
// resolve `#include "Pch.h"` to THIS file, so the system headers
// must be reachable here. Casso/Pch.h pulls these in for the GUI
// project; we mirror the subset the UI types need.
#include <d3d11.h>
#include <wrl/client.h>
#include <dwmapi.h>
#include <uxtheme.h>

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#include <CppUnitTest.h>
