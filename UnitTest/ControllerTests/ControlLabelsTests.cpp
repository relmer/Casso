#include "Pch.h"

#include "Controllers/ControlLabels.h"
#include "Controllers/XInputSampleDecoder.h"
#include "Core/UnicodeSymbols.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControlLabelsTests
//
//  What each control is called in the mapping list. An Xbox controller's
//  controls carry the names on the controller; a DirectInput device's are
//  counted from one; and no control of any kind is ever left unlabeled.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (ControlLabelsTests)
    {
    public:

        TEST_METHOD (Xbox_UsesTheNamesOnTheController)
        {
            Assert::AreEqual (std::wstring (L"A"),  ControlLabels::For (ControllerKind::XInput, { ControlKind::Button, 0 }));
            Assert::AreEqual (std::wstring (L"B"),  ControlLabels::For (ControllerKind::XInput, { ControlKind::Button, 1 }));
            Assert::AreEqual (std::wstring (L"X"),  ControlLabels::For (ControllerKind::XInput, { ControlKind::Button, 2 }));
            Assert::AreEqual (std::wstring (L"Y"),  ControlLabels::For (ControllerKind::XInput, { ControlKind::Button, 3 }));
            Assert::AreEqual (std::wstring (L"Left bumper (LB)"),   ControlLabels::For (ControllerKind::XInput, { ControlKind::Button, 4 }));
            Assert::AreEqual (std::wstring (L"Left trigger (LT)"),  ControlLabels::For (ControllerKind::XInput, { ControlKind::Trigger, 0 }));
            Assert::AreEqual (std::wstring (L"Right trigger (RT)"), ControlLabels::For (ControllerKind::XInput, { ControlKind::Trigger, 1 }));
            Assert::AreEqual (std::wstring (L"Right stick X"),
                              ControlLabels::For (ControllerKind::XInput, { ControlKind::Axis, XInputSampleDecoder::kRightStickX }));
            Assert::AreEqual (std::wstring (L"D-pad left"), ControlLabels::For (ControllerKind::XInput, { ControlKind::DpadLeft, 0 }));
        }


        TEST_METHOD (XboxControlsHaveGlyphs_OtherControllersDoNot)
        {
            Assert::AreEqual (std::wstring (s_kpszMdl2ButtonA),     ControlLabels::GlyphFor (ControllerKind::XInput, { ControlKind::Button,   0 }));
            Assert::AreEqual (std::wstring (s_kpszMdl2ButtonX),     ControlLabels::GlyphFor (ControllerKind::XInput, { ControlKind::Button,   2 }), L"X is third in the decoder's order, though fourth in the font's");
            Assert::AreEqual (std::wstring (s_kpszMdl2ButtonView),  ControlLabels::GlyphFor (ControllerKind::XInput, { ControlKind::Button,   6 }));
            Assert::AreEqual (std::wstring (s_kpszMdl2TriggerLeft), ControlLabels::GlyphFor (ControllerKind::XInput, { ControlKind::Trigger,  0 }));
            Assert::AreEqual (std::wstring (s_kpszMdl2Dpad),        ControlLabels::GlyphFor (ControllerKind::XInput, { ControlKind::DpadLeft, 0 }));
            Assert::IsTrue   (ControlLabels::GlyphFor (ControllerKind::XInput,      { ControlKind::Button, 10 }).empty(), L"a button past the named ones has none");
            Assert::IsTrue   (ControlLabels::GlyphFor (ControllerKind::DirectInput, { ControlKind::Button, 0 }).empty(),  L"and a DirectInput device has none");
        }


        TEST_METHOD (DirectInput_NamesAxesAndNumbersTheRestFromOne)
        {
            Assert::AreEqual (std::wstring (L"X axis"),     ControlLabels::For (ControllerKind::DirectInput, { ControlKind::Axis, 0 }));
            Assert::AreEqual (std::wstring (L"Z rotation"), ControlLabels::For (ControllerKind::DirectInput, { ControlKind::Axis, 5 }));
            Assert::AreEqual (std::wstring (L"Button 1"),   ControlLabels::For (ControllerKind::DirectInput, { ControlKind::Button, 0 }));
            Assert::AreEqual (std::wstring (L"D-pad up"),   ControlLabels::For (ControllerKind::DirectInput, { ControlKind::DpadUp, 0 }));
            Assert::AreEqual (std::wstring (L"Hat 2 down"), ControlLabels::For (ControllerKind::DirectInput, { ControlKind::DpadDown, 1 }));
        }


        TEST_METHOD (XboxNamesAreNotTheDirectInputFallback)
        {
            Assert::AreNotEqual (ControlLabels::For (ControllerKind::XInput,      { ControlKind::Button, 0 }),
                                 ControlLabels::For (ControllerKind::DirectInput, { ControlKind::Button, 0 }));
        }


        TEST_METHOD (EveryKindOnBothControllersHasALabel)
        {
            const ControlKind  kinds[] = { ControlKind::Axis, ControlKind::Trigger, ControlKind::Button,
                                           ControlKind::DpadUp, ControlKind::DpadDown,
                                           ControlKind::DpadLeft, ControlKind::DpadRight };

            for (ControllerKind controller : { ControllerKind::XInput, ControllerKind::DirectInput })
            {
                for (ControlKind kind : kinds)
                {
                    Assert::IsFalse (ControlLabels::For (controller, { kind, 0 }).empty());
                }
            }
        }


        TEST_METHOD (AnIndexPastTheNamedOnesIsStillLabeled)
        {
            Assert::AreEqual (std::wstring (L"Button 43"), ControlLabels::For (ControllerKind::XInput,      { ControlKind::Button, 42 }));
            Assert::AreEqual (std::wstring (L"Axis 12"),   ControlLabels::For (ControllerKind::DirectInput, { ControlKind::Axis, 11 }));
        }
    };
}
