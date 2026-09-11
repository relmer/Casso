#include "Pch.h"

#include "Shell/Input/AppleKeyMapping.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyMappingTests
//
//  Host virtual keys to Apple keys.
//
//  These were private helpers on the shell, so the only way to check that
//  Escape still reaches the guest was to press Escape and look. A mapping that
//  quietly stopped working showed up as a key that does nothing, which is the
//  kind of regression that survives a release because nobody presses every key.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleKeyMappingTests)
{
public:

    TEST_METHOD (TheSevenSpecialKeysMap)
    {
        AssertMaps (VK_LEFT,   AppleSpecialKey::Left);
        AssertMaps (VK_RIGHT,  AppleSpecialKey::Right);
        AssertMaps (VK_UP,     AppleSpecialKey::Up);
        AssertMaps (VK_DOWN,   AppleSpecialKey::Down);
        AssertMaps (VK_TAB,    AppleSpecialKey::Tab);
        AssertMaps (VK_ESCAPE, AppleSpecialKey::Escape);
        AssertMaps (VK_DELETE, AppleSpecialKey::Delete);
    }


    TEST_METHOD (AnOrdinaryKeyIsNotSpecialAndLeavesTheOutputAlone)
    {
        AppleSpecialKey  key    = AppleSpecialKey::Escape;
        bool             mapped = AppleKeyMapping::TryMapVkToSpecialKey ('A', key);

        Assert::IsFalse (mapped, L"'A' is a character key, not a special one");
        Assert::IsTrue  (key == AppleSpecialKey::Escape,
                         L"a refused mapping must not write to the out parameter");
    }


    TEST_METHOD (OnlyTabAndEscapeAlsoArriveAsCharacters)
    {
        //  Windows manufactures a WM_CHAR for these two and for no other
        //  special key. Getting this wrong delivers the key twice, or swallows
        //  a character the machine refused and should have received.
        Assert::IsTrue (AppleKeyMapping::DoesSpecialKeySynthesizeChar (AppleSpecialKey::Tab));
        Assert::IsTrue (AppleKeyMapping::DoesSpecialKeySynthesizeChar (AppleSpecialKey::Escape));

        Assert::IsFalse (AppleKeyMapping::DoesSpecialKeySynthesizeChar (AppleSpecialKey::Left));
        Assert::IsFalse (AppleKeyMapping::DoesSpecialKeySynthesizeChar (AppleSpecialKey::Right));
        Assert::IsFalse (AppleKeyMapping::DoesSpecialKeySynthesizeChar (AppleSpecialKey::Up));
        Assert::IsFalse (AppleKeyMapping::DoesSpecialKeySynthesizeChar (AppleSpecialKey::Down));
        Assert::IsFalse (AppleKeyMapping::DoesSpecialKeySynthesizeChar (AppleSpecialKey::Delete));
    }


    TEST_METHOD (TheFourArrowsAreArrowsAndNothingElseIs)
    {
        Assert::IsTrue (AppleKeyMapping::IsArrowVk (VK_LEFT));
        Assert::IsTrue (AppleKeyMapping::IsArrowVk (VK_RIGHT));
        Assert::IsTrue (AppleKeyMapping::IsArrowVk (VK_UP));
        Assert::IsTrue (AppleKeyMapping::IsArrowVk (VK_DOWN));

        //  The arrows double as the joystick when the user asks for that, so a
        //  key wrongly classified here moves a paddle nobody touched.
        Assert::IsFalse (AppleKeyMapping::IsArrowVk (VK_TAB));
        Assert::IsFalse (AppleKeyMapping::IsArrowVk (VK_ESCAPE));
        Assert::IsFalse (AppleKeyMapping::IsArrowVk (VK_DELETE));
        Assert::IsFalse (AppleKeyMapping::IsArrowVk (VK_RETURN));
        Assert::IsFalse (AppleKeyMapping::IsArrowVk ('W'));
    }


    TEST_METHOD (EveryArrowIsAlsoASpecialKey)
    {
        //  The two classifiers must agree: an arrow that is not a special key
        //  would be staged as a joystick axis and never reach the keyboard.
        for (WPARAM vk : { VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN })
        {
            AppleSpecialKey  key = AppleSpecialKey::Escape;

            Assert::IsTrue (AppleKeyMapping::IsArrowVk (vk));
            Assert::IsTrue (AppleKeyMapping::TryMapVkToSpecialKey (vk, key));
        }
    }


private:

    static void AssertMaps (WPARAM vk, AppleSpecialKey expected)
    {
        AppleSpecialKey  key    = AppleSpecialKey::Delete;
        bool             mapped = AppleKeyMapping::TryMapVkToSpecialKey (vk, key);

        Assert::IsTrue (mapped, L"this host key must map to an Apple key");
        Assert::IsTrue (key == expected, L"and to the right one");
    }
};
