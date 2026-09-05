#include "Pch.h"

#include "Widgets/DxuiTextInput.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TextInputDegenerateTests
//
//  What a text input paints when it has no room to paint in.
//
//  A control sizes itself from its window's client rect, and a window can have
//  none: one shown minimized has a 0x0 client, and every control laid out
//  against it comes out zero DIPs wide. Insetting that width by the field's
//  own padding then takes it BELOW zero, and a negative extent is not a small
//  box -- DirectWrite refuses one outright, so the frame fails an assertion,
//  and being a frame it fails again on the next one and the next. Launching
//  Casso minimized with the boot-disk picker due used to stack some thirty
//  modal assertion dialogs within seconds.
//
//  So the cases below paint a zero-width field and assert that nothing it
//  hands the renderer has a negative width -- neither the clip rect it pushes
//  nor the text or placeholder it draws. A field that is merely narrow, rather
//  than empty, still paints normally.
//
////////////////////////////////////////////////////////////////////////////////



TEST_CLASS (TextInputDegenerateTests)
{
public:

    static RECT MakeRect (int l, int t, int r, int b)
    {
        RECT  rc = { l, t, r, b };
        return rc;
    }

    // Every extent the control passed down, whatever the call was.
    static void AssertNoNegativeExtents (const MockDxuiTextRenderer & text)
    {
        for (const RecordedTextCall & call : text.Calls())
        {
            Assert::IsTrue (call.width  >= 0.0f, L"negative width reached the renderer");
            Assert::IsTrue (call.height >= 0.0f, L"negative height reached the renderer");
        }
    }

    TEST_METHOD (ZeroWidthBounds_PlaceholderNeverDrawnNegative)
    {
        DxuiTextInput          input;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;

        input.SetRect (MakeRect (0, 0, 0, 38));
        input.SetPlaceholder (L"Search");

        input.Paint (painter, text);

        AssertNoNegativeExtents (text);
    }

    TEST_METHOD (ZeroWidthBounds_WithTextNeverDrawnNegative)
    {
        DxuiTextInput          input;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;

        input.SetRect (MakeRect (0, 0, 0, 38));
        input.SetText (L"karateka");

        input.Paint (painter, text);

        AssertNoNegativeExtents (text);
    }

    TEST_METHOD (ZeroHeightBounds_NeverDrawnNegative)
    {
        DxuiTextInput          input;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;

        input.SetRect (MakeRect (0, 0, 0, 0));
        input.SetPlaceholder (L"Search");

        input.Paint (painter, text);

        AssertNoNegativeExtents (text);
    }

    // The floor must not cost a field that genuinely has room: a normal
    // width still reaches the placeholder draw as a positive one.
    TEST_METHOD (NormalBounds_PlaceholderStillDrawn)
    {
        DxuiTextInput          input;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        bool                   sawPlaceholder = false;

        input.SetRect (MakeRect (0, 0, 200, 38));
        input.SetPlaceholder (L"Search");

        input.Paint (painter, text);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == L"Search")
            {
                sawPlaceholder = true;
                Assert::IsTrue (call.width > 0.0f);
            }
        }

        Assert::IsTrue (sawPlaceholder);
        AssertNoNegativeExtents (text);
    }
};
