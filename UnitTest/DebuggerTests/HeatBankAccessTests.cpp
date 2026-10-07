#include "Pch.h"

#include "Debugger/HeatAccessJump.h"
#include "Debugger/HeatMapOptions.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Ui/Debugger/Panes/HeatMapView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatBankAccessTests
//
//  The heat map's banks as the options keep them and the tip gives them,
//  and its last accesses as a click asks for them: the keys a click takes,
//  the words the window sends the machine, and what the machine does with
//  them.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatBankAccessTests)
    {
    public:

        using Bank   = HeatMapOptions::Bank;
        using Action = HeatMapView::PickAction;

        static HeatLastAccess MakeAccess (Word pc, uint64_t position, uint64_t cycle)
        {
            return HeatLastAccess::Make (pc, position, cycle);
        }

        static HeatAccessRequest MakeRequest (bool isRewind, bool isWrite, Bank bank, Word address)
        {
            HeatAccessRequest  request;



            request.isRewind = isRewind;
            request.isWrite  = isWrite;
            request.bank     = bank;
            request.address  = address;
            return request;
        }

        static DxuiMouseEvent MakeClick (DxuiMouseEventKind kind, POINT point, bool ctrl, bool shift, bool alt)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = point;
            ev.ctrl        = ctrl;
            ev.shift       = shift;
            ev.alt         = alt;
            return ev;
        }



        TEST_METHOD (TheBankChosenSurvivesTheRoundTripAndTheCpusIsNotWritten)
        {
            HeatMapOptions  options;



            Assert::AreEqual (std::string ("fade=10 view=all"), options.ToText(), L"the CPU's bank adds nothing to the text");

            for (Bank bank : { Bank::Cpu, Bank::Main, Bank::Aux, Bank::LanguageCard, Bank::AuxLanguageCard, Bank::Rom })
            {
                options.bank = bank;
                Assert::IsTrue (HeatMapOptions::FromText (options.ToText()) == options, HeatMapOptions::GetBankLabel (bank).c_str());
            }

            Assert::AreEqual ((int) Bank::Aux, (int) HeatMapOptions::FromText ("fade=5 bank=aux").bank);
            Assert::AreEqual ((int) Bank::Cpu, (int) HeatMapOptions::FromText ("bank=sideways").bank, L"a bank it does not know is passed over");
        }



        TEST_METHOD (EachBankCountsInItsSpaceAndACardViewShowsItsSixteenKilobytesAlone)
        {
            Assert::AreEqual ((int) HeatSpace::Cpu,  (int) HeatMapOptions::GetSpace (Bank::Cpu));
            Assert::AreEqual ((int) HeatSpace::Main, (int) HeatMapOptions::GetSpace (Bank::Main));
            Assert::AreEqual ((int) HeatSpace::Main, (int) HeatMapOptions::GetSpace (Bank::LanguageCard));
            Assert::AreEqual ((int) HeatSpace::Aux,  (int) HeatMapOptions::GetSpace (Bank::AuxLanguageCard));
            Assert::AreEqual ((int) HeatSpace::Rom,  (int) HeatMapOptions::GetSpace (Bank::Rom));

            Assert::IsTrue  (HeatMapOptions::IsShown (Bank::Main,         0x2000));
            Assert::IsFalse (HeatMapOptions::IsShown (Bank::LanguageCard, 0xBFFF));
            Assert::IsTrue  (HeatMapOptions::IsShown (Bank::LanguageCard, 0xC000));
        }



        TEST_METHOD (TheTipSaysWhereInTheBankACellIs)
        {
            Assert::AreEqual (std::wstring (L"$2000"),                           HeatMapOptions::DescribeLocation (Bank::Cpu,             0x2000, true));
            Assert::AreEqual (std::wstring (L"Main RAM $2000"),                  HeatMapOptions::DescribeLocation (Bank::Main,            0x2000, true));
            Assert::AreEqual (std::wstring (L"Aux RAM $0400"),                   HeatMapOptions::DescribeLocation (Bank::Aux,             0x0400, true));
            Assert::AreEqual (std::wstring (L"Main language card bank 1 $D123"), HeatMapOptions::DescribeLocation (Bank::Main,            0xC123, true));
            Assert::AreEqual (std::wstring (L"Aux language card bank 2 $D123"),  HeatMapOptions::DescribeLocation (Bank::AuxLanguageCard, 0xD123, true));
            Assert::AreEqual (std::wstring (L"Main high RAM $F000"),             HeatMapOptions::DescribeLocation (Bank::LanguageCard,    0xF000, true));
            Assert::AreEqual (std::wstring (L"Language card bank 1 $D000"),      HeatMapOptions::DescribeLocation (Bank::LanguageCard,    0xC000, false));
            Assert::AreEqual (std::wstring (L"High RAM $E000"),                  HeatMapOptions::DescribeLocation (Bank::Main,            0xE000, false));
            Assert::AreEqual (std::wstring (L"ROM $FFFC"),                       HeatMapOptions::DescribeLocation (Bank::Rom,             0xFFFC, true));
        }



        TEST_METHOD (CtrlShiftAndAltChooseWhatAClickAsksFor)
        {
            Assert::AreEqual ((int) Action::ShowMemory,    (int) HeatMapView::GetPickAction (false, false, false));
            Assert::AreEqual ((int) Action::ShowMemory,    (int) HeatMapView::GetPickAction (false, true,  true),  L"Shift and Alt alone change nothing");
            Assert::AreEqual ((int) Action::ShowWriter,    (int) HeatMapView::GetPickAction (true,  false, false));
            Assert::AreEqual ((int) Action::ShowReader,    (int) HeatMapView::GetPickAction (true,  true,  false));
            Assert::AreEqual ((int) Action::RewindToWrite, (int) HeatMapView::GetPickAction (true,  false, true));
            Assert::AreEqual ((int) Action::RewindToRead,  (int) HeatMapView::GetPickAction (true,  true,  true));
        }



        TEST_METHOD (AClickWithCtrlOnACellAsksForItsAccessAndAPlainOneShowsMemory)
        {
            HeatMapView                              view;
            DxuiDpiScaler                            scaler;
            std::vector<Word>                        shown;
            std::vector<std::pair<Word, Action>>     asked;
            RECT                                     cell   = {};
            POINT                                    at     = {};



            view.Layout (RECT { 0, 0, 600, 400 }, scaler);
            view.SetOnPickAddress ([&shown] (Word address) { shown.push_back (address); });
            view.SetOnPickAccess  ([&asked] (Word address, Action action) { asked.emplace_back (address, action); });

            cell = view.GetCellRect (0x0123);
            at   = { cell.left, cell.top };

            for (int keys = 0; keys < 3; keys++)
            {
                bool  ctrl = keys > 0;
                bool  alt  = keys == 2;



                (void) view.OnMouse (MakeClick (DxuiMouseEventKind::Down, at, ctrl, false, alt));
                (void) view.OnMouse (MakeClick (DxuiMouseEventKind::Up,   at, ctrl, false, alt));
            }

            Assert::AreEqual ((size_t) 1, shown.size(), L"the plain click showed memory");
            Assert::AreEqual ((Word) 0x0123, shown[0]);
            Assert::AreEqual ((size_t) 2, asked.size(), L"the Ctrl clicks asked for the access");
            Assert::IsTrue   (asked[0] == std::pair<Word, Action> { 0x0123, Action::ShowWriter });
            Assert::IsTrue   (asked[1] == std::pair<Word, Action> { 0x0123, Action::RewindToWrite });
        }



        TEST_METHOD (TheTipGivesTheLastWriterAndReaderOnceTheMachineHasThem)
        {
            HeatMapView      view;
            HeatAccessHover  hover;



            view.SetShownBank (Bank::Aux, true);
            Assert::AreEqual (std::wstring (L"Aux RAM $2000  untouched"), view.GetTipText (0x2000), L"before the machine looks them up");

            hover.address            = 0x2000;
            hover.bank               = Bank::Aux;
            hover.writer.has         = true;
            hover.writer.pc          = 0x6A12;
            hover.writer.cycle       = 1234567;
            hover.writer.instruction = "STA ($06),Y";
            view.SetHoverAccess (hover);

            Assert::AreEqual (std::wstring (L"Aux RAM $2000  untouched\nLast written by $6A12 STA ($06),Y at cycle 1,234,567\nNot read since counting started"),
                              view.GetTipText (0x2000));
            Assert::AreEqual (std::wstring (L"Aux RAM $2001  untouched"), view.GetTipText (0x2001), L"another cell's are not this one's");

            hover.writer.label = "DRAWROW";
            view.SetHoverAccess (hover);
            Assert::IsTrue   (view.GetTipText (0x2000).find (L"by $6A12 DRAWROW: STA") != std::wstring::npos, L"a symbol at the PC is given");

            view.SetShownBank (Bank::Main, true);
            Assert::AreEqual (std::wstring (L"Main RAM $2000  untouched"), view.GetTipText (0x2000), L"another bank's are not this one's");
        }



        TEST_METHOD (ARequestGoesToTheMachineAsWordsAndBack)
        {
            HeatAccessRequest  request = MakeRequest (true, false, Bank::AuxLanguageCard, 0xC0DE);
            HeatAccessRequest  parsed;



            Assert::AreEqual (std::string ("rewind read auxlc C0DE"), HeatAccessJump::FormatWords (request));
            Assert::IsTrue   (HeatAccessJump::TryParseWords (HeatAccessJump::FormatWords (request), parsed));
            Assert::IsTrue   (parsed == request);
            Assert::IsTrue   (HeatAccessJump::TryParseWords ("code write cpu 2000", parsed));
            Assert::IsTrue   (parsed == MakeRequest (false, true, Bank::Cpu, 0x2000));

            for (const char * bad : { "", "code", "code write cpu", "jump write cpu 2000", "code wrote cpu 2000", "code write ram 2000",
                                      "code write cpu 12345", "code write cpu 20G0", "code write cpu 2000 more" })
            {
                Assert::IsFalse (HeatAccessJump::TryParseWords (bad, parsed), std::wstring (bad, bad + strlen (bad)).c_str());
            }
        }



        TEST_METHOD (TheHoverWordsGiveTheCellOrNone)
        {
            std::optional<Word>  address = Word (1);



            Assert::IsTrue   (CpuCommandDispatcher::TryGetHeatMapHover ("hover 2a0f", address));
            Assert::AreEqual ((Word) 0x2A0F, address.value_or (0));
            Assert::IsTrue   (CpuCommandDispatcher::TryGetHeatMapHover ("hover none", address));
            Assert::IsFalse  (address.has_value());
            Assert::IsFalse  (CpuCommandDispatcher::TryGetHeatMapHover ("hover", address));
            Assert::IsFalse  (CpuCommandDispatcher::TryGetHeatMapHover ("hover 12345", address));
            Assert::IsFalse  (CpuCommandDispatcher::TryGetHeatMapHover ("reset", address));
        }



        TEST_METHOD (APlanShowsTheCodeOrSeeksJustAfterTheAccessOrSaysWhyNot)
        {
            HeatAccessPlan                  plan;
            HeatLastAccess                  access = MakeAccess (0x6A12, 5000, 90000);



            plan = HeatAccessJump::Plan (MakeRequest (false, true, Bank::Cpu, 0x2000), HeatAccessState::Found, access, true, 0);
            Assert::AreEqual ((int) HeatAccessPlan::Kind::ShowCode, (int) plan.kind);
            Assert::AreEqual ((Word) 0x6A12, plan.pc);

            plan = HeatAccessJump::Plan (MakeRequest (true, true, Bank::Cpu, 0x2000), HeatAccessState::Found, access, true, 1000);
            Assert::AreEqual ((int) HeatAccessPlan::Kind::Seek, (int) plan.kind);
            Assert::AreEqual ((uint64_t) 5001, plan.position, L"just after the instruction that wrote it");

            plan = HeatAccessJump::Plan (MakeRequest (true, true, Bank::Main, 0x2000), HeatAccessState::Found, access, true, 6000);
            Assert::AreEqual ((int) HeatAccessPlan::Kind::Report, (int) plan.kind);
            Assert::AreEqual (std::string ("The last write of Main RAM $2000, at cycle 90,000, is older than the history kept."), plan.message);

            plan = HeatAccessJump::Plan (MakeRequest (true, false, Bank::Cpu, 0x2000), HeatAccessState::Found, access, false, 0);
            Assert::AreEqual ((int) HeatAccessPlan::Kind::Report, (int) plan.kind);
            Assert::AreEqual (std::string ("The last read of $2000 was at cycle 90,000, and history is not being recorded."), plan.message);

            plan = HeatAccessJump::Plan (MakeRequest (false, false, Bank::Rom, 0xD000), HeatAccessState::None, HeatLastAccess(), true, 0);
            Assert::AreEqual ((int) HeatAccessPlan::Kind::Report, (int) plan.kind);
            Assert::AreEqual (std::string ("No read of ROM $D000 since the heat map started counting."), plan.message);

            plan = HeatAccessJump::Plan (MakeRequest (false, true, Bank::Aux, 0x0400), HeatAccessState::Unknown, HeatLastAccess(), true, 0);
            Assert::AreEqual ((int) HeatAccessPlan::Kind::Report, (int) plan.kind);
            Assert::AreEqual (std::string ("The last write of Aux RAM $0400 is older than the history kept."), plan.message);
        }
    };
}
