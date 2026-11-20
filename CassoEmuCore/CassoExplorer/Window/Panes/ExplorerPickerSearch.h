#pragma once

#include "Pch.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch
//
//  A picker's search box above the file list. Each keystroke or paste
//  filters the list to the files whose name contains every word typed,
//  within the picker's scope and the tree node on display. In a window that is not a
//  picker it has no control and its band has no height.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPickerSearch
{
public:
    explicit ExplorerPickerSearch (ExplorerContext & context);

    static constexpr int  kBandDip     = 48;
    static constexpr int  kBoxDip      = 34;
    static constexpr int  kEdgePadDip  = 12;

    void  Create ();

    //  The box at the top of the rect, and the rect below it.
    RECT  Layout (const RECT & rect);

    //  Puts the focus in the box with its text selected.
    void  Focus  ();

    std::wstring  GetText () const;
    bool          HasBox  () const { return m_box != nullptr; }

    bool  HandleMouse        (const DxuiMouseEvent & ev);
    bool  HandleKey          (const DxuiKeyEvent & ev);
    void  HandleFocusChanged (bool isFocused);
    bool  IsCaretOn          () const;

private:
    void  ApplyFilter (const std::wstring & text);
    void  MoveToList  ();

    ExplorerContext  & m_context;
    DxuiTextInput    * m_box          = nullptr;
    bool               m_isConfigured = false;
};
