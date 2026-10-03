#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/AccessHeatMap.h"
#include "Shell/CpuCommandDispatcher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapViewStateTests
//
//  The snapshot carries the heat map's levels only while its pane is shown,
//  and the build that sees the pane shown or hidden turns the machine's
//  recording on or off to match.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatMapViewStateTests)
    {
    public:

        TEST_METHOD (AHiddenPaneLeavesTheMachineUnrecordedAndTheSnapshotEmpty)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot = rig.view.Build (rig.controller.GetSession());



            Assert::IsTrue (snapshot.heatMap.execute.empty());
            Assert::IsFalse (rig.machine.GetMemoryBus().AreAllPagesWatched());
        }



        TEST_METHOD (AShownPaneRecordsAndCarriesALevelPerAddress)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetHeatMapShown (true);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue    (rig.machine.GetMemoryBus().AreAllPagesWatched());
            Assert::AreEqual  (AccessHeatMap::kAddressCount, snapshot.heatMap.execute.size());
            Assert::AreEqual  (AccessHeatMap::kAddressCount, snapshot.heatMap.read.size());
            Assert::AreEqual  (AccessHeatMap::kAddressCount, snapshot.heatMap.write.size());
        }



        TEST_METHOD (HidingThePaneAgainStopsTheRecording)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetHeatMapShown (true);
            snapshot = rig.view.Build (rig.controller.GetSession());
            rig.view.SetHeatMapShown (false);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue (snapshot.heatMap.execute.empty());
            Assert::IsFalse (rig.machine.GetMemoryBus().AreAllPagesWatched());
        }



        TEST_METHOD (TheViewMessageReadsOnAndOff)
        {
            bool  shown = false;



            Assert::IsTrue  (CpuCommandDispatcher::TryGetHeatMapShown ("on",  shown));
            Assert::IsTrue  (shown);
            Assert::IsTrue  (CpuCommandDispatcher::TryGetHeatMapShown ("off", shown));
            Assert::IsFalse (shown);
            Assert::IsFalse (CpuCommandDispatcher::TryGetHeatMapShown ("",    shown));
            Assert::IsFalse (CpuCommandDispatcher::TryGetHeatMapShown ("ON1", shown));
        }
    };
}
