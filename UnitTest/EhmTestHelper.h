#pragma once

#include "Ehm.h"



namespace UnitTestHelpers
{
    // Redirect EHM assertions to CppUnitTestFramework Assert::Fail()
    // and suppress CRT assert dialogs in Debug builds.
    void SetupForUnitTests ();
}
