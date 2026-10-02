#include "Pch.h"

#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuPreviewTests
//
//  A row whose command carries a preview runs it each time the highlight
//  moves onto that row, by pointer or by key, and never on its own when
//  another row is highlighted.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPopupMenuPreviewTests)
{
public:

    static constexpr int  kHostWidth  = 800;
    static constexpr int  kHostHeight = 600;


    static std::shared_ptr<DxuiCommand>  MakeRow (const wchar_t * label, int & previews)
    {
        auto  command = std::make_shared<DxuiCommand>();



        command->label   = label;
        command->preview = [&previews] { previews++; };

        return command;
    }


    TEST_METHOD (HighlightingARowRunsItsPreview)
    {
        DxuiPopupMenu                   menu;
        MockDxuiTextRenderer            text;
        int                             first  = 0;
        int                             second = 0;
        std::vector<DxuiPopupMenuItem>  rows;



        rows.push_back (DxuiPopupMenuItem::ForCommand (MakeRow (L"Light", first)));
        rows.push_back (DxuiPopupMenuItem::ForCommand (MakeRow (L"Dark",  second)));

        menu.ShowAt (0, 0, std::move (rows), text, RECT { 0, 0, kHostWidth, kHostHeight });

        menu.SetHighlight (1);

        Assert::AreEqual (0, first);
        Assert::AreEqual (1, second, L"the highlighted row did not preview");

        menu.SetHighlight (0);

        Assert::AreEqual (1, first);
        Assert::AreEqual (1, second);
    }


    TEST_METHOD (RowWithoutPreviewIsHighlightedQuietly)
    {
        DxuiPopupMenu                   menu;
        MockDxuiTextRenderer            text;
        auto                            plain = std::make_shared<DxuiCommand>();
        std::vector<DxuiPopupMenuItem>  rows;



        plain->label = L"Plain";
        rows.push_back (DxuiPopupMenuItem::ForCommand (plain));

        menu.ShowAt (0, 0, std::move (rows), text, RECT { 0, 0, kHostWidth, kHostHeight });
        menu.SetHighlight (0);

        Assert::AreEqual (0, menu.GetHighlight());
    }
};
