#include "Pch.h"

#include "Ui/Debugger/MemoryBarCommands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ShrinkableEntry
//
//  A custom entry that can give up width down to a minimum, as a text box
//  can.
//
////////////////////////////////////////////////////////////////////////////////

class ShrinkableEntry : public IDxuiToolbarCustomEntry
{
public:
    ShrinkableEntry (int widthPx, int minPx) : m_widthPx (widthPx), m_minPx (minPx) {}

    int              GetWidthPx    (bool, const DxuiDpiScaler &, IDxuiTextRenderer *) const override { return m_widthPx; }
    int              GetMinWidthPx (const DxuiDpiScaler &) const                             override { return m_minPx; }
    void             Layout        (const RECT &, bool, const DxuiDpiScaler &)              override {}
    void             Paint         (IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &, bool, bool, bool) override {}
    const wchar_t *  GetTooltipAt  (int, int, RECT &) const                                 override { return nullptr; }
    bool             OnClick       (int, int)                                               override { return false; }

private:
    int  m_widthPx = 0;
    int  m_minPx   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarShrinkTests
//
//  A never-overflow entry that can shrink gives up width, down to its
//  minimum, once everything else has gone into See more.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarShrinkTests)
{
public:

    static constexpr int  kCustomId = 1;


    static std::vector<DxuiToolbar::Entry> MakeEntries (IDxuiToolbarCustomEntry * custom)
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
        entries[0].neverOverflow = true;

        return entries;
    }


    static int GetCustomWidth (int stripWidthPx, int minPx)
    {
        DxuiToolbar      bar;
        DxuiDpiScaler    scaler;
        ShrinkableEntry  custom (300, minPx);
        RECT             rc = {};

        scaler.SetDpi (96);
        bar.SetEntries (MakeEntries (&custom));
        bar.Layout     (RECT { 0, 0, stripWidthPx, 40 }, scaler);

        Assert::IsTrue (bar.TryGetEntryRect (kCustomId, rc), L"the custom entry is on the strip");

        return rc.right - rc.left;
    }


    TEST_METHOD (AShrinkableEntryNarrowsToFitTheStrip)
    {
        int  width = GetCustomWidth (200, 50);

        Assert::IsTrue (width < 200, L"the entry gives up width rather than run past the strip");
        Assert::IsTrue (width >= 50, L"but not below its minimum");
    }


    TEST_METHOD (AShrinkableEntryStopsAtItsMinimum)
    {
        Assert::AreEqual (50, GetCustomWidth (40, 50), L"the minimum holds however narrow the strip");
    }


    TEST_METHOD (AShrinkableEntryKeepsItsFullWidthWhenItFits)
    {
        Assert::AreEqual (300, GetCustomWidth (2000, 50), L"no shrink with room to spare");
    }


    TEST_METHOD (TheMemoryAddressBoxNeverOverflows)
    {
        MemoryBarCommands                commands ({});
        ShrinkableEntry                  address (180, 80);
        std::vector<DxuiToolbar::Entry>  entries = commands.BuildEntries (&address);
        bool                             found   = false;

        for (const DxuiToolbar::Entry & entry : entries)
        {
            if (entry.custom == &address)
            {
                found = true;
                Assert::IsTrue (entry.neverOverflow, L"the Address box stays on the strip");
            }
        }

        Assert::IsTrue (found, L"the bar has the Address box");
    }
};
