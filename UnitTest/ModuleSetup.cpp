#include "Pch.h"

#include "EhmTestHelper.h"
#include "TestSystemState.h"
#include "Core/NumberFormat.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleSetup
//
//  Numbers are written with English (United States) settings, not the PC's
//  region.
//
////////////////////////////////////////////////////////////////////////////////

TEST_MODULE_INITIALIZE (ModuleSetup)
{
    static const NumberSettings  s_kEnglish;



    UnitTestHelpers::SetupForUnitTests();

    //  No key, pointer or setting of this desktop's.
    TestSystemState::Install();

    //  No window of the test process is ever under the pointer.
    DxuiTooltip::SetDefaultActiveTest ([] () { return true; });
    NumberFormat::SetSettings (&s_kEnglish);
}
