#include "Pch.h"

#include "Ui/Settings/DiskPage.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Settings/SettingsPanelState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StoragePageTests
//
//  The Storage page's two sections: disk drives first, then a rule running
//  margin to margin, then cassette tape.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StoragePageTests)
{
public:

    static constexpr UINT  kDpi    = 96;
    static constexpr int   kTop    = 10;
    static constexpr int   kBottom = 900;


    template <typename T>
    static const T * FindChild (const DiskPage & page, const wchar_t * pszText = nullptr)
    {
        for (size_t i = 0; i < page.GetChildCount(); ++i)
        {
            const T  * child = dynamic_cast<const T *> (page.GetChild (i));

            if (child == nullptr)
            {
                continue;
            }

            if constexpr (std::is_same_v<T, DxuiLabel>)
            {
                if (child->GetText() != pszText)
                {
                    continue;
                }
            }

            return child;
        }

        return nullptr;
    }


    template <typename T>
    static const T * FindOnRow (const DiskPage & page, const RECT & row)
    {
        for (size_t i = 0; i < page.GetChildCount(); ++i)
        {
            const T  * child   = dynamic_cast<const T *> (page.GetChild (i));
            int        centerY = 0;

            if (child == nullptr)
            {
                continue;
            }

            centerY = (child->GetBounds().top + child->GetBounds().bottom) / 2;

            if (centerY >= row.top && centerY < row.bottom)
            {
                return child;
            }
        }

        return nullptr;
    }


    // The tip's glyph starts just past the end of its label's text, on the
    // label's line, and ends before the controls start.
    static void AssertTipFollowsItsText (const DiskPage & page, const wchar_t * pszLabel, int textWidthPx)
    {
        constexpr int        kGlyphPx    = 14;
        constexpr int        kNearPx     = 8;
        constexpr int        kLinePx     = 3;
        const DxuiLabel    * label       = FindChild<DxuiLabel> (page, pszLabel);
        const DxuiInfoTip  * tip         = nullptr;
        RECT                 row         = {};
        RECT                 tipBounds   = {};
        int                  textEnd     = 0;
        int                  glyphLeft   = 0;
        int                  controlsX   = page.GetFastTapeToggle().GetRect().left;



        Assert::IsNotNull (label);

        row       = label->GetRect();
        tip       = FindOnRow<DxuiInfoTip> (page, row);
        Assert::IsNotNull (tip);

        tipBounds = tip->GetBounds();
        textEnd   = row.left + textWidthPx;
        glyphLeft = (tipBounds.left + tipBounds.right) / 2 - kGlyphPx / 2;

        Assert::IsTrue (glyphLeft >= textEnd,                      L"the tip follows the label's text");
        Assert::IsTrue (glyphLeft - textEnd <= kNearPx,            L"right after it, not at the column's edge");
        Assert::IsTrue (glyphLeft + kGlyphPx <= controlsX,         L"and ends before the controls start");
        Assert::IsTrue (std::abs ((tipBounds.top + tipBounds.bottom) / 2 - (row.top + row.bottom) / 2) <= kLinePx,
                        L"on the label's line");
    }

    static void LayOut (DiskPage & page, int left, int right)
    {
        DxuiDpiScaler  scaler;



        scaler.SetDpi (kDpi);
        page.Layout (RECT { left, kTop, right, kBottom }, scaler);
    }


    TEST_METHOD (RuleRunsMarginToMarginAndFollowsAResize)
    {
        constexpr int        kLeft   = 20;
        constexpr int        kNarrow = 500;
        constexpr int        kWide   = 900;
        DiskPage             page (L"Storage");
        const DxuiDivider  * rule    = nullptr;
        int                  margin  = 0;



        LayOut (page, kLeft, kNarrow);
        rule = FindChild<DxuiDivider> (page);
        Assert::IsNotNull (rule);

        margin = rule->GetBounds().left - kLeft;
        Assert::IsTrue   (margin > 0, L"the rule starts at the page margin, not the edge");
        Assert::AreEqual (kNarrow - margin, (int) rule->GetBounds().right, L"and ends at the opposite margin");

        LayOut (page, kLeft, kWide);
        Assert::AreEqual (kLeft + margin, (int) rule->GetBounds().left);
        Assert::AreEqual (kWide - margin, (int) rule->GetBounds().right, L"a wider page carries the rule to its new margin");
    }


    TEST_METHOD (DisksComeFirstThenTheRuleThenTape)
    {
        DiskPage             page (L"Storage");
        const DxuiLabel    * disks     = nullptr;
        const DxuiLabel    * protect   = nullptr;
        const DxuiLabel    * defaults  = nullptr;
        const DxuiDivider  * rule      = nullptr;
        const DxuiLabel    * tape      = nullptr;
        const DxuiLabel    * fastTape  = nullptr;



        LayOut (page, 0, 600);
        disks    = FindChild<DxuiLabel> (page, L"Disk drives");
        protect  = FindChild<DxuiLabel> (page, L"Write protect:");
        defaults = FindChild<DxuiLabel> (page, L"Drive 2 pan:");
        rule     = FindChild<DxuiDivider> (page);
        tape     = FindChild<DxuiLabel> (page, L"Cassette tape");
        fastTape = FindChild<DxuiLabel> (page, L"Fast tape loading:");

        Assert::IsNotNull (disks);
        Assert::IsNotNull (protect);
        Assert::IsNotNull (defaults);
        Assert::IsNotNull (rule);
        Assert::IsNotNull (tape);
        Assert::IsNotNull (fastTape);

        Assert::IsTrue (disks->GetRect().bottom    <= protect->GetRect().top);
        Assert::IsTrue (defaults->GetRect().bottom <= rule->GetBounds().top, L"every disk row sits above the rule");
        Assert::IsTrue (rule->GetBounds().bottom   <= tape->GetRect().top);
        Assert::IsTrue (tape->GetRect().bottom     <= fastTape->GetRect().top);
    }


    TEST_METHOD (ControlsClearTheWidestLabelAndTipsFollowTheirText)
    {
        constexpr int           kWidestPx   = 220;
        constexpr int           kWavTextPx  = 130;
        constexpr int           kLinePx     = 16;
        DiskPage                page (L"Storage");
        MockDxuiTextRenderer    text;
        const DxuiLabel       * idleStop    = nullptr;



        text.SetCannedMetrics (L"Stop when loading ends:", SIZE { kWidestPx,  kLinePx });
        text.SetCannedMetrics (L"New tape WAV format:",    SIZE { kWavTextPx, kLinePx });
        page.SetTextRenderer  (&text);

        LayOut (page, 0, 800);
        idleStop = FindChild<DxuiLabel> (page, L"Stop when loading ends:");

        Assert::IsNotNull (idleStop);
        Assert::IsTrue    (idleStop->GetRect().left + kWidestPx < page.GetFastTapeToggle().GetRect().left,
                           L"every control starts past the widest label, so none wraps");

        AssertTipFollowsItsText (page, L"New tape WAV format:", kWavTextPx);
    }


    TEST_METHOD (RestoreDefaultsResetsTheWholePageTapeIncluded)
    {
        DiskPage                page (L"Storage");
        SettingsPanelState      state;
        const SettingsUiPrefs   defaults;



        page.SetState (&state);
        state.SetFastTapeLoading  (!defaults.fastTapeLoading);
        state.SetTapeVolume       (0.25f);
        state.SetTapeAutoStop     (!defaults.tapeAutoStop);
        state.SetWriteProtect     (0, !defaults.writeProtect[0]);
        state.SetFloppySound      (!defaults.floppySoundEnabled);
        state.SetDriveMotorVolume (0.1f);

        LayOut (page, 0, 600);
        page.GetRestoreDefaultsButton().Click();

        Assert::AreEqual (defaults.fastTapeLoading,    state.GetPrefs().fastTapeLoading);
        Assert::AreEqual (defaults.tapeVolume,         state.GetPrefs().tapeVolume);
        Assert::AreEqual (defaults.tapeAutoStop,       state.GetPrefs().tapeAutoStop);
        Assert::AreEqual (defaults.writeProtect[0],    state.GetPrefs().writeProtect[0]);
        Assert::AreEqual (defaults.floppySoundEnabled, state.GetPrefs().floppySoundEnabled);
        Assert::AreEqual (defaults.driveMotorVolume,   state.GetPrefs().driveMotorVolume);
        Assert::AreEqual (defaults.fastTapeLoading,    page.GetFastTapeToggle().IsChecked(), L"and the widgets show it");
    }


    TEST_METHOD (RestoreDefaultsSitsBelowTheTapeSection)
    {
        DiskPage           page (L"Storage");
        const DxuiLabel  * autoStop = nullptr;



        LayOut (page, 0, 600);
        autoStop = FindChild<DxuiLabel> (page, L"Stop at end of tape:");

        Assert::IsNotNull (autoStop);
        Assert::IsTrue    (autoStop->GetRect().bottom <= page.GetRestoreDefaultsButton().GetBounds().top,
                           L"it restores the whole page, so it follows both sections");
    }
};