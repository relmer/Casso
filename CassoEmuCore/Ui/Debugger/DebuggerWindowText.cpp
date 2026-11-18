#include "Pch.h"

#include "Ui/Debugger/DebuggerWindowText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindowText::GetCaption
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindowText::GetCaption (const std::wstring & titlePrefix, const std::wstring & projectLabel)
{
    std::wstring  caption = titlePrefix.empty() ? std::wstring (kpszWindowTitle) : titlePrefix + L" - " + kpszWindowTitle;



    if (!projectLabel.empty())
    {
        caption += L" - " + projectLabel;
    }

    return caption;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindowText::GetUndoLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindowText::GetUndoLabel (bool redo, const std::wstring & text)
{
    std::wstring  verb = redo ? L"Redo" : L"Undo";



    return text.empty() ? verb : verb + L" " + text;
}
