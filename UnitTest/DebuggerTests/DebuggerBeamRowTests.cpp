#include "Pch.h"

#include "ControllerRig.h"
#include "Machines/Apple2/Common/VideoTiming.h"
#include "Ui/Debugger/DebuggerViewState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerBeamRowTests
//
//  The registers pane ends with the beam: its scanline and cycle in hex, as
//  VIDEOINFO gives them, and beside them the same in decimal with the
//  blanking the beam is in.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebuggerBeamRowTests)
    {
    public:

        using Beam = DebuggerViewSnapshot::BeamState;



        TEST_METHOD (TheValueIsScanlineAndCycleInHex)
        {
            Assert::AreEqual (std::string ("02A:11"), DebuggerViewState::GetBeamValue (Beam { 42, 17 }));
            Assert::AreEqual (std::string ("105:40"), DebuggerViewState::GetBeamValue (Beam { 261, 64 }));
        }



        TEST_METHOD (TheNoteSaysWhichBlankingTheBeamIsIn)
        {
            Assert::AreEqual (std::string ("Scanline 42, cycle 30"),                     DebuggerViewState::GetBeamNote (Beam { 42, 30 }));
            Assert::AreEqual (std::string ("Scanline 42, cycle 3, horizontal blank"),    DebuggerViewState::GetBeamNote (Beam { 42, 3 }));
            Assert::AreEqual (std::string ("Scanline 192, cycle 30, vertical blank"),    DebuggerViewState::GetBeamNote (Beam { 192, 30 }));
            Assert::AreEqual (std::string ("Scanline 24, cycle 24, horizontal blank"),   DebuggerViewState::GetBeamNote (Beam { 24, 24 }));
            Assert::AreEqual (std::string ("Scanline 24, cycle 25"),                     DebuggerViewState::GetBeamNote (Beam { 24, 25 }));
        }



        TEST_METHOD (TheSnapshotCarriesTheMachinesBeam)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot = rig.view.Build (rig.controller.GetSession());
            const VideoTiming   * timing   = rig.machine.GetVideoTiming();



            Assert::IsNotNull (timing);
            Assert::IsTrue    (snapshot.beam.has_value());
            Assert::AreEqual  (timing->GetCurrentScanline(), snapshot.beam->scanline);
            Assert::AreEqual  (timing->GetHorizontalPos(),   snapshot.beam->cycle);
        }
    };
}
