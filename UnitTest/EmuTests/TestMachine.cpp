#include "Pch.h"

#include "TestMachine.h"

#include "FixtureRomSource.h"
#include "HResultAssert.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::TestMachine
//
////////////////////////////////////////////////////////////////////////////////

TestMachine::TestMachine (const std::string & machineId, Slots slots)
    : m_builder (*this, m_nothingListening)
{
    FixtureRomSource  source;
    std::string       error;
    std::wstring      wideId = TextEncoding::NarrowToWide (machineId);
    HRESULT           hr     = S_OK;



    hr = HeadlessMachineFactory::Build (*this, m_builder, source, machineId, slots, kSeed, error);

    AssertSucceeded (hr, std::format (L"{} must build: {}",
                                      wideId,
                                      TextEncoding::NarrowToWide (error)).c_str());
}
