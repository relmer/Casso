#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCommand
//
//  One action's declaration, shared by every surface that shows it: a menu
//  row, a toolbar button, a context menu row. The application owns the object
//  and keeps it alive for as long as any surface holds a pointer to it.
//
//  A surface reads the label, glyph, tooltip, accelerator, checked and enabled
//  state through the accessors below AT PAINT AND AT CLICK TIME, never from a
//  copy taken earlier. That is what the type exists for: the separate tables it
//  replaces held their own labels and enabled rules and could disagree, and a
//  copy cached across frames reintroduces exactly that.
//
//  The four functors are optional. Absent, they mean unchecked, enabled, and
//  the static label, which is the common case and costs a caller nothing. They
//  run on every paint, so they must be cheap and free of side effects.
//
//  `id` is an int so an application's own command identifiers cast in without
//  change and its existing dispatch target keeps working.
//
////////////////////////////////////////////////////////////////////////////////



struct DxuiCommand
{
    int                            id           = 0;
    std::wstring                   label;
    std::wstring                   shortLabel;
    const wchar_t                * glyph        = nullptr;
    std::wstring                   tip;
    std::wstring                   accelerator;
    std::function<void()>          dispatch;
    std::function<bool()>          isChecked;
    std::function<bool()>          isEnabled;
    std::function<std::wstring()>  labelText;

    //  Absent means never checked.
    bool          IsChecked    () const { return isChecked ? isChecked() : false; }

    //  Absent means enabled, so a command supplies the functor only when its
    //  availability actually moves.
    bool          IsEnabled    () const { return isEnabled ? isEnabled() : true; }

    //  The dynamic text when supplied, else the static label -- so a row can
    //  quote its target and flip verbs with state without being rebuilt.
    std::wstring  GetLabelText () const { return labelText ? labelText() : label; }

    //  What a toolbar draws: the short form when there is one, else the same
    //  text a menu row would show.
    std::wstring  GetShortText () const { return shortLabel.empty() ? GetLabelText() : shortLabel; }
};
