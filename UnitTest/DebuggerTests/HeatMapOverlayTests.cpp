#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"
#include "ControllerRig.h"
#include "Core/Cpu65C02Table.h"
#include "Debugger/AccessHeatMap.h"
#include "Debugger/HeatMapSymbols.h"
#include "Debugger/Reply.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/Panes/HeatMapView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOverlayTests
//
//  What the heat map draws over and inside its cells: a cell's value and its
//  opcode's form or bits once it is large enough, an operand byte dimmer
//  than its opcode, outlines for the PC, the stack pointer and breakpoints,
//  a tint for a symbol, and Blend's mixed colors with self-modifying code
//  set apart; and the snapshot's side of each.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatMapOverlayTests)
    {
    public:

        static constexpr uint32_t  kBackground    = 0xFF000000;
        static constexpr uint32_t  kCold          = 0xFF303030;
        static constexpr uint32_t  kExecute       = 0xFFE0201C;   // the heat column's hottest run color, HeatStrip::GetScaleStops
        static constexpr uint32_t  kRead          = 0xFF9AEEFF;
        static constexpr uint32_t  kWrite         = 0xFFFFA6E8;
        static constexpr uint32_t  kPc            = 0xFFFFFF00;
        static constexpr uint32_t  kStack         = 0xFF00FFFF;
        static constexpr uint32_t  kBreakpoint    = 0xFFF04040;
        static constexpr uint32_t  kReadWatch     = 0xFF80FF80;
        static constexpr uint32_t  kWriteWatch    = 0xFFFF8000;
        static constexpr uint32_t  kSymbol        = 0xFF40C0C0;
        static constexpr uint32_t  kSelfModifying = 0xFFE000E0;
        static constexpr size_t    kCount         = (size_t) HeatMapView::kAddressCount;

        using Mode      = HeatMapView::Mode;
        using BreakKind = HeatMapView::BreakKind;

        static HeatMapView::Palette MakePalette()
        {
            HeatMapView::Palette  palette;



            palette.background    = kBackground;
            palette.cold          = kCold;
            palette.execute       = kExecute;
            palette.read          = kRead;
            palette.write         = kWrite;
            palette.pc            = kPc;
            palette.stack         = kStack;
            palette.breakpoint    = kBreakpoint;
            palette.readWatch     = kReadWatch;
            palette.writeWatch    = kWriteWatch;
            palette.symbol        = kSymbol;
            palette.selfModifying = kSelfModifying;
            return palette;
        }

        static void Place (HeatMapView & view)
        {
            DxuiDpiScaler  scaler;



            view.SetPalette (MakePalette());
            view.Layout (RECT { 0, 0, 600, 400 }, scaler);
        }

        static void Paint (HeatMapView & view, MockDxuiTextRenderer & text)
        {
            MockDxuiPainter  painter;
            MockDxuiTheme    theme;



            text.Reset();
            view.Paint (painter, text, theme);
        }

        static bool HasText (const MockDxuiTextRenderer & text, const std::wstring & wanted)
        {
            return std::ranges::any_of (text.Calls(), [&wanted] (const RecordedTextCall & call)
            {
                return call.kind == RecordedTextKind::DrawString && call.text == wanted;
            });
        }

        static bool Contains (const std::wstring & text, const std::wstring & part)
        {
            return text.find (part) != std::wstring::npos;
        }



        TEST_METHOD (DetailShowsOnlyWhereACellHoldsLegibleText)
        {
            HeatMapView::Detail  tiny   = HeatMapView::GetDetail (14, 14, 13.0f, 9.0f, 2.0f);
            HeatMapView::Detail  one    = HeatMapView::GetDetail (20, 20, 13.0f, 9.0f, 2.0f);
            HeatMapView::Detail  narrow = HeatMapView::GetDetail (30, 64, 13.0f, 9.0f, 2.0f);
            HeatMapView::Detail  two    = HeatMapView::GetDetail (64, 64, 13.0f, 9.0f, 2.0f);



            Assert::AreEqual (0, tiny.lines,   L"too small for 9-pixel text");
            Assert::AreEqual (1, one.lines,    L"room for the value");
            Assert::IsTrue   (one.fontPx >= 9.0f && one.fontPx <= 13.0f);
            Assert::AreEqual (1, narrow.lines, L"tall enough for two lines, too narrow for the second");
            Assert::AreEqual (2, two.lines);
            Assert::AreEqual (10.0f, two.fontPx, L"ten characters across 60 pixels");
        }



        TEST_METHOD (TheSecondLineIsTheOpcodesFormOrTheBits)
        {
            Assert::AreEqual (std::wstring (L"LDA (..),Y"), HeatMapView::GetDetailLine (0xB1, true,  "LDA (..),Y"));
            Assert::AreEqual (std::wstring (L"01011010"),   HeatMapView::GetDetailLine (0x5A, false, "LSR ....,X"), L"data, its bits");
            Assert::AreEqual (std::wstring (L"00000010"),   HeatMapView::GetDetailLine (0x02, true,  ""),           L"no form known");
        }



        TEST_METHOD (DetailTextTakesWhicheverThemeColorStandsOutFromTheCell)
        {
            constexpr uint32_t  kText = 0xFFF0F0F0;
            constexpr uint32_t  kPage = 0xFF101010;



            Assert::AreEqual (kPage, HeatMapView::GetDetailTextColor (0xFFFFE000, kText, kPage), L"on a bright cell, the page's color");
            Assert::AreEqual (kText, HeatMapView::GetDetailTextColor (0xFF303030, kText, kPage), L"on a dim cell, the text's");
        }



        TEST_METHOD (OpcodeFormsPutEachOperandByteAsADot)
        {
            const Microcode  * set = GetCpu65C02InstructionSet();



            Assert::AreEqual (std::string ("LDA (..),Y"),   DebuggerViewState::GetOpcodeForm (set, 0xB1));
            Assert::AreEqual (std::string ("LDA #.."),      DebuggerViewState::GetOpcodeForm (set, 0xA9));
            Assert::AreEqual (std::string ("LDA ...."),     DebuggerViewState::GetOpcodeForm (set, 0xAD));
            Assert::AreEqual (std::string ("JMP (....,X)"), DebuggerViewState::GetOpcodeForm (set, 0x7C));
            Assert::AreEqual (std::string ("INX"),          DebuggerViewState::GetOpcodeForm (set, 0xE8));
        }



        TEST_METHOD (ZoomedAllTheWayInACellShowsItsValueAndSecondLine)
        {
            HeatMapView           view;
            MockDxuiTextRenderer  text;
            std::vector<Byte>     execute (kCount, 0);
            std::vector<Byte>     opcodes (kCount, 0);
            std::vector<int16_t>  values  (kCount, -1);
            auto                  forms   = std::make_shared<std::vector<std::string>> (256);



            Place (view);

            // $0000: LDA #$41, run; $0002 cannot be read.
            values[0]       = 0xA9;
            values[1]       = 0x41;
            execute[0]      = 200;
            execute[1]      = 200;
            opcodes[0]      = 1;
            (*forms)[0xA9]  = "LDA #..";

            view.SetOpcodes     (opcodes);
            view.SetValues      (values);
            view.SetOpcodeForms (forms);
            view.SetLevels      (execute, {}, {});

            Paint (view, text);
            Assert::IsFalse (HasText (text, L"A9"), L"three-pixel cells hold no text");

            view.ZoomAt ({ 300, 200 }, 40.0f);
            view.ScrollBy (-1000000);
            Paint (view, text);

            Assert::AreEqual (HeatMapView::kMaxCellPx, view.GetCellPx());
            Assert::IsTrue  (HasText (text, L"LDA #$41"), L"the instruction across its opcode's and operand's cells");
            Assert::IsTrue  (HasText (text, L"A9 41"),    L"its bytes under it");
            Assert::IsFalse (HasText (text, L"01000001"), L"the operand drawn as part of the instruction");
            Assert::IsFalse (HasText (text, L"FF"),       L"a byte not known shows nothing");
        }



        TEST_METHOD (AnInstructionPastTheEndOfItsRowIsSplitAtItsMnemonic)
        {
            HeatMapView           view;
            MockDxuiTextRenderer  text;
            std::vector<Byte>     execute (kCount, 0);
            std::vector<Byte>     opcodes (kCount, 0);
            std::vector<int16_t>  values  (kCount, -1);
            auto                  forms   = std::make_shared<std::vector<std::string>> (256);
            Word                  at      = 0;
            RECT                  first   = {};
            RECT                  second  = {};
            int                   drawn   = 0;
            auto                  isRing  = [&text] (const RECT & cell)
            {
                return std::ranges::any_of (text.Calls(), [&cell] (const RecordedTextCall & call)
                {
                    return call.kind == RecordedTextKind::FillRect && call.argb == kPc &&
                           call.x == (float) (cell.left - HeatMapView::kStreetPx) && call.y == (float) (cell.top - HeatMapView::kStreetPx);
                });
            };



            Place (view);
            view.ZoomAt ({ 300, 200 }, 40.0f);
            view.ScrollBy (-1000000);

            // LDA $C000 at the last address of the first row, its operand on the second.
            at                    = (Word) (view.GetColumns() - 1);
            values[at]            = 0xAD;
            values[at + 1]        = 0x00;
            values[at + 2]        = 0xC0;
            execute[at]           = 200;
            execute[at + 1]       = 200;
            execute[at + 2]       = 200;
            opcodes[at]           = 1;
            (*forms)[0xAD]        = "LDA ....";

            view.SetOpcodes     (opcodes);
            view.SetValues      (values);
            view.SetOpcodeForms (forms);
            view.SetLevels      (execute, {}, {});
            view.SetCpuMarks    (at, std::nullopt);
            Paint (view, text);

            drawn = (int) std::ranges::count_if (text.Calls(), [] (const RecordedTextCall & call)
            {
                return call.kind == RecordedTextKind::DrawString && call.text == L"LDA $C000";
            });

            first  = view.GetCellRect (at);
            second = view.GetCellRect ((Word) (at + 1));

            Assert::AreEqual (0,    drawn,                     L"the instruction split between its pieces");
            Assert::IsTrue   (HasText (text, L"LDA"),          L"the mnemonic in the first");
            Assert::IsTrue   (HasText (text, L"$C000"),        L"the operand in the second");
            Assert::IsTrue   (HasText (text, L"00 C0"),        L"over the operand's bytes");
            Assert::IsTrue   (second.top > first.top,          L"the operand starts the next row");
            Assert::IsFalse  (HasText (text, L"00"),           L"the operand drawn as part of the instruction");
            Assert::IsTrue   (isRing (first),                  L"the PC's ring around the first piece");
            Assert::IsTrue   (isRing (second),                 L"and around the second");
            Assert::IsTrue   (Contains (view.GetTipText (at), L"\nLDA $C000  (AD 00 C0)"), L"the tip has the instruction and its bytes");
            Assert::AreEqual ((int) at, (int) *view.GetPickAt (POINT { (second.left + second.right) / 2, (second.top + second.bottom) / 2 }),
                              L"a point over an operand picks the instruction");
        }



        TEST_METHOD (AnOpcodeIsMarkedWhereItWasFetchedAndItsOperandsAreNot)
        {
            AccessHeatMap      map;
            std::vector<Byte>  marks;



            map.Start (GetCpu65C02InstructionSet(), 0);

            // $0300: LDA $2000, the opcode read, fetched, then its operands.
            map.OnWatchedAccess (0x0300, 0, BusAccess::Read, std::nullopt);
            map.OnFetch (0x0300, 0xAD);
            map.OnWatchedAccess (0x0301, 0, BusAccess::Read, std::nullopt);
            map.OnWatchedAccess (0x0302, 0, BusAccess::Read, std::nullopt);

            map.GetOpcodeMarks (HeatSpace::Cpu, marks);
            Assert::AreEqual (kCount, marks.size());
            Assert::AreEqual ((Byte) 1, marks[0x0300]);
            Assert::AreEqual ((Byte) 0, marks[0x0301]);
            Assert::AreEqual ((Byte) 0, marks[0x0302]);

            map.Reset();
            map.GetOpcodeMarks (HeatSpace::Cpu, marks);
            Assert::AreEqual ((Byte) 0, marks[0x0300], L"Reset clears them");

            map.Stop();
            map.GetOpcodeMarks (HeatSpace::Cpu, marks);
            Assert::IsTrue (marks.empty(), L"off, none");
        }



        TEST_METHOD (AnOperandByteIsDrawnAsHotAsItsOpcode)
        {
            HeatMapView        view;
            std::vector<Byte>  execute (kCount, 0);
            std::vector<Byte>  opcodes (kCount, 0);



            Place (view);

            execute[0x0300] = 255;
            execute[0x0301] = 255;
            execute[0x0302] = 255;

            view.SetLevels (execute, {}, {});
            Assert::AreEqual (kExecute, view.GetCellColor (0x0301), L"no opcodes known, every byte is code");

            opcodes[0x0300] = 1;
            view.SetOpcodes (opcodes);
            view.SetLevels  (execute, {}, {});

            Assert::AreEqual (kExecute, view.GetCellColor (0x0300));
            Assert::AreEqual (kExecute, view.GetCellColor (0x0301), L"an operand, as hot as its opcode");
            Assert::AreEqual (kExecute, view.GetCellColor (0x0302));
            Assert::IsTrue   (Contains (view.GetTipText (0x0301), L"executed as an operand"));
            Assert::IsFalse  (Contains (view.GetTipText (0x0300), L"as an operand"));
        }



        TEST_METHOD (ThePcAndTheStackPointerAreOutlinedAndSaySo)
        {
            HeatMapView           view;
            MockDxuiTextRenderer  text;
            RECT                  cell     = {};
            bool                  isFramed = false;



            Place (view);
            view.SetCpuMarks (Word (0x0302), Word (0x01FD));

            Assert::IsTrue (std::vector<uint32_t> { kPc }    == view.GetOutlineColors (0x0302));
            Assert::IsTrue (std::vector<uint32_t> { kStack } == view.GetOutlineColors (0x01FD));
            Assert::IsTrue (view.GetOutlineColors (0x0303).empty());
            Assert::IsTrue (Contains (view.GetTipText (0x0302), L"\nPC"));
            Assert::IsTrue (Contains (view.GetTipText (0x01FD), L"\nStack pointer"));

            Paint (view, text);
            cell     = view.GetCellRect (0x0302);
            isFramed = std::ranges::any_of (text.Calls(), [&cell] (const RecordedTextCall & call)
            {
                return call.kind == RecordedTextKind::FillRect && call.argb == kPc &&
                       call.x == (float) (cell.left - HeatMapView::kStreetPx) && call.y == (float) (cell.top - HeatMapView::kStreetPx);
            });

            Assert::IsTrue (isFramed, L"a ring in the street around the PC's cell");
        }



        TEST_METHOD (TheSnapshotCarriesTheBytesTheOpcodesTheFormsAndThePcAndStack)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;
            HeatMapOptions        options;
            Byte                  sp = 0;



            rig.view.SetHeatMapShown (true);
            snapshot = rig.view.Build (rig.controller.GetSession());

            //  LDA #$41 at $0300.
            rig.machine.StepOne();
            snapshot = rig.view.Build (rig.controller.GetSession());
            sp       = rig.controller.GetSession().GetTarget().GetRegisters().sp;

            Assert::AreEqual (kCount, snapshot.heatMap.values.size());
            Assert::AreEqual ((int16_t) 0xA9, snapshot.heatMap.values[0x0300]);
            Assert::AreEqual ((int16_t) 0x41, snapshot.heatMap.values[0x0301]);
            Assert::IsTrue   (snapshot.heatMap.values[0xC000] >= 0,          L"I/O is read by its side-effect-free peek");
            Assert::AreEqual (MemoryRegionKeys::kRegionIo,                    snapshot.heatMap.regions[0xC000]);
            Assert::AreEqual ((uint16_t) (MemoryRegionKeys::kRegionSlotIo + 6), snapshot.heatMap.regions[0xC0E0], L"slot 6's I/O is a separate range");
            Assert::AreEqual ((uint16_t) 0,                                   snapshot.heatMap.regions[0x0300], L"main RAM is in none");
            Assert::AreEqual ((Byte) 1,       snapshot.heatMap.opcodes[0x0300]);
            Assert::AreEqual ((Byte) 0,       snapshot.heatMap.opcodes[0x0301]);
            Assert::IsTrue   (snapshot.heatMap.pc    == Word (0x0302));
            Assert::IsTrue   (snapshot.heatMap.stack == Word (0x0100 | sp));
            Assert::IsNotNull (snapshot.heatMap.opcodeForms.get());
            Assert::AreEqual (std::string ("LDA #.."), (*snapshot.heatMap.opcodeForms)[0xA9]);

            options.bank = HeatMapOptions::Bank::Main;
            rig.view.SetHeatMapOptions (options);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue   (snapshot.heatMap.pc == Word (0x0302), L"the code is in main RAM");
            Assert::AreEqual ((int16_t) 0xA9, snapshot.heatMap.values[0x0300], L"main RAM's own byte");

            options.bank = HeatMapOptions::Bank::Aux;
            rig.view.SetHeatMapOptions (options);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse  (snapshot.heatMap.pc.has_value(), L"not in aux RAM");
            Assert::IsTrue   (snapshot.heatMap.values[0x0300] >= 0, L"aux RAM's byte, read from its buffer");
        }



        TEST_METHOD (BreakpointKindsFollowTheBreakpoint)
        {
            BreakpointInfo  info;



            info.kind = BreakpointKind::Address;
            Assert::IsTrue (HeatMapView::GetBreakKind (info) == BreakKind::Execute);

            info.enabled = false;
            Assert::IsFalse (HeatMapView::GetBreakKind (info).has_value(), L"disabled, not outlined");

            info.enabled = true;
            info.kind    = BreakpointKind::Memory;
            info.access  = WatchAccess::Read;
            Assert::IsTrue (HeatMapView::GetBreakKind (info) == BreakKind::Read);

            info.access = WatchAccess::Write;
            Assert::IsTrue (HeatMapView::GetBreakKind (info) == BreakKind::Write);

            info.access = WatchAccess::ReadWrite;
            Assert::IsTrue (HeatMapView::GetBreakKind (info) == BreakKind::ReadWrite);

            info.kind = BreakpointKind::MemoryValue;
            Assert::IsTrue (HeatMapView::GetBreakKind (info) == BreakKind::Write);

            info.kind = BreakpointKind::Opcode;
            Assert::IsFalse (HeatMapView::GetBreakKind (info).has_value(), L"on no address");
        }



        TEST_METHOD (BreakpointsAreOutlinedInTheirKindsColorsAndListedInTheTip)
        {
            HeatMapView  view;



            Place (view);
            view.SetCpuMarks    (Word (0x0300), std::nullopt);
            view.SetBreakpoints ({ { 1, 0x0300, 0x0300, BreakKind::Execute   },
                                   { 2, 0x0400, 0x0401, BreakKind::Write     },
                                   { 3, 0x0500, 0x0500, BreakKind::ReadWrite },
                                   { 4, 0x0600, 0x0600, BreakKind::Read      },
                                   { 5, 0xD100, 0xD100, BreakKind::Execute   } });

            Assert::IsTrue (std::vector<uint32_t> { kPc, kBreakpoint }         == view.GetOutlineColors (0x0300));
            Assert::IsTrue (std::vector<uint32_t> { kWriteWatch }              == view.GetOutlineColors (0x0401));
            Assert::IsTrue (std::vector<uint32_t> { kReadWatch, kWriteWatch }  == view.GetOutlineColors (0x0500));
            Assert::IsTrue (std::vector<uint32_t> { kReadWatch }               == view.GetOutlineColors (0x0600));
            Assert::IsTrue (view.GetOutlineColors (0x0402).empty());

            Assert::IsTrue (Contains (view.GetTipText (0x0300), L"\nBreakpoint #1"));
            Assert::IsTrue (Contains (view.GetTipText (0x0400), L"\nWrite data breakpoint #2"));
            Assert::IsTrue (Contains (view.GetTipText (0x0500), L"\nRead and write data breakpoint #3"));
            Assert::IsTrue (Contains (view.GetTipText (0x0600), L"\nRead data breakpoint #4"));

            //  In main RAM's view, a breakpoint at $D100 marks both language
            //  card banks the CPU reaches there.
            view.SetShownBank (HeatMapOptions::Bank::Main, true);

            Assert::IsTrue (std::vector<uint32_t> { kBreakpoint } == view.GetOutlineColors (0xC100), L"bank 1, held at $C000");
            Assert::IsTrue (std::vector<uint32_t> { kBreakpoint } == view.GetOutlineColors (0xD100), L"bank 2");
        }



        TEST_METHOD (AnAddressWithASymbolIsTintedAndNamedInTheTip)
        {
            HeatMapView                      view;
            std::shared_ptr<HeatMapSymbols>  symbols = std::make_shared<HeatMapSymbols>();
            uint32_t                         tinted  = DxuiColor::Mix (kCold, kSymbol, HeatMapView::kSymbolTint) | 0xFF000000u;



            symbols->Add ("COUT",  0xFDED);
            symbols->Add ("WIDTH", 0x0010, 0, false, true);

            Place (view);
            view.SetLevels  ({}, {}, {});
            view.SetSymbols (symbols);

            Assert::AreEqual (tinted, view.GetCellColor (0xFDED));
            Assert::AreEqual (kCold,  view.GetCellColor (0x0010), L"a constant is no address");
            Assert::IsTrue   (Contains (view.GetTipText (0xFDED), L"\nSymbol COUT"));
        }



        //  Zoomed in, a cell with a symbol has its name over its value, an I/O
        //  byte's value is in I/O's color, each labeled range is outlined in its
        //  color, and its first cell has its label, a slot's by its card.
        TEST_METHOD (ZoomedInSymbolsRangesAndCardsAreLabeled)
        {
            HeatMapView                      view;
            MockDxuiTextRenderer             text;
            std::shared_ptr<HeatMapSymbols>  symbols = std::make_shared<HeatMapSymbols>();
            std::vector<int16_t>             values  (kCount, -1);
            std::vector<uint16_t>            regions (kCount, 0);
            MemoryRegionKeys::Colors         colors;
            MemoryRegionKeys::SlotCards      cards;
            uint32_t                         stroke  = 0;
            auto                             findInk = [&text] (const std::wstring & wanted)
            {
                for (const RecordedTextCall & call : text.Calls())
                {
                    if (call.kind == RecordedTextKind::DrawString && call.text == wanted)
                    {
                        return call.argb;
                    }
                }

                return 0u;
            };



            //  $0000-$0003 I/O, $0004-$0007 slot 2's I/O, $0008 main RAM.
            for (Word address = 0; address < 9; address++)
            {
                values[address] = 0xC1;
            }

            for (Word address = 0; address < 4; address++)
            {
                regions[address]     = MemoryRegionKeys::kRegionIo;
                regions[address + 4] = MemoryRegionKeys::kRegionSlotIo + 2;
            }

            symbols->Add ("KBD", 0x0000);
            cards[2] = "Super Serial Card";

            Place (view);
            view.SetLevels       ({}, {}, {});
            view.SetValues       (values);
            view.SetSymbols      (symbols);
            view.SetRegions      (regions);
            view.SetSlotCards    (cards);
            view.SetRegionColors (colors);
            view.ZoomAt ({ 300, 200 }, 40.0f);
            view.ScrollBy (-1000000);
            Paint (view, text);

            stroke = DxuiColor::Mix (kBackground | 0xFF000000u, colors.io, 0.8f) | 0xFF000000u;

            Assert::IsTrue   (HasText (text, L"KBD"),                          L"the symbol's name in its cell");
            Assert::IsTrue   (HasText (text, L"I/O"),                          L"the I/O range's label");
            Assert::IsTrue   (HasText (text, L"Super Serial Card in slot 2"),  L"slot 2's range by its card");
            Assert::AreEqual (DebuggerTextColors::GetReadable (colors.io, view.GetCellColor (0x0000) | 0xFF000000u), findInk (L"C1"), L"an I/O value in I/O's color");
            Assert::IsTrue   (std::ranges::any_of (text.Calls(), [stroke] (const RecordedTextCall & call) { return call.kind == RecordedTextKind::FillRect && call.argb == stroke; }),
                              L"the range outlined in its color");
        }



        TEST_METHOD (BlendMixesTheKindsAndGivesSelfModifyingCodeItsOwnColor)
        {
            HeatMapView::Palette  palette = MakePalette();



            Assert::AreEqual (kExecute,       HeatMapView::GetColor (Mode::All,  255, 0,   255, palette),               L"without Blend, the hottest kind");
            Assert::AreEqual (kSelfModifying, HeatMapView::GetColor (Mode::All,  255, 0,   255, palette, true));
            Assert::AreEqual (0xFFCDCAF4u,    HeatMapView::GetColor (Mode::All,  0,   255, 255, palette, true),  L"read and written alike, half each");
            Assert::AreEqual (kExecute,       HeatMapView::GetColor (Mode::Code, 255, 0,   255, palette, true),  L"Code shows the code alone");
            Assert::AreEqual (kWrite,         HeatMapView::GetColor (Mode::Data, 255, 0,   255, palette, true),  L"Data shows the data alone");
            Assert::AreEqual (kCold,          HeatMapView::GetColor (Mode::All,  0,   0,   0,   palette, true));
        }



        TEST_METHOD (BlendIsKeptInTheOptionsAndOffByDefault)
        {
            HeatMapOptions  options;



            Assert::IsFalse (options.blend);
            Assert::IsFalse (HeatMapOptions::FromText ("fade=10 view=all").blend);

            options.blend = true;

            Assert::IsTrue (options.ToText().find (" blend") != std::string::npos);
            Assert::IsTrue (HeatMapOptions::FromText (options.ToText()).blend);
        }



        TEST_METHOD (SelfModifyingCodeIsCalledOutInTheTipAndTheBlend)
        {
            HeatMapView           view;
            HeatMapOptions        options;
            std::vector<Byte>     execute (kCount, 0);
            std::vector<Byte>     write   (kCount, 0);



            Place (view);

            execute[0x0300] = 255;
            write[0x0300]   = 255;
            view.SetLevels (execute, {}, write);

            Assert::IsTrue (Contains (view.GetTipText (0x0300), L"\nSelf-modifying: run as code and written"));
            Assert::IsFalse (Contains (view.GetTipText (0x0301), L"Self-modifying"));

            options.blend = true;
            view.SetOptions (options);

            Assert::AreEqual (kSelfModifying, view.GetCellColor (0x0300));
        }



        TEST_METHOD (TheHeatMapsColorsAreInTheLegend)
        {
            using Meaning = ColorLegend::Meaning;

            ColorLegend::Palette  palette;
            std::vector<Meaning>  listed;



            palette.text       = DebuggerTextColors::Make (0xFF1E1E1E, 0xFFD4D4D4, 0xFF808080, 0);
            palette.pcMarker   = kPc;
            palette.breakpoint = kBreakpoint;

            for (const ColorLegend::Entry & entry : ColorLegend::GetEntries())
            {
                if (entry.pane == ColorLegend::Pane::HeatMap)
                {
                    listed.push_back (entry.meaning);
                }
            }

            Assert::IsTrue (std::vector<Meaning> { Meaning::HeatCode, Meaning::HeatRead, Meaning::HeatWrite, Meaning::HeatUnwritten,
                                                   Meaning::HeatSymbol, Meaning::HeatPc, Meaning::HeatStack, Meaning::HeatBreakpoint,
                                                   Meaning::HeatReadWatch, Meaning::HeatWriteWatch } == listed);

            Assert::AreEqual (kPc,                             ColorLegend::GetArgb (Meaning::HeatPc,            palette), L"the disassembly's PC yellow");
            Assert::AreEqual (kBreakpoint,                     ColorLegend::GetArgb (Meaning::HeatBreakpoint,    palette), L"the disassembly's breakpoint red");
            Assert::AreEqual (palette.text.heatStack,          ColorLegend::GetArgb (Meaning::HeatStack,         palette));
            Assert::AreEqual (palette.text.heatReadWatch,      ColorLegend::GetArgb (Meaning::HeatReadWatch,     palette));
            Assert::AreEqual (palette.text.heatWriteWatch,     ColorLegend::GetArgb (Meaning::HeatWriteWatch,    palette));
            Assert::AreEqual (palette.text.heatSelfModifying,  ColorLegend::GetArgb (Meaning::HeatSelfModifying, palette));
            Assert::AreEqual (std::wstring (L"PC"),            std::wstring (ColorLegend::GetText (Meaning::HeatPc)));
            Assert::AreEqual (std::wstring (L"Stack pointer"), std::wstring (ColorLegend::GetText (Meaning::HeatStack)));
        }
    };
}
