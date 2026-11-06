#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/CodeMargins.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/Panes/SourcePane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CodeMarginTests
    //
    //  A disassembly view and a source document have the same margins ahead
    //  of their text: the glyph margin, with a breakpoint's dot and the PC's
    //  arrow beside it, then the heat strip while heat is on, each as wide
    //  and in the same place in both, at every scale.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CodeMarginTests)
    {
    public:
        static constexpr uint32_t  kHeat        = 0xFFE05030;
        static constexpr int       kDpis[]      = { 96, 120, 144 };
        static constexpr LONG      kViewWidth   = 800;
        static constexpr LONG      kViewHeight  = 400;


        //  A disassembly list as a view has it, with the heat column on, and
        //  one row whose heat cell has the heat color.
        static void  PaintList (DxuiListView & list, int dpi, MockDxuiPainter & painter, MockDxuiTextRenderer & text,
                                std::shared_ptr<const DxuiIconImage> icon = nullptr)
        {
            CassoTheme                                    theme   = CassoTheme::MakeSkeuomorphic();
            DxuiDpiScaler                                 scaler;
            DisassemblyOptions                            options;
            std::vector<std::vector<DxuiListView::Cell>>  rows (1, std::vector<DxuiListView::Cell> (DisassemblyCells::kColumnCount));



            scaler.SetDpi (dpi);
            options.Set (DisassemblyOptions::Option::Heat, true);

            DebuggerWindow::MakeDense     (&list);
            DisassemblyCells::Configure   (list);
            DisassemblyCells::ShowColumns (list, options);
            list.SetPaneTextInset         (false);
            list.Layout                   (RECT { 0, 0, kViewWidth, kViewHeight }, scaler);

            rows[0][DisassemblyCells::kGutterColumn].icon      = icon;
            rows[0][DisassemblyCells::kHeatColumn].text        = L"  ";
            rows[0][DisassemblyCells::kHeatColumn].background  = kHeat;
            rows[0][DisassemblyCells::kAddressColumn].text     = L"6000";
            rows[0][DisassemblyCells::kInstructionColumn].text = L"LDA #$41";

            list.SetRows (std::move (rows));
            list.Paint   (painter, text, theme);
        }


        //  A source document's view as its pane configures it, heat on, and
        //  one line whose heat is the heat color.
        static void  PaintSource (DxuiTextView & view, int dpi, MockDxuiPainter & painter, MockDxuiTextRenderer & text,
                                  std::shared_ptr<const DxuiIconImage> icon = nullptr)
        {
            CassoTheme             theme  = CassoTheme::MakeSkeuomorphic();
            DxuiDpiScaler          scaler;
            DxuiActionBanner       banner;
            SourcePane             pane (&view, &banner,
                                         [] (const DebugSourceFile &, const std::wstring &, const std::string &) { return SourceLookup(); },
                                         [] (const DebuggerActionBuilder &) {});
            DxuiTextView::Row      row;



            scaler.SetDpi (dpi);

            pane.Configure (nullptr);
            pane.SetHeat   (HeatMapView::Palette());

            row.cells      = { L"", L"1", L"        lda #$41" };
            row.gutterFill = kHeat;
            row.icon       = icon;

            view.SetCellSize (8, 16);
            view.Layout      (RECT { 0, 0, kViewWidth, kViewHeight }, scaler);
            view.SetRows     ({ row });
            view.Paint       (painter, text, theme);
        }


        static const RecordedPaintCall *  FindFill (const MockDxuiPainter & painter, uint32_t argb)
        {
            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillRect && call.argb == argb)
                {
                    return &call;
                }
            }

            return nullptr;
        }


        //  The heat strip of a source line is drawn where a disassembly row's
        //  heat cell is, as wide, right after the glyph margin.
        TEST_METHOD (TheHeatStripIsTheSameWidthAndPlaceInBoth)
        {
            for (int dpi : kDpis)
            {
                DxuiDpiScaler              scaler;
                DxuiListView               list;
                DxuiTextView               view;
                MockDxuiPainter            listPainter;
                MockDxuiPainter            viewPainter;
                MockDxuiTextRenderer       text;
                const RecordedPaintCall  * listFill = nullptr;
                const RecordedPaintCall  * viewFill = nullptr;
                std::wstring               at       = std::format (L"at {} DPI", dpi);



                scaler.SetDpi (dpi);
                PaintList   (list, dpi, listPainter, text);
                PaintSource (view, dpi, viewPainter, text);

                listFill = FindFill (listPainter, kHeat);
                viewFill = FindFill (viewPainter, kHeat);

                Assert::IsNotNull (listFill, at.c_str());
                Assert::IsNotNull (viewFill, at.c_str());
                Assert::AreEqual  ((float) scaler.ToPx (CodeMargins::kGlyphDip), listFill->x,     0.5f, at.c_str());
                Assert::AreEqual  ((float) scaler.ToPx (CodeMargins::kHeatDip),  listFill->width, 0.5f, at.c_str());
                Assert::AreEqual  (listFill->x,     viewFill->x,     0.5f, (L"the strip starts where the heat column does " + at).c_str());
                Assert::AreEqual  (listFill->width, viewFill->width, 0.5f, (L"and is as wide " + at).c_str());
            }
        }


        //  The glyph margin's dot, with the PC's arrow beside it, is centered
        //  on the same line in both, and the margin is as wide.
        TEST_METHOD (TheGlyphMarginIsTheSameInBoth)
        {
            auto  icon = std::make_shared<DxuiIconImage>();



            icon->width  = 4;
            icon->height = 4;
            icon->bgraPremul.assign (16, 0xFF0000FFu);

            for (int dpi : kDpis)
            {
                DxuiDpiScaler         scaler;
                DxuiListView          list;
                DxuiTextView          view;
                MockDxuiPainter       painter;
                MockDxuiTextRenderer  listText;
                MockDxuiTextRenderer  viewText;
                float                 textLeft = 0.0f;
                std::wstring          at       = std::format (L"at {} DPI", dpi);



                scaler.SetDpi (dpi);
                PaintList   (list, dpi, painter, listText, icon);
                PaintSource (view, dpi, painter, viewText, icon);

                Assert::AreEqual ((size_t) 1, listText.IconCalls().size(), at.c_str());
                Assert::AreEqual ((size_t) 1, viewText.IconCalls().size(), at.c_str());
                Assert::AreEqual (listText.IconCalls()[0].x, viewText.IconCalls()[0].x, 0.001f, (L"the dot is in the same place " + at).c_str());

                Assert::AreEqual (scaler.ToPx (CodeMargins::kGlyphDip), list.GetColumnEffectiveWidthPx (DisassemblyCells::kGutterColumn), at.c_str());
                Assert::IsTrue   (view.IsInGutter (POINT { scaler.ToPx (CodeMargins::kGlyphDip) - 1, 10 }), (L"the glyph margin " + at).c_str());
                Assert::IsFalse  (view.IsInGutter (POINT { scaler.ToPx (CodeMargins::kGlyphDip),     10 }), (L"ends where the heat strip starts " + at).c_str());
                Assert::IsTrue   (view.IsInGutterBand (POINT { scaler.ToPx (CodeMargins::kGlyphDip), 10 }), at.c_str());

                Assert::IsTrue   (view.GetCellLeftPx (0, 0, textLeft), at.c_str());
                Assert::AreEqual ((float) (list.GetPaneLeadPx() + list.GetColumnEffectiveWidthPx (DisassemblyCells::kGutterColumn) +
                                           list.GetColumnEffectiveWidthPx (DisassemblyCells::kHeatColumn)),
                                  textLeft, 0.5f, (L"the text starts where the margins end, as the address column does " + at).c_str());
            }
        }
    };
}
