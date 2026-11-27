#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkHexSource
//
//  A run of bytes of a given length.
//
////////////////////////////////////////////////////////////////////////////////

class LinkHexSource : public IDxuiHexSource
{
public:
    explicit LinkHexSource (uint64_t count) : m_count (count) {}

    uint64_t  GetByteCount() const override { return m_count; }

    void  ReadBytes (uint64_t offset, std::span<uint8_t> out) const override
    {
        for (size_t idx = 0; idx < out.size(); idx++)
        {
            out[idx] = (uint8_t) ((offset + idx) & 0xFF);
        }
    }

private:
    uint64_t  m_count = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewLinkTests
//
//  Two linked views: a setting changed from either view's menu is copied
//  to the other, and scrolling either scrolls the other, clamped to its
//  rows. Cells are 8 by 16 at 96 DPI, sixteen bytes a row.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiHexViewLinkTests)
{
public:

    static void  LayOut (DxuiHexView & view)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        view.SetCellSizeDip (8, 16);
        view.Layout (RECT { 0, 0, 800, 160 }, scaler);
    }


    //  Runs the command in a view's menu with this label, in a submenu or not.
    static bool  Choose (DxuiHexView & view, const std::wstring & label)
    {
        std::vector<DxuiPopupMenuItem>  items;



        view.BuildContextMenu (items);
        return Choose (items, label);
    }


    static bool  Choose (const std::vector<DxuiPopupMenuItem> & items, const std::wstring & label)
    {
        for (const DxuiPopupMenuItem & item : items)
        {
            if (item.command != nullptr && item.kind == DxuiPopupMenuItem::Kind::Command && item.command->label == label)
            {
                item.command->dispatch();
                return true;
            }

            if (Choose (item.children, label))
            {
                return true;
            }
        }

        return false;
    }


    TEST_METHOD (AMenuChoiceInEitherViewIsCopiedToTheOther)
    {
        LinkHexSource    source (1024);
        DxuiHexView      first;
        DxuiHexView      second;
        DxuiHexViewLink  link;
        int              told = 0;



        first.SetSource  (&source);
        second.SetSource (&source);
        LayOut (first);
        LayOut (second);
        link.Link (first, second);
        link.SetOnSettingsChanged ([&told] () { told++; });

        Assert::IsTrue (Choose (first, L"&2-byte integer"));
        Assert::AreEqual (2, second.GetGrouping());

        Assert::IsTrue (Choose (second, L"&Signed"));
        Assert::IsTrue (first.GetValueFormat() == DxuiHexViewTypes::ValueFormat::Signed);

        Assert::IsTrue (Choose (second, L"&8"));
        Assert::AreEqual (8, first.GetColumns());

        Assert::IsTrue (Choose (first, L"Show &text only"));
        Assert::IsFalse (second.IsShowingValues());

        Assert::AreEqual (4, told, L"the host hears of each change once");
    }


    TEST_METHOD (ScrollingEitherViewScrollsTheOther)
    {
        LinkHexSource    source (1024);
        LinkHexSource    shorter (256);
        DxuiHexView      first;
        DxuiHexView      second;
        DxuiHexViewLink  link;



        first.SetSource  (&source);
        second.SetSource (&shorter);
        LayOut (first);
        LayOut (second);
        link.Link (first, second);

        first.SetTopRow (5);
        Assert::AreEqual ((uint64_t) 5, second.GetTopRow());

        second.SetTopRow (2);
        Assert::AreEqual ((uint64_t) 2, first.GetTopRow());

        first.SetTopRow (first.GetMaxTopRow());
        Assert::AreEqual (second.GetMaxTopRow(), second.GetTopRow(), L"the shorter view stops at its last rows");
    }
};