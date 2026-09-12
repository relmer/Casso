#include "Pch.h"
#include "Shell/EmulatorShell.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellKeyWiringTests
//
//  ShellKeyRoutingTests covers WHICH decision is correct. These cover whether
//  the handlers use it: that OnKeyDown routes through the classifier, that a
//  chrome-owned key arms the swallow, and that OnChar drops the character. A
//  classifier that is correct and never consulted is the same defect over
//  again, and nothing above this file would catch it.
//
//  EmulatorShell is driven here without Initialize -- no HWND, no D3D device,
//  no WASAPI client, because those members construct empty and the work
//  happens in Initialize. The handlers are private overrides, so the tests
//  reach them the way the host does, through IDxuiHostClient.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ShellKeyWiringTests)
{
public:

    //  Records what the guest was handed. The real viewport forwards to the
    //  machine; this one only counts, so delivery is visible without one.
    class StubViewport : public DxuiViewport
    {
    public:
        int     keys     = 0;
        int     chars    = 0;
        WPARAM  lastChar = 0;

        bool  OnKey (const DxuiKeyEvent & ev) override
        {
            if (ev.kind == DxuiKeyEventKind::Char)
            {
                chars++;
                lastChar = ev.vk;
            }
            else
            {
                keys++;
            }

            return true;
        }
    };


    //  Exposes the protected keystroke state, the way TestCpu exposes Cpu's.
    class TestShell : public EmulatorShell
    {
    public:
        bool  IsSwallowArmed() const            { return m_swallowMetaChar; }
        void  ArmSwallow     (bool armed)       { m_swallowMetaChar = armed; }
        void  SetViewport    (DxuiViewport * v) { m_viewport = v; }
    };


    //  A classifier that returns what the test told it to, and records the
    //  call so the test can tell a consulted one from an ignored one.
    struct Recorder
    {
        int                     calls    = 0;
        WPARAM                  lastVk   = 0;
        ShellKeyRouting::State  lastState;
        ShellKeyOwner           verdict  = ShellKeyOwner::Guest;
    };

    static EmulatorShell::KeyOwnerFn  MakeResolver (Recorder & rec)
    {
        return [&rec] (const ShellKeyRouting::State & state, WPARAM vk)
        {
            rec.calls++;
            rec.lastVk    = vk;
            rec.lastState = state;
            return rec.verdict;
        };
    }


    TEST_METHOD (OnKeyDownRoutesThroughTheClassifier_WithTheKeyPressed)
    {
        std::unique_ptr<TestShell>  shell = std::make_unique<TestShell>();
        IDxuiHostClient *           host  = shell.get();
        Recorder                    rec;


        rec.verdict = ShellKeyOwner::Guest;
        shell->SetKeyOwnerFn (MakeResolver (rec));

        (void) host->OnKeyDown ('A', 0);

        Assert::AreEqual (1, rec.calls, L"OnKeyDown must route through the classifier");
        Assert::AreEqual<WPARAM> ('A', rec.lastVk);
    }


    TEST_METHOD (AChromeOwnedKeyArmsTheSwallow_AGuestKeyDoesNot)
    {
        std::unique_ptr<TestShell>  shell = std::make_unique<TestShell>();
        IDxuiHostClient *           host  = shell.get();
        Recorder                    rec;


        shell->SetKeyOwnerFn (MakeResolver (rec));

        // A key the chrome took. Which owner does not matter here -- that the
        // arming follows from the verdict is ShellKeyRoutingTests' business;
        // this is that OnKeyDown does the arming at all.
        rec.verdict = ShellKeyOwner::Toolbar;
        (void) host->OnKeyDown ('A', 0);
        Assert::IsTrue (shell->IsSwallowArmed(), L"a chrome-owned key must drop its character");

        // And a key that was never the chrome's leaves it disarmed, so the
        // guest's own typing is not eaten.
        rec.verdict = ShellKeyOwner::Guest;
        (void) host->OnKeyDown ('A', 0);
        Assert::IsFalse (shell->IsSwallowArmed());
    }


    TEST_METHOD (AnArmedSwallowKeepsTheCharacterFromTheGuest)
    {
        std::unique_ptr<TestShell>  shell    = std::make_unique<TestShell>();
        IDxuiHostClient *           host     = shell.get();
        StubViewport                viewport;
        AppleKeyboard               keyboard;


        shell->SetViewport (&viewport);
        shell->GetMachine().GetRefs().keyboard = &keyboard;

        shell->ArmSwallow (true);
        (void) host->OnChar (L'A', 0);

        Assert::AreEqual (0, viewport.chars, L"the swallowed character must not reach the guest");
        Assert::IsFalse (shell->IsSwallowArmed(), L"and the swallow is one shot");
    }


    TEST_METHOD (AGuestCharacterStillReachesTheGuest)
    {
        std::unique_ptr<TestShell>  shell    = std::make_unique<TestShell>();
        IDxuiHostClient *           host     = shell.get();
        StubViewport                viewport;
        AppleKeyboard               keyboard;


        shell->SetViewport (&viewport);
        shell->GetMachine().GetRefs().keyboard = &keyboard;

        shell->ArmSwallow (false);
        (void) host->OnChar (L'A', 0);

        // The other half of the pair. Without this, a handler that swallowed
        // EVERY character would pass the test above and type nothing at all.
        Assert::AreEqual (1, viewport.chars);
        Assert::AreEqual<WPARAM> (L'A', viewport.lastChar);
    }


    TEST_METHOD (AChromeOwnedKeyDoesNotReachTheGuestAsAKeyEither)
    {
        std::unique_ptr<TestShell>  shell = std::make_unique<TestShell>();
        IDxuiHostClient *           host  = shell.get();
        StubViewport                viewport;
        AppleKeyboard               keyboard;
        Recorder                    rec;


        shell->SetViewport (&viewport);
        shell->GetMachine().GetRefs().keyboard = &keyboard;
        shell->SetKeyOwnerFn (MakeResolver (rec));

        rec.verdict = ShellKeyOwner::Toolbar;
        (void) host->OnKeyDown ('A', 0);

        Assert::AreEqual (0, viewport.keys, L"a chrome-owned keydown bails before the viewport");

        rec.verdict = ShellKeyOwner::Guest;
        (void) host->OnKeyDown ('A', 0);

        Assert::AreEqual (1, viewport.keys, L"and a guest keydown goes through");
    }
};
