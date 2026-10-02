#pragma once

#include "Pch.h"

#include "Core/DxuiTextElide.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLabelFit
//
//  A cap on how wide a surface may draw a command's label, and how a label
//  over it is shortened. `keptSuffix` is never cut when the label ends with
//  it, so a marker after a changing description stays whole. A label that
//  can end in more than one marker lists the others in `keptSuffixes`; the
//  longest one the label ends with is kept.
//
//  The surface fits the label once and uses that one string both to measure
//  the entry and to paint it, so the space it reserves and the text it draws
//  cannot disagree.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiLabelFit
{
    float                      maxWidthDip = 0.0f;
    DxuiElide                  mode        = DxuiElide::Tail;
    std::wstring               keptSuffix;
    std::vector<std::wstring>  keptSuffixes;
};





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

    //  The tooltip when it moves with state, as "Undo changed 2 bytes at
    //  $0300"; absent, the static tip. Cheap and free of side effects, as the
    //  four above are.
    std::function<std::wstring()>  tipText;

    //  Run when a menu's highlight moves onto the row, by pointer or key,
    //  for a row whose effect is shown before it is chosen. Unlike the four
    //  functors above it acts, and it runs only on that move, never on paint.
    std::function<void()>          preview;

    //  Absent means the label is drawn at its full width.
    std::optional<DxuiLabelFit>    labelFit;

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

    //  The dynamic tooltip when supplied, else the static one.
    std::wstring  GetTipText   () const { return tipText ? tipText() : tip; }
};
