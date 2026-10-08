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



struct DxuiVectorIcon;
struct DxuiIconImage;



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

    //  Drawn beside the label in a menu, as Explorer's View menu draws each
    //  view's icon. Absent for most rows, and then no column is kept for it.
    const DxuiVectorIcon         * vectorIcon   = nullptr;

    //  An icon font glyph drawn beside the label in a menu, as Explorer's
    //  context menu draws its rows' icons, for a row with no vector icon.
    //  Separate from `glyph`, which a toolbar draws, so a command a toolbar
    //  shows does not gain an icon in every menu too.
    const wchar_t                * menuGlyph    = nullptr;

    //  An SVG icon drawn in that place instead, when the renderer can draw
    //  one; the glyph stands in when it cannot. The text is the host's, and
    //  must outlive the command.
    const std::string            * menuSvg      = nullptr;

    //  A full-color image drawn there in place of either, as Explorer draws
    //  the icon of the program that opens an item beside its Open row.
    std::shared_ptr<const DxuiIconImage>  menuImage;

    //  One of a group where one is chosen, as Explorer's Sort menu's rows are:
    //  checked, it is marked with a dot rather than a check.
    bool                           radio        = false;

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
};
