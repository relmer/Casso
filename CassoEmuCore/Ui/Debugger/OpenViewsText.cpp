#include "Pch.h"

#include "Ui/Debugger/OpenViewsText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OpenViewsText::FormatOpenViews
//
////////////////////////////////////////////////////////////////////////////////

std::string OpenViewsText::FormatOpenViews (const DebuggerViewSnapshot & snapshot)
{
    std::string  text;



    for (int view = 1; view < kMaxCodeViews; view++)
    {
        const std::vector<DebuggerViewSnapshot::CodeLine> & lines = snapshot.codeViews[(size_t) view];

        //  The TOP line, which the view reopens on exactly. The middle moves
        //  whenever the pane's height does, so saving it would drift.
        if (snapshot.codeOpen[(size_t) view] && !lines.empty())
        {
            text += std::format (" code{}={:04X}", view + 1, lines.front().address);
        }
    }

    if (snapshot.followView != 0)
    {
        text += std::format (" follow={}", snapshot.followView + 1);
    }

    for (const DebuggerViewSnapshot::MemoryWindow & window : snapshot.memoryWindows)
    {
        if (window.id >= 2)
        {
            text += std::format (" memory{}={:04X}", window.id, window.first);
        }
    }

    for (const DebuggerViewSnapshot::PanelInfo & panel : snapshot.panels)
    {
        if (panel.open)
        {
            text += " panel=" + panel.id;
        }
    }

    return text.empty() ? text : text.substr (1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenViewsText::ParseOpenViews
//
//  Anything this build does not recognize is passed over rather than
//  failing the rest, so a view a later build adds does not cost the others.
//
////////////////////////////////////////////////////////////////////////////////

OpenViewsText::OpenViews OpenViewsText::ParseOpenViews (const std::string & text)
{
    OpenViews           views;
    std::istringstream  in (text);
    std::string         token;



    while (in >> token)
    {
        size_t       equals = token.find ('=');
        std::string  name   = token.substr (0, equals);
        std::string  value  = (equals == std::string::npos) ? std::string() : token.substr (equals + 1);
        unsigned     number = 0;
        bool         isHex  = !value.empty() &&
                              std::from_chars (value.data(), value.data() + value.size(), number, 16).ptr == value.data() + value.size();

        if (name == "panel" && !value.empty())
        {
            views.panels.push_back (value);
        }
        else if (name == "follow" && isHex && number >= 1 && number <= (unsigned) kMaxCodeViews)
        {
            views.follow = (int) number - 1;
        }
        else if (name.size() == 5 && name.starts_with ("code") && isHex && number <= 0xFFFF)
        {
            int  view = name[4] - '1';

            if (view >= 1 && view < kMaxCodeViews)
            {
                views.code[(size_t) view] = (Word) number;
            }
        }
        else if (name.size() == 7 && name.starts_with ("memory") && isHex && number <= 0xFFFF)
        {
            int  window = name[6] - '0';

            if (window >= 2 && window <= kMaxMemoryWindows)
            {
                views.memory[(size_t) (window - 1)] = (Word) number;
            }
        }
    }

    return views;
}
