#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTheme.h"
#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





//  The line height the mock reports for any string it has no canned size for.
static constexpr float  s_kLineHeight = 16.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuCueTests
//
//  The access-key underline sits just under its letter's baseline, inside
//  the text line rather than at its bottom, and turning the cues on reaches
//  a submenu that is already open.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiMenuCueTests)
{
public:

    TEST_METHOD (TheUnderlineSitsInsideTheTextLine)
    {
        auto                            file    = std::make_shared<DxuiCommand>();
        std::vector<DxuiPopupMenuItem>  rows;
        DxuiPopupMenu                   menu;
        MockDxuiTextRenderer            text;
        MockDxuiPainter                 painter;
        MockDxuiTheme                   theme;
        RECT                            anchor  = { 100, 100, 140, 130 };
        float                           textY   = -1.0f;
        float                           lineY   = -1.0f;

        file->id    = 1;
        file->label = L"&File";

        rows.push_back (DxuiPopupMenuItem::ForCommand (file));

        menu.ShowUnder (anchor, std::move (rows), text, RECT {});
        menu.SetTheme  (&theme);
        menu.SetShowMnemonicCues (true);
        text.Reset();
        menu.Paint (painter, text);

        textY = FindLabelTop (text, L"File");
        lineY = FindUnderlineTop (painter);

        Assert::IsTrue (textY >= 0.0f, L"the label was drawn");
        Assert::IsTrue (lineY >= 0.0f, L"the underline was drawn");
        Assert::IsTrue (lineY < textY + s_kLineHeight - 1.0f, L"the underline sits above the line's bottom");
        Assert::IsTrue (lineY > textY + s_kLineHeight / 2.0f, L"the underline sits below the letter's middle");
    }



    TEST_METHOD (TurningCuesOnReachesAnOpenSubmenu)
    {
        auto                            plain   = std::make_shared<DxuiCommand>();
        auto                            sub     = std::make_shared<DxuiCommand>();
        auto                            child   = std::make_shared<DxuiCommand>();
        std::vector<DxuiPopupMenuItem>  kids;
        std::vector<DxuiPopupMenuItem>  rows;
        DxuiPopupMenu                   menu;
        MockDxuiTextRenderer            text;
        MockDxuiPainter                 painter;
        MockDxuiTheme                   theme;
        RECT                            anchor  = { 100, 100, 140, 130 };

        plain->id    = 1;
        plain->label = L"&Plain";
        sub->id      = 2;
        sub->label   = L"&Group by";
        child->id    = 3;
        child->label = L"&Bytes";

        kids.push_back (DxuiPopupMenuItem::ForCommand (child));
        rows.push_back (DxuiPopupMenuItem::ForCommand (plain));
        rows.push_back (DxuiPopupMenuItem::ForSubmenu (sub, std::move (kids)));

        menu.ShowUnder (anchor, std::move (rows), text, RECT {});
        menu.SetTheme  (&theme);
        menu.SetHighlight (1);
        menu.OnKey (VK_RIGHT);

        Assert::IsTrue (menu.HasOpenChild(), L"the submenu opened");

        menu.SetShowMnemonicCues (true);
        menu.GetChild()->Paint (painter, text);

        Assert::IsTrue (FindUnderlineTop (painter) >= 0.0f, L"the open submenu underlines its access key");
    }



private:

    static float FindLabelTop (const MockDxuiTextRenderer & text, const wchar_t * label)
    {
        float  top = -1.0f;



        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == label)
            {
                top = call.y;
            }
        }

        return top;
    }



    static float FindUnderlineTop (const MockDxuiPainter & painter)
    {
        float  top = -1.0f;



        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && call.height == 1.0f && call.width > 0.0f && call.width < 20.0f)
            {
                top = call.y;
            }
        }

        return top;
    }
};
