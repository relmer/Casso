#include "Pch.h"

#include "Controllers/ControllerTokens.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerTokensTests
//
//  Preferences store controllers and controls as text, so the text is the
//  contract between one launch and the next. Every form round trips, and every
//  malformed form is refused rather than read as some other controller or
//  control: a binding that silently parsed "button:128" as button 0 would
//  press the wrong button with nothing reporting it.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (ControllerTokensTests)
    {
    public:

        static constexpr Word  kXboxVendor    = 0x045e;
        static constexpr Word  kXboxProduct   = 0x0b13;
        static constexpr Word  kStickVendor   = 0x044f;
        static constexpr Word  kStickProduct  = 0xb10a;


        // Every Xbox-class controller shares one key (FR-018a): XInput reports
        // them all through one fixed layout, and the same controller reports
        // 045E:02FF over USB and 045E:0B13 over Bluetooth, so a key built from
        // those IDs would split one controller's profiles by cable.
        TEST_METHOD (Model_EveryXInputControllerSharesOneToken)
        {
            ControllerModelKey  usb      = { ControllerKind::XInput, kXboxVendor, kXboxProduct };
            ControllerModelKey  bluetoo  = { ControllerKind::XInput, kXboxVendor, 0x02ff };
            ControllerModelKey  parsed;
            HRESULT             hr       = ControllerTokens::ModelFromToken (ControllerTokens::ModelToToken (usb), parsed);

            Assert::AreEqual (std::string ("xinput"), ControllerTokens::ModelToToken (usb));
            Assert::AreEqual (std::string ("xinput"), ControllerTokens::ModelToToken (bluetoo),
                L"the same controller on another connection must produce the same key");
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (parsed.kind == ControllerKind::XInput, L"the parsed key is an XInput key");
            Assert::AreEqual (static_cast<Word> (0), parsed.vendorId,  L"an XInput key carries no vendor");
            Assert::AreEqual (static_cast<Word> (0), parsed.productId, L"an XInput key carries no product");
        }


        TEST_METHOD (Model_UppercaseHexParses)
        {
            ControllerModelKey  parsed;
            HRESULT             hr = ControllerTokens::ModelFromToken ("dinput:044F:B10A", parsed);

            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (kStickProduct, parsed.productId);
        }


        TEST_METHOD (Unit_DirectInputSerialRoundTrips)
        {
            ControllerUnitKey  unit   = { { ControllerKind::DirectInput, kStickVendor, kStickProduct }, "A1/B2", ControllerUnitSource::Serial };
            ControllerUnitKey  parsed;
            std::string        token  = ControllerTokens::UnitToToken (unit);
            HRESULT            hr     = ControllerTokens::UnitFromToken (token, parsed);

            Assert::AreEqual (std::string ("dinput:044f:b10a/serial:A1/B2"), token);
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (parsed == unit, L"a serial containing a slash must survive the round trip");
        }


        TEST_METHOD (Unit_DirectInputGuidRoundTrips)
        {
            ControllerUnitKey  unit   = { { ControllerKind::DirectInput, kStickVendor, kStickProduct }, "{8E8A0001-0000-0000-0000-504944564944}", ControllerUnitSource::InstanceGuid };
            ControllerUnitKey  parsed;
            HRESULT            hr     = ControllerTokens::UnitFromToken (ControllerTokens::UnitToToken (unit), parsed);

            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (parsed == unit, L"an instance GUID identity must survive the round trip");
        }


        TEST_METHOD (Unit_WithoutIdentityRoundTrips)
        {
            ControllerUnitKey  xbox   = { { ControllerKind::XInput, 0, 0 }, "", ControllerUnitSource::None };
            ControllerUnitKey  stick  = { { ControllerKind::DirectInput, kStickVendor, kStickProduct }, "", ControllerUnitSource::None };
            ControllerUnitKey  parsed;
            HRESULT            hr     = S_OK;

            hr = ControllerTokens::UnitFromToken (ControllerTokens::UnitToToken (xbox), parsed);
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (parsed == xbox, L"an Xbox unit is its model");

            hr = ControllerTokens::UnitFromToken (ControllerTokens::UnitToToken (stick), parsed);
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (parsed == stick, L"a DirectInput unit with no identity is its model");
        }


        TEST_METHOD (Control_EveryKindRoundTrips)
        {
            const ControlId  controls[] =
            {
                { ControlKind::Axis,      7   },
                { ControlKind::Trigger,   1   },
                { ControlKind::Button,    127 },
                { ControlKind::DpadUp,    0   },
                { ControlKind::DpadDown,  1   },
                { ControlKind::DpadLeft,  2   },
                { ControlKind::DpadRight, 3   },
            };

            for (const ControlId & control : controls)
            {
                ControlId    parsed;
                std::string  token = ControllerTokens::ControlToToken (control);
                HRESULT      hr    = ControllerTokens::ControlFromToken (token, parsed);

                Assert::AreEqual (S_OK, hr, std::wstring (token.begin(), token.end()).c_str());
                Assert::IsTrue   (parsed == control, std::wstring (token.begin(), token.end()).c_str());
            }
        }


        TEST_METHOD (Control_TokenText)
        {
            Assert::AreEqual (std::string ("dpad-left:0"), ControllerTokens::ControlToToken ({ ControlKind::DpadLeft, 0 }));
            Assert::AreEqual (std::string ("axis:1"),      ControllerTokens::ControlToToken ({ ControlKind::Axis, 1 }));
        }


        TEST_METHOD (Malformed_ModelsAreRefused)
        {
            const char *  tokens[] =
            {
                "", "gamepad:045e:0b13", "dinput:45e:0b13", "dinput:045g:0b13",
                "dinput:045e", "dinput:generic", "dinput:045e:0b13:00",
                "xinput:045e:0b13", "xinput:generic", "xinputx",
            };

            for (const char * token : tokens)
            {
                ControllerModelKey  parsed;
                HRESULT             hr = ControllerTokens::ModelFromToken (token, parsed);

                Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, std::wstring (token, token + strlen (token)).c_str());
            }
        }


        TEST_METHOD (Malformed_UnitsAreRefused)
        {
            const char *  tokens[] =
            {
                "dinput:044f:b10a/", "dinput:044f:b10a/serial:", "dinput:044f:b10a/guid:",
                "dinput:044f:b10a/other:x", "xinput:045e:0b13/serial:ABC",
            };

            for (const char * token : tokens)
            {
                ControllerUnitKey  parsed;
                HRESULT            hr = ControllerTokens::UnitFromToken (token, parsed);

                Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, std::wstring (token, token + strlen (token)).c_str());
            }
        }


        TEST_METHOD (Malformed_ControlsAreRefused)
        {
            const char *  tokens[] =
            {
                "", "axis", "axis:", "axis:8", "trigger:2", "button:128", "dpad-up:4",
                "axis:-1", "axis:1x", "axis:0001", "stick:0",
            };

            for (const char * token : tokens)
            {
                ControlId  parsed;
                HRESULT    hr = ControllerTokens::ControlFromToken (token, parsed);

                Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, std::wstring (token, token + strlen (token)).c_str());
            }
        }
    };
}
