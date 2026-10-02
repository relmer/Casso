#include "Pch.h"

#include "Ui/Settings/DiskPage.h"

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
};
