#include "Pch.h"

#include "EhmTestHelper.h"
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
    NumberFormat::SetSettings (&s_kEnglish);
}
