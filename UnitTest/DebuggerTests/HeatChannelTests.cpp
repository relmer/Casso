#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/AccessHeatMap.h"
#include "Debugger/HeatMapRangeSets.h"
#include "Debugger/HeatMapSymbols.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Ui/Debugger/Panes/HeatMapView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatChannelTests
//
//  How the heat map shows its reads before written and its writes that
//  changed their byte: the snapshot that carries them, the options that
//  choose them, the colors the pane draws them in, the tip, the bar's
//  entries, the colors' legend, and the spans of a set of ranges left out
//  as the window sends them to the machine.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatChannelTests)
    {
    public:

        using Mode = HeatMapView::Mode;

        static constexpr uint32_t  kCold      = 0xFF303030;
        static constexpr uint32_t  kExecute   = 0xFF0000FF;
        static constexpr uint32_t  kRead      = 0xFF00FF00;
        static constexpr uint32_t  kWrite     = 0xFFFF0000;
        static constexpr uint32_t  kUnwritten = 0xFFFF00FF;
        static constexpr uint32_t  kChanged   = 0xFFFFA000;

        static HeatMapView::Palette MakePalette()
        {
            HeatMapView::Palette  palette;



            palette.background = 0xFF000000;
            palette.cold       = kCold;
            palette.execute    = kExecute;
            palette.read       = kRead;
            palette.write      = kWrite;
            palette.unwritten  = kUnwritten;
            palette.changed    = kChanged;
            return palette;
        }

        //  $0300: LDA $2000 / LDA #$41 / STA $2001 / STA $2001 / JMP $030B,
        //  with $2001 holding $FF first; stepped to the spin.
        static void RunReadThenStores (ControllerRig & rig)
        {
            static constexpr Byte  kProgram[] = { 0xAD, 0x00, 0x20, 0xA9, 0x41, 0x8D, 0x01, 0x20, 0x8D, 0x01, 0x20, 0x4C, 0x0B, 0x03 };
            constexpr int          kSteps     = 6;
            IDebugTarget         & target     = rig.controller.GetSession().GetTarget();
            Cpu6502Registers       registers  = target.GetRegisters();
            Word                   at         = 0x0300;



            for (Byte b : kProgram)
            {
                target.TryPoke (at++, b);
            }

            target.TryPoke (0x2001, 0xFF);

            registers.pc = 0x0300;
            target.SetRegisters (registers);

            for (int i = 0; i < kSteps; i++)
            {
                rig.machine.StepOne();
            }
        }



        //  The changes show alone in Changed, in their own color; elsewhere
        //  they leave the color as it is.
        TEST_METHOD (ChangedShowsTheChangesAloneInTheirOwnColor)
        {
            HeatMapView::Palette  palette = MakePalette();



            Assert::AreEqual (kChanged, HeatMapView::GetChannelColor (Mode::Changed, 0, 255, kWrite, palette));
            Assert::AreEqual (kCold,    HeatMapView::GetChannelColor (Mode::Changed, 0, 0,   kWrite, palette), L"a write that changed nothing is cold there");
            Assert::AreEqual (kWrite,   HeatMapView::GetChannelColor (Mode::Data,    0, 255, kWrite, palette));
            Assert::AreEqual (std::wstring (L"Changed"), HeatMapView::GetModeLabel (Mode::Changed));
        }



        //  A read before written shows over everything else in All and Data,
        //  and not in Code, which shows executes alone.
        TEST_METHOD (AReadBeforeWrittenShowsOverTheRestInAllAndData)
        {
            HeatMapView::Palette  palette = MakePalette();



            Assert::AreEqual (kUnwritten, HeatMapView::GetChannelColor (Mode::All,  255, 0, kExecute, palette));
            Assert::AreEqual (kUnwritten, HeatMapView::GetChannelColor (Mode::Data, 255, 0, kRead,    palette));
            Assert::AreEqual (kExecute,   HeatMapView::GetChannelColor (Mode::Code, 255, 0, kExecute, palette));
            Assert::AreEqual (kRead,      HeatMapView::GetChannelColor (Mode::All,  0,   0, kRead,    palette), L"none, the color stands");
        }



        //  The cell drawn takes the channels, and the tip names what touched
        //  it among them.
        TEST_METHOD (TheCellsAndTheTipShowTheChannels)
        {
            HeatMapView        view;
            DxuiDpiScaler      scaler;
            HeatMapOptions     options;
            std::vector<Byte>  read      (0x10000, 0);
            std::vector<Byte>  write     (0x10000, 0);
            std::vector<Byte>  unwritten (0x10000, 0);
            std::vector<Byte>  changed   (0x10000, 0);
            std::vector<Byte>  edited    (0x10000, 0);
            std::vector<Byte>  none;



            view.SetPalette (MakePalette());
            view.Layout (RECT { 0, 0, 600, 400 }, scaler);
            view.SetTop (50000.0);

            read[0x2000]      = 255;
            unwritten[0x2000] = 255;
            write[0x2001]     = 255;
            changed[0x2001]   = 255;

            view.SetChannelLevels (unwritten, changed);
            view.SetLevels        (none, read, write);

            Assert::AreEqual (kUnwritten, view.GetCellColor (0x2000), L"the read before written shows over the read");
            Assert::AreEqual (kWrite,     view.GetCellColor (0x2001));
            Assert::AreEqual (std::wstring (L"$2000  read 50,000+/s, read before written 50,000+/s"), view.GetTipText (0x2000));
            Assert::AreEqual (std::wstring (L"$2001  written 50,000+/s, changed 50,000+/s"),          view.GetTipText (0x2001));

            edited[0x2001] = 1;
            view.SetChannelLevels (unwritten, changed, edited);
            view.SetLevels        (none, read, write);
            Assert::AreEqual (std::wstring (L"$2001  written 50,000+/s, changed 50,000+/s\nEdited in the debugger"), view.GetTipText (0x2001));

            options.view = Mode::Changed;
            view.SetOptions (options);

            Assert::AreEqual (kCold,    view.GetCellColor (0x2000));
            Assert::AreEqual (kChanged, view.GetCellColor (0x2001));
        }



        //  The snapshot carries both channels at every level; with writes
        //  that change nothing left out, the writes are the changes.
        TEST_METHOD (TheSnapshotCarriesTheChannelsAndCanLeaveOutWritesThatChangeNothing)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;
            HeatMapOptions        options;



            rig.view.SetHeatMapShown (true);
            snapshot = rig.view.Build (rig.controller.GetSession());
            RunReadThenStores (rig);

            options.cumulative = true;
            rig.view.SetHeatMapOptions (options);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual (AccessHeatMap::kAddressCount, snapshot.heatMap.unwritten.size());
            Assert::AreEqual (AccessHeatMap::kAddressCount, snapshot.heatMap.changed.size());
            Assert::IsTrue   (snapshot.heatMap.unwritten[0x2000] > 0, L"nothing wrote $2000 before it was read");
            Assert::AreEqual ((Byte) 0, snapshot.heatMap.unwritten[0x2001]);
            Assert::IsTrue   (snapshot.heatMap.changed[0x2001] > 0);
            Assert::IsTrue   (snapshot.heatMap.changed[0x2001] < snapshot.heatMap.write[0x2001], L"one change of two writes");
            Assert::AreEqual ((uint64_t) 1, rig.controller.GetSession().GetTarget().GetUnwrittenStatus().reads);

            options.ignoreSameWrites = true;
            rig.view.SetHeatMapOptions (options);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue (snapshot.heatMap.write == snapshot.heatMap.changed, L"the writes shown are the changes");
        }



        //  The spans left out are not carried, and the map stays on for the
        //  break with the pane hidden, carrying no levels.
        TEST_METHOD (TheSpansLeftOutAreNotCarriedAndTheBreakKeepsTheMapOn)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetHeatMapShown (true);
            snapshot = rig.view.Build (rig.controller.GetSession());
            rig.controller.GetSession().GetTarget().SetUnwrittenIgnore ({ { 0x2000, 0x20FF } });
            RunReadThenStores (rig);

            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::AreEqual ((Byte) 0, snapshot.heatMap.unwritten[0x2000], L"in a span left out");
            Assert::AreEqual ((uint64_t) 0, rig.controller.GetSession().GetTarget().GetUnwrittenStatus().reads);

            rig.controller.GetSession().SetUnwrittenBreak (true);
            rig.view.SetHeatMapShown (false);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue (rig.machine.GetMemoryBus().AreAllPagesWatched(), L"the break keeps the map recording");
            Assert::IsTrue (snapshot.heatMap.unwritten.empty(), L"with the pane hidden, no levels");

            rig.controller.GetSession().SetUnwrittenBreak (false);
            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::IsFalse (rig.machine.GetMemoryBus().AreAllPagesWatched());
        }



        //  The options keep the Changed view, writes that change nothing left
        //  out, and the set left out, whose name runs to the end of the text.
        TEST_METHOD (TheOptionsKeepTheNewChoices)
        {
            HeatMapOptions  options;
            HeatMapOptions  back;
            std::string     text;



            options.view             = Mode::Changed;
            options.ignoreSameWrites = true;
            options.ignoreSet        = "Boot probes";

            text = options.ToText();
            back = HeatMapOptions::FromText (text);

            Assert::IsTrue (back == options, std::wstring (text.begin(), text.end()).c_str());
            Assert::IsTrue (HeatMapOptions::FromText ("fade=10 view=all").ignoreSet.empty());
            Assert::IsFalse (HeatMapOptions::FromText ("fade=10 view=all").ignoreSameWrites);
        }



        //  A cell of a bank's view stands for the CPU address its range is
        //  given in: bank 1 of the language card at $D000.
        TEST_METHOD (ACellOfABankStandsForItsCpuAddress)
        {
            using Bank = HeatMapOptions::Bank;



            Assert::AreEqual ((Word) 0xD010, HeatMapOptions::GetCpuAddress (Bank::Main,         0xC010));
            Assert::AreEqual ((Word) 0xD010, HeatMapOptions::GetCpuAddress (Bank::AuxLanguageCard, 0xC010));
            Assert::AreEqual ((Word) 0xD010, HeatMapOptions::GetCpuAddress (Bank::Main,         0xD010), L"bank 2 is where it is");
            Assert::AreEqual ((Word) 0x2000, HeatMapOptions::GetCpuAddress (Bank::Aux,          0x2000));
            Assert::AreEqual ((Word) 0xC010, HeatMapOptions::GetCpuAddress (Bank::Cpu,          0xC010), L"the CPU's own");
        }



        //  The set left out goes to the machine as spans of hex, and back,
        //  through the dispatcher's words.
        TEST_METHOD (TheSpansLeftOutGoToTheMachineAsWords)
        {
            HeatMapRangeSets                     sets;
            HeatMapRangeSet                      set     = HeatMapRangeSets::MakeSet ("Boot probes");
            std::shared_ptr<HeatMapSymbols>      symbols = std::make_shared<HeatMapSymbols>();
            std::vector<std::pair<Word, Word>>   spans;
            std::vector<std::pair<Word, Word>>   parsed;
            std::string                          words;



            for (HeatMapRange & range : set.ranges)
            {
                range.included = range.preset == "zeropage";
            }

            set.ranges.push_back (HeatMapRange { "Power-up byte", "$3F2", "$3F4", {}, true });
            sets.sets.push_back (set);

            for (const HeatMapRangeSets::Span & span : sets.GetSpans ("Boot probes", *symbols))
            {
                spans.emplace_back (span.first, span.last);
            }

            words = HeatMapRangeSets::FormatSpanWords (spans);

            Assert::AreEqual (std::string ("0000-00FF 03F2-03F4"), words);
            Assert::IsTrue   (CpuCommandDispatcher::TryGetHeatMapIgnore ("ignore " + words, parsed));
            Assert::IsTrue   (parsed == spans);

            Assert::AreEqual (std::string ("none"), HeatMapRangeSets::FormatSpanWords ({}));
            Assert::IsTrue   (CpuCommandDispatcher::TryGetHeatMapIgnore ("ignore none", parsed));
            Assert::IsTrue   (parsed.empty());
            Assert::IsFalse  (CpuCommandDispatcher::TryGetHeatMapIgnore ("ignore 0400-03FF", parsed), L"first after last");
            Assert::IsFalse  (CpuCommandDispatcher::TryGetHeatMapIgnore ("ignore $0400-$0500", parsed));
            Assert::IsTrue   (sets.GetSpans ("No such set", *symbols).empty());
        }



        //  The bar offers both choices in sentence case, and the legend lists
        //  both colors under the heat map.
        TEST_METHOD (TheBarAndTheLegendOfferTheChannels)
        {
            HeatMapBarCommands  commands ({});
            int                 listed   = 0;



            Assert::AreEqual (std::wstring (L"Ignore writes that don't change the value"), commands.Find (HeatMapBarCommands::kIgnoreSame)->label);
            Assert::IsNotNull (commands.Find (HeatMapBarCommands::kIgnoreSet).get());
            Assert::AreEqual (std::wstring (L"Leave out: None"),        HeatMapBarCommands::GetIgnoreSetLabel (L""));
            Assert::AreEqual (std::wstring (L"Leave out: Boot probes"), HeatMapBarCommands::GetIgnoreSetLabel (L"Boot probes"));

            for (const ColorLegend::Entry & entry : ColorLegend::GetEntries())
            {
                if (entry.meaning == ColorLegend::Meaning::HeatUnwritten || entry.meaning == ColorLegend::Meaning::HeatChanged)
                {
                    Assert::AreEqual (std::wstring (L"Heat map"), std::wstring (entry.group));
                    listed++;
                }
            }

            Assert::AreEqual (2, listed);
            Assert::IsTrue   (std::wstring (ColorLegend::GetText (ColorLegend::Meaning::HeatUnwritten)).starts_with (L"Read before written"));
            Assert::IsTrue   (std::wstring (ColorLegend::GetText (ColorLegend::Meaning::HeatChanged)).starts_with (L"Value changed"));
        }
    };
}
