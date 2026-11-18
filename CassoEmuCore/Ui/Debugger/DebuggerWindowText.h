#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindowText
//
//  The debugger window's caption and the labels of its Undo and Redo items.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerWindowText
{
public:
    static constexpr LPCWSTR  kpszWindowTitle = L"Debugger";

    static std::wstring  GetCaption   (const std::wstring & titlePrefix, const std::wstring & projectLabel = {});
    static std::wstring  GetUndoLabel (bool redo, const std::wstring & text);
};
