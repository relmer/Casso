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



        TEST_METHOD (ARunningMachinesMapIsReadNowAndThenAndAStoppedOnesEveryBuild)
        {
            std::unique_ptr<ControllerRig>  rig      = std::make_unique<ControllerRig>();
            DebuggerViewSnapshot            snapshot;
            uint64_t                        first    = 0;
            uint64_t                        running  = 0;
            uint64_t                        asked    = 0;



            rig->view.SetClock ([] { return (uint64_t) 1000; });
            rig->view.SetHeatMapShown (true);
            snapshot = rig->view.Build (rig->controller.GetSession(), false);
            first    = snapshot.heatMap.serial;

            //  LDA #$41 at $0300: counted, but a running machine's snapshot
            //  within the interval holds the reading before it.
            rig->machine.StepOne();
            snapshot = rig->view.Build (rig->controller.GetSession(), false);
            running  = snapshot.heatMap.serial;

            Assert::AreNotEqual ((uint64_t) 0, first);
            Assert::AreEqual    (first, running, L"the same reading");
            Assert::AreEqual    ((Byte) 0, snapshot.heatMap.execute[0x0300]);

            rig->view.MarkHeatMapReadDue();
            snapshot = rig->view.Build (rig->controller.GetSession(), false);
            asked    = snapshot.heatMap.serial;

            Assert::AreNotEqual (running, asked, L"read again when asked");
            Assert::IsTrue      (snapshot.heatMap.execute[0x0300] > 0);

            snapshot = rig->view.Build (rig->controller.GetSession(), true);
            Assert::AreNotEqual (asked, snapshot.heatMap.serial, L"a stopped machine's at every build");
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



        TEST_METHOD (CumulativeOptionsCarryTheTotalsAndTheFadeReachesTheMachine)
        {
            ControllerRig           rig;
            DebuggerViewSnapshot    snapshot;
            HeatMapOptions          options;
            AccessHeatMap           thirty;
            const AccessHeatMap   * map    = nullptr;
            Byte                    fading = 0;



            rig.view.SetHeatMapShown (true);
            snapshot = rig.view.Build (rig.controller.GetSession());

            //  LDA #$41 at $0300, run once: two bytes executed, once each.
            rig.machine.StepOne();
            Assert::AreEqual ((Word) 0x0302, rig.controller.GetSession().GetTarget().GetRegisters().pc);

            snapshot = rig.view.Build (rig.controller.GetSession());
            fading   = snapshot.heatMap.execute[0x0300];

            options.cumulative  = true;
            options.fadeSeconds = 30;
            rig.view.SetHeatMapOptions (options);
            snapshot = rig.view.Build (rig.controller.GetSession());
            map      = rig.controller.GetSession().GetTarget().FoldHeatMap();
            thirty.SetFadeSeconds (30.0);

            Assert::IsTrue   (fading > 0 && fading < 255, L"fading, one access is faint");
            Assert::AreEqual ((Byte) 255, snapshot.heatMap.execute[0x0300], L"cumulative, the busiest address is the top");
            Assert::IsNotNull (map);
            Assert::AreEqual (thirty.GetFadePerFrame(), map->GetFadePerFrame(), L"the fade time reached the machine's map");
        }



        TEST_METHOD (TheSnapshotCarriesTheBankChosenAndTheBanksTheMachineHas)
        {
            ControllerRig           rig;
            DebuggerViewSnapshot    snapshot;
            HeatMapOptions          options;
            const AccessHeatMap   * map    = nullptr;



            rig.view.SetHeatMapShown (true);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) HeatMapOptions::kBankCount, snapshot.heatMap.banks.size(), L"a //e has every bank");
            Assert::IsTrue   (snapshot.heatMap.hasAux);
            Assert::AreEqual ((int) HeatMapOptions::Bank::Cpu, (int) snapshot.heatMap.bank);

            //  LDA #$41 at $0300: executed from main RAM.
            rig.machine.StepOne();

            options.cumulative = true;
            options.bank       = HeatMapOptions::Bank::Main;
            rig.view.SetHeatMapOptions (options);
            snapshot = rig.view.Build (rig.controller.GetSession());
            map      = rig.controller.GetSession().GetTarget().FoldHeatMap();

            Assert::AreEqual ((int) HeatMapOptions::Bank::Main, (int) snapshot.heatMap.bank);
            Assert::IsNotNull (map);
            Assert::AreEqual ((double) map->GetMostTotal (HeatSpace::Main), snapshot.heatMap.top, L"the top is the bank's busiest");
            Assert::AreEqual ((Byte) 255, snapshot.heatMap.execute[0x0300], L"the main RAM's levels");

            options.bank = HeatMapOptions::Bank::LanguageCard;
            rig.view.SetHeatMapOptions (options);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((Byte) 0, snapshot.heatMap.execute[0x0300], L"the language card's view shows its 16 KB alone");
        }



        TEST_METHOD (WithTheMouseOverACellTheSnapshotCarriesItsLastWriterAndReader)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetHeatMapShown (true);
            snapshot = rig.view.Build (rig.controller.GetSession());

            //  LDA #$41, then STA $0400.
            rig.machine.StepOne();
            rig.machine.StepOne();

            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::IsNull (snapshot.heatMap.hover.get(), L"no cell, nothing looked up");

            rig.view.SetHeatMapHover (Word (0x0400));
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsNotNull (snapshot.heatMap.hover.get());
            Assert::AreEqual ((Word) 0x0400, snapshot.heatMap.hover->address);
            Assert::IsTrue   (snapshot.heatMap.hover->writer.has, L"STA $0400 wrote it");
            Assert::AreEqual ((Word) 0x0302, snapshot.heatMap.hover->writer.pc);
            Assert::AreEqual (std::string ("STA $0400"), snapshot.heatMap.hover->writer.instruction);
            Assert::IsFalse  (snapshot.heatMap.hover->reader.has, L"nothing read it");
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
