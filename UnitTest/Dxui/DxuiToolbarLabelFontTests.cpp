#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"
#include "Widgets/DxuiToolbar.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarLabelFontTests
//
//  A toolbar's labels are drawn in the theme's body font, the size a pane's
//  title and tabs use, whatever the Windows menu font is.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarLabelFontTests)
{
public:

    TEST_METHOD (ALabelIsDrawnAtTheBodyFontSize)
    {
        DxuiToolbar                      bar;
        DxuiDpiScaler                    scaler;
        MockDxuiTextRenderer             text;
        MockDxuiPainter                  painter;
        MockDxuiTheme                    theme;
        std::vector<DxuiToolbar::Entry>  entries (1);
        auto                             command = std::make_shared<DxuiCommand>();
        float                            size    = -1.0f;

        command->id    = 1;
        command->label = L"Show columns";
        command->glyph = L"x";

        entries[0].command = command;

        scaler.SetDpi (144);
        bar.SetTextRenderer (&text);
        bar.SetEntries      (std::move (entries));
        bar.Layout          (RECT { 0, 0, 2000, 60 }, scaler);
        bar.Paint           (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == L"Show columns")
            {
                size = call.fontSizeDip;
            }
        }

        Assert::AreEqual (scaler.ToPxf (theme.BodyFont().sizeDip), size, 0.01f, L"the label uses the body font, scaled to the DPI");
    }
};
