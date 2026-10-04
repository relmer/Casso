#include "Pch.h"

#include "ControllerRig.h"
#include "Machines/Apple2/Common/VideoTiming.h"
#include "Ui/Debugger/DebuggerViewState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerBeamRowTests
//
//  The snapshot holds the machine's beam, which the status bar shows.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebuggerBeamRowTests)
    {
    public:

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
