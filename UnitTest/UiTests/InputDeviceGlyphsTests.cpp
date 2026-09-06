#include "Pch.h"

#include "Ui/Chrome/InputDeviceGlyphs.h"
#include "../Dxui/MockDxuiPainter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InputDeviceGlyphsTests
//
//  Coverage for the drawn peripheral icons: each painter emits primitives,
//  and every primitive stays inside the box it was handed (the mock painter
//  records the quad/ellipse/line extents as bounding boxes, so containment
//  is asserted across all of them).
//
////////////////////////////////////////////////////////////////////////////////



TEST_CLASS (InputDeviceGlyphsTests)
{
public:

    TEST_METHOD (GlyphPainters_EmitPrimitivesInsideTheBox)
    {
        RECT  box = { 100, 100, 148, 148 };   // 48px glyph

        auto check = [&] (void (*paint) (IDxuiPainter &, const RECT &, bool), bool skeuo, const wchar_t * name)
        {
            MockDxuiPainter  p;
            paint (p, box, skeuo);

            Assert::IsTrue (p.Calls().size() > 4, name);

            for (const RecordedPaintCall & c : p.Calls())
            {
                Assert::IsTrue (c.x >= box.left - 2 && c.y >= box.top - 2 &&
                                c.x + c.width  <= box.right + 2 &&
                                c.y + c.height <= box.bottom + 2,
                    L"glyph primitives stay inside the box");
            }
        };

        check (&InputDeviceGlyphs::PaintJoystickGlyph, false, L"joystick flat emits");
        check (&InputDeviceGlyphs::PaintJoystickGlyph, true,  L"joystick skeuo emits");
        check (&InputDeviceGlyphs::PaintPaddleGlyph,   false, L"paddle flat emits");
        check (&InputDeviceGlyphs::PaintPaddleGlyph,   true,  L"paddle skeuo emits");
        check (&InputDeviceGlyphs::PaintMouseGlyph,    false, L"mouse flat emits");
        check (&InputDeviceGlyphs::PaintMouseGlyph,    true,  L"mouse skeuo emits");
    }
};
