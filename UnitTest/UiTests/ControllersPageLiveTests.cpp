#include "Pch.h"

#include "Ui/Settings/ControllersPage.h"
#include "Ui/Settings/ControllersPageState.h"

// A ControllersPage holds every control on the page, about 16 KB, and each
// test here builds one in the test frame, which trips C6262. The page is the
// system under test -- suppress for this file.
#pragma warning (disable: 6262)

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllersPageLiveTests
//
//  The Controllers page's live views: each button's light and the stick, fed
//  from the controller the page shows. A press that comes and goes between
//  two of the page's polls still lights its button, a lit button stays lit
//  long enough to see, and the page polls at display rate only while it is
//  shown.
//
//  Every page here has its animation setting pinned, since the CI runner
//  reports animations off and a desktop usually reports them on.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ControllersPageLiveTests)
{
public:

    static constexpr UINT     kDpi         = 96;
    static constexpr int64_t  kStartMs     = 1000;
    static constexpr int64_t  kFrameMs     = 16;
    static constexpr size_t   kPb0         = 0;
    static constexpr int      kPb0Button   = 0;     // the Default profile's PB0
    static constexpr UINT     kFrameRateMs = 17;    // a poll at least this often keeps up with a 60 Hz display
    static constexpr LONG     kPageRight   = 760;
    static constexpr LONG     kPageBottom  = 600;


    //  A stick with two axes and three buttons.
    static ControllerDeviceInfo MakeStick()
    {
        ControllerDeviceInfo  info;



        info.formFactor  = ControllerFormFactor::Joystick;
        info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
        info.unit.unitId = "{STICK}";
        info.unit.source = ControllerUnitSource::InstanceGuid;
        info.description = L"VKBsim Gladiator";
        info.controls    = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 },
                             { ControlKind::Button, 0 }, { ControlKind::Button, 1 }, { ControlKind::Button, 2 } };
        return info;
    }


    //  A reading with the stick centered and PB0's button up or down.
    static ControllerSample MakeSample (bool isPressed)
    {
        ControllerSample  sample;



        sample.connected = true;
        sample.buttons.set (kPb0Button, isPressed);
        return sample;
    }


    //  What the page reads: the latest reading, and every reading since the
    //  page last took them.
    struct FakeReadings
    {
        ControllerSample               current = MakeSample (false);
        std::vector<ControllerSample>  pending;
        int                            reads   = 0;
        std::vector<bool>              inspects;
    };


    //  Lays out a page editing one stick, fed from `readings`, with its
    //  animation setting pinned.
    static void SetUpPage (ControllersPage & page, ControllersPageState & state, FakeReadings & readings, bool isAnimated)
    {
        DxuiDpiScaler  scaler;



        state.Load ({ MakeStick() }, {}, {}, true);
        state.SelectController (0);

        page.SetState             (&state);
        page.SetAnimationsEnabled (isAnimated);
        page.SetVisible           (true);

        page.SetSampleSource ([&readings] (const ControllerUnitKey &) -> std::optional<ControllerSample>
        {
            readings.reads++;
            return readings.current;
        });

        page.SetHistorySource ([&readings] (const ControllerUnitKey &)
        {
            std::vector<ControllerSample>  taken;



            taken.swap (readings.pending);
            return taken;
        });

        page.SetOnInspect ([&readings] (const std::optional<ControllerUnitKey> & unit)
        {
            readings.inspects.push_back (unit.has_value());
        });

        scaler.SetDpi (kDpi);
        page.Layout   (RECT { 0, 0, kPageRight, kPageBottom }, scaler);
    }


    //  PB0's light.
    static const ButtonLightView & GetPb0Light (const ControllersPage & page)
    {
        const ButtonLightView  * light = nullptr;
        size_t                   i     = 0;



        for (i = 0; i < page.GetChildCount() && light == nullptr; ++i)
        {
            light = dynamic_cast<const ButtonLightView *> (page.GetChild (i));
        }

        Assert::IsNotNull (light, L"the page has a light for PB0");
        return *light;
    }


    //  A press and release between two polls, with the button up at both,
    //  still lights the circle.
    TEST_METHOD (QuickPress_BetweenTwoPolls_LightsTheCircle)
    {
        ControllersPage       page;
        ControllersPageState  state;
        FakeReadings          readings;



        SetUpPage (page, state, readings, false);

        page.Poll (kStartMs);
        Assert::IsFalse (GetPb0Light (page).IsLit(), L"dark at rest");

        readings.pending = { MakeSample (false), MakeSample (true), MakeSample (false) };
        page.Poll (kStartMs + kFrameMs);

        Assert::IsTrue (GetPb0Light (page).IsLit(), L"lit though the button was up at both polls");
    }


    //  A press seen once stays fully lit for the minimum time, then goes
    //  dark: at once with animations off, after a short fade with them on.
    TEST_METHOD (QuickPress_StaysLitForTheMinimumTime_ThenGoesDark)
    {
        constexpr int64_t     kPressMs = kStartMs + kFrameMs;
        constexpr int64_t     kHoldEnd = kPressMs + ButtonLightView::kMinLitMs;
        ControllersPage       still;
        ControllersPage       animated;
        ControllersPageState  stillState;
        ControllersPageState  animatedState;
        FakeReadings          stillReadings;
        FakeReadings          animatedReadings;



        SetUpPage (still,    stillState,    stillReadings,    false);
        SetUpPage (animated, animatedState, animatedReadings, true);

        stillReadings.pending    = { MakeSample (true) };
        animatedReadings.pending = { MakeSample (true) };
        still.Poll    (kPressMs);
        animated.Poll (kPressMs);

        still.Poll    (kHoldEnd - 1);
        animated.Poll (kHoldEnd - 1);
        Assert::AreEqual (1.0f, GetPb0Light (still).GetLevel(),    L"animations off: fully lit through the minimum time");
        Assert::AreEqual (1.0f, GetPb0Light (animated).GetLevel(), L"animations on: the same");

        still.Poll    (kHoldEnd + 1);
        animated.Poll (kHoldEnd + ButtonLightView::kFadeMs / 2);
        Assert::AreEqual (0.0f, GetPb0Light (still).GetLevel(), L"animations off: dark right after it");
        Assert::IsTrue   (GetPb0Light (animated).GetLevel() > 0.0f && GetPb0Light (animated).GetLevel() < 1.0f, L"animations on: fading");

        animated.Poll (kHoldEnd + ButtonLightView::kFadeMs + 1);
        Assert::AreEqual (0.0f, GetPb0Light (animated).GetLevel(), L"and dark once the fade is done");
    }


    //  A button held down stays fully lit for as long as it is held.
    TEST_METHOD (HeldButton_StaysLitWhileHeld)
    {
        constexpr int64_t     kHeldMs  = 500;
        ControllersPage       page;
        ControllersPageState  state;
        FakeReadings          readings;
        int64_t               nowMs    = kStartMs;



        SetUpPage (page, state, readings, true);
        readings.current = MakeSample (true);

        for (nowMs = kStartMs; nowMs < kStartMs + kHeldMs; nowMs += kFrameMs)
        {
            page.Poll (nowMs);
        }

        Assert::AreEqual (1.0f, GetPb0Light (page).GetLevel(), L"fully lit while held");
    }


    //  With animations on a press shows at once, part lit, and reaches full
    //  within the ramp; with them off it is fully lit at once.
    TEST_METHOD (Press_AnimatesInQuickly)
    {
        ControllersPage       still;
        ControllersPage       animated;
        ControllersPageState  stillState;
        ControllersPageState  animatedState;
        FakeReadings          stillReadings;
        FakeReadings          animatedReadings;
        float                 first            = 0.0f;



        SetUpPage (still,    stillState,    stillReadings,    false);
        SetUpPage (animated, animatedState, animatedReadings, true);
        stillReadings.current    = MakeSample (true);
        animatedReadings.current = MakeSample (true);

        still.Poll    (kStartMs);
        animated.Poll (kStartMs);
        first = GetPb0Light (animated).GetLevel();

        Assert::AreEqual (1.0f, GetPb0Light (still).GetLevel(), L"animations off: fully lit at once");
        Assert::IsTrue   (first >= ButtonLightView::kPressStartLevel && first < 1.0f, L"animations on: visibly lit at once, and still rising");

        animated.Poll (kStartMs + ButtonLightView::kPressRampMs);
        Assert::AreEqual (1.0f, GetPb0Light (animated).GetLevel(), L"and full by the end of the ramp");
    }


    //  While the page is shown it asks to be polled at display rate; hidden,
    //  it asks for no polls, reads nothing and lets the controller go, so the
    //  service stops reading it.
    TEST_METHOD (HiddenPage_ReadsNothingAndReleasesTheController)
    {
        ControllersPage       page;
        ControllersPageState  state;
        FakeReadings          readings;



        SetUpPage (page, state, readings, false);
        page.Poll (kStartMs);

        Assert::IsTrue (page.GetPollIntervalMs() > 0 && page.GetPollIntervalMs() <= kFrameRateMs, L"shown: polled at display rate");
        Assert::IsTrue (readings.reads > 0,                                                       L"and reads the controller");
        Assert::IsTrue (!readings.inspects.empty() && readings.inspects.back(),                   L"which it asked for");

        readings.reads = 0;
        page.SetVisible (false);
        page.Poll (kStartMs + kFrameMs);

        Assert::AreEqual (0u, page.GetPollIntervalMs(),  L"hidden: no polls wanted");
        Assert::AreEqual (0,  readings.reads,            L"a poll reads nothing");
        Assert::IsFalse  (readings.inspects.back(),      L"and the controller is let go");
    }

    //  The messages beside a lit light come from one table, every entry with
    //  text, and a pick never repeats the one before it.
    TEST_METHOD (FunMessages_PickNeverRepeatsThePrevious)
    {
        constexpr uint32_t  kRandomsPerCase = 200;
        size_t              count           = ButtonLightView::GetFunMessageCount();
        size_t              previous        = 0;
        size_t              picked          = 0;
        uint32_t            random          = 0;



        Assert::AreEqual ((size_t) 37, count, L"the owner's list");

        for (previous = 0; previous < count; previous++)
        {
            Assert::IsTrue (wcslen (ButtonLightView::GetFunMessageAt (previous)) > 0, L"every entry has text");

            for (random = 0; random < kRandomsPerCase; random++)
            {
                picked = ButtonLightView::PickFunMessage (previous, random);
                Assert::IsTrue (picked < count,     L"in the table");
                Assert::IsTrue (picked != previous, L"never the previous one");
            }
        }

        Assert::IsTrue (ButtonLightView::PickFunMessage (std::nullopt, 0) < count, L"the first pick is in the table");
    }


    //  A press shows a message beside its light while the light is lit; the
    //  message goes when the light goes dark, and the next press shows another.
    TEST_METHOD (Press_ShowsAMessageUntilTheLightGoesDark)
    {
        constexpr int64_t  kPressMs  = kStartMs;
        constexpr int64_t  kDarkMs   = kPressMs + ButtonLightView::kMinLitMs + 1;
        constexpr int64_t  kSecondMs = kDarkMs + kFrameMs;
        ButtonLightView    light;
        std::wstring       first;



        light.SetAnimationsEnabled (false);
        Assert::IsTrue (light.GetFunMessage().empty(), L"no message at rest");

        light.Update (true, false, kPressMs);
        first = light.GetFunMessage();
        Assert::IsFalse (first.empty(), L"a message while lit");

        light.Update (false, false, kDarkMs);
        Assert::IsFalse (light.IsLit(),               L"dark");
        Assert::IsTrue  (light.GetFunMessage().empty(), L"and the message gone");

        light.Update (true, false, kSecondMs);
        Assert::IsFalse (light.GetFunMessage().empty(), L"a message for the next press");
        Assert::AreNotEqual (first, light.GetFunMessage(), L"not the one before");

        light.Clear();
        Assert::IsTrue (light.GetFunMessage().empty(), L"cleared with the light");
    }
};
