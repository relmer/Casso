#pragma once

#include "Pch.h"

struct ExplorerContext;
class ExplorerPickerChips;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar
//
//  A picker's bar along the bottom of the window. An open picker has the
//  filter list when there are several filters, the create command when the
//  request has one, and the confirm and cancel buttons. A save picker lays
//  them out as the standard Save As dialog does: a labeled file name row, a
//  labeled type row, the selected type's note, the Verify box when the
//  request has one, and the buttons. In a window that is not a picker the
//  bar has no controls and its band has no height.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPickerBar
{
public:
    explicit ExplorerPickerBar (ExplorerContext & context);

    static constexpr int  kBandDip       = 52;
    static constexpr int  kButtonDip     = DxuiButtonRow::kButtonHeightDip;
    static constexpr int  kEdgePadDip    = DxuiButtonRow::kEdgePadDip;
    static constexpr int  kGapDip        = 8;
    static constexpr int  kNameMinDip    = 160;
    static constexpr int  kVerifyDip     = 90;
    static constexpr int  kNoteDip       = 24;
    static constexpr int  kSaveLabelDip  = 100;
    static constexpr int  kRowPadDip     = 12;

    //  The standard Save As dialog's text: Segoe UI at 9 points.
    static constexpr const wchar_t *  kDialogFace    = L"Segoe UI";
    static constexpr float            kDialogFontDip = 12.0f;

    //  The band's height in the window: kBandDip in a picker, and kNoteDip
    //  more when a filter entry has a note; 0 otherwise.
    int   GetBandDip () const;

    void  Create      ();
    //  The filter list starts at `listLeft`, the left edge of the list.
    void  Layout      (const RECT & band, int listLeft);

    //  The capsules of what a drive's picker lists, in the command bar's free
    //  area from the left edge of the list.
    void  LayoutChips (const RECT & freeArea, int listLeft);

    //  The confirm button follows the selection, and the filter list and its
    //  note the active entry.
    void  Update     ();

    bool  IsConfirmEnabled () const;

    //  The save form's file name box: its text, and the mouse, keys and
    //  focus the window routes to it. Enter in the box confirms.
    std::wstring  GetNameText        () const;
    void          SetNameText        (const std::wstring & text);
    bool          HandleNameMouse    (const DxuiMouseEvent & ev);
    bool          HandleNameKey      (const DxuiKeyEvent & ev);
    void          HandleFocusChanged (bool isFocused);
    bool          IsCaretOn          () const;
    bool          HasNameBox         () const { return m_name != nullptr; }

    //  Whether the Verify check box is checked; false when the bar has none.
    bool  IsVerifyChecked () const { return m_verify != nullptr && m_verify->IsChecked(); }

    //  The filter list's text at a row, headings included; empty past the end.
    std::wstring  GetFilterItemText (int row) const { return (m_filter != nullptr && row >= 0 && row < (int) m_filter->GetItems().size()) ? m_filter->GetItems()[(size_t) row] : std::wstring(); }

    //  The bar's controls as Tab stops, left to right: the Verify check box,
    //  the filter list, then the buttons. -1 clears the focus.
    int   GetButtonCount  () const;
    bool  IsButtonEnabled (int index) const;
    void  SetButtonFocus  (int index);
    bool  HandleButtonKey (int index, const DxuiKeyEvent & ev);

private:
    int                       LayoutButtons  (int top, int right);
    void                      LayoutSaveRows (const RECT & band, int listLeft);
    int                       GetSaveBoxLeft (const RECT & band, int listLeft) const;
    void                      FillFilterList ();
    bool                      HasFilterNotes () const;
    std::vector<DxuiButton *> GetButtons     () const;
    int                       GetVerifyStop  () const { return (m_verify != nullptr) ? 0 : -1; }
    int                       GetFilterStop  () const { return (m_filter != nullptr) ? ((m_verify != nullptr) ? 1 : 0) : -1; }
    int                       GetLeadStops   () const { return ((m_verify != nullptr) ? 1 : 0) + ((m_filter != nullptr) ? 1 : 0); }

    ExplorerContext      & m_context;
    ExplorerPickerChips  * m_chips            = nullptr;
    DxuiLabel            * m_nameLabel        = nullptr;
    DxuiTextInput        * m_name             = nullptr;
    DxuiCheckbox         * m_verify           = nullptr;
    DxuiLabel            * m_filterLabel      = nullptr;
    DxuiComboBox         * m_filter           = nullptr;
    DxuiLabel            * m_filterNote       = nullptr;
    std::vector<int>       m_filterRows;                 // each list row's filter entry, -1 for a heading
    DxuiButton           * m_create           = nullptr;
    DxuiButton           * m_skip             = nullptr;
    DxuiButton           * m_confirm          = nullptr;
    DxuiButton           * m_cancel           = nullptr;
    std::wstring           m_copiedFrom;                 // the selected file whose name the box was last given
    bool                   m_isNameConfigured = false;
};
