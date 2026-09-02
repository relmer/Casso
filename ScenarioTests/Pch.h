#pragma once

//
//  SCENARIO TESTS, NOT UNIT TESTS. Everything in this project needs external
//  inputs -- the stock DOS 3.3 System Master or ProDOS Users Disk, fetched
//  rather than committed --
//  and a booted guest executing real 6502 code. CI has no copy of the master
//  and never will, so these cases live in their own binary: a separate DLL is
//  what makes running them a deliberate act (RunTests.ps1 -Scenario) rather
//  than something a filter forgets to exclude.
//

// CassoEmuCore/Pch.h must come first: it includes <windows.h> before
// Ehm.h, so Ehm.h binds to the real winerror.h HRESULT codes instead
// of its portable-path fallbacks (which winerror.h would then
// redefine, spraying C4005 through every PCH rebuild).
#include "../CassoEmuCore/Pch.h"
#include "../CassoCore/Pch.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <regex>

#include <CppUnitTest.h>

#include "../UnitTest/HResultAssert.h"
