#include "Pch.h"

#include "Widgets/DxuiToolbar.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FixedWidthEntry
//
//  A custom entry as wide as it is told, in both forms, as an address box is.
//
////////////////////////////////////////////////////////////////////////////////

class FixedWidthEntry : public IDxuiToolbarCustomEntry
{
public:
    explicit FixedWidthEntry (int widthPx) : m_widthPx (widthPx) {}

    int              GetWidthPx   (bool, const DxuiDpiScaler &, IDxuiTextRenderer *) const override { return m_widthPx; }
    void             Layout       (const RECT &, bool, const DxuiDpiScaler &)              override {}
    void             Paint        (IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &, bool, bool, bool) override {}
    const wchar_t *  GetTooltipAt (int, int, RECT &) const                                 override { return nullptr; }
    bool             OnClick      (int, int)                                               override { return false; }

private:
    int  m_widthPx = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarCustomOverflowTests
//
//  A custom entry overflows into See more like any other entry, unless it is
//  marked never-overflow.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarCustomOverflowTests)
{
public:

    static constexpr int  kCustomId = 1;


    static std::vector<DxuiToolbar::Entry> MakeEntries (IDxuiToolbarCustomEntry * custom, bool neverOverflow)
    {
        std::vector<DxuiToolbar::Entry>  entries (3);

        for (int i = 0; i < 3; i++)
        {
            auto  command = std::make_shared<DxuiCommand>();

            command->id    = i + 1;
            command->label = L"Command";
            command->glyph = L"x";

            entries[(size_t) i].command = command;
        }

        entries[0].custom        = custom;
        entries[0].neverOverflow = neverOverflow;

        return entries;
    }


    TEST_METHOD (ACustomEntryTooWideForTheStripMovesIntoSeeMore)
    {
        DxuiToolbar      bar;
        DxuiDpiScaler    scaler;
        FixedWidthEntry  custom (300);

        scaler.SetDpi (96);
        bar.SetEntries   (MakeEntries (&custom, false));
        bar.PlanForWidth (200, scaler);

        Assert::IsTrue  (bar.IsInSeeMore (kCustomId),               L"the custom entry goes into the menu");
        Assert::IsFalse (bar.IsInSeeMore (DxuiToolbar::kSeeMoreId), L"and the button shows");
    }


    TEST_METHOD (ANeverOverflowEntryStaysOnTheStrip)
    {
        DxuiToolbar      bar;
        DxuiDpiScaler    scaler;
        FixedWidthEntry  custom (300);

        scaler.SetDpi (96);
        bar.SetEntries   (MakeEntries (&custom, true));
        bar.PlanForWidth (200, scaler);

        Assert::IsFalse (bar.IsInSeeMore (kCustomId), L"a never-overflow entry stays");
        Assert::IsTrue  (bar.IsInSeeMore (3),         L"the others still go");
    }
};
