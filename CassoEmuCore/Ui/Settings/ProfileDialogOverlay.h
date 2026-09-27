#pragma once

#include "Pch.h"

#include "Controllers/ControllerProfileStore.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiComboBox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiRadio.h"
#include "Widgets/DxuiTextInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileDialogOverlay
//
//  The Controllers page's profile dialogs, hosted as the Settings sheet's
//  modal overlay: New and Rename, which take a name, the Delete confirmation,
//  and the save-or-discard prompt shown when switching away from a profile
//  with unapplied edits.
//
//  The primary button runs the accept callback, which returns what the edit
//  did. A refused name is shown under the field and the dialog stays open;
//  anything else closes it. The save-or-discard prompt has a third button,
//  Cancel, beside Save and Discard.
//
//  New lists the starting points it is given. A copy's radio button is
//  followed by a drop-down of the profiles it can copy, and choosing from
//  the drop-down chooses the copy.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileDialogOverlay
{
public:

    enum class Kind
    {
        NewProfile,
        RenameProfile,
        ConfirmDelete,
        SaveOrDiscard,
    };

    // `copySource` is the profile chosen to copy, and empty for any other
    // starting point.
    using AcceptFn  = std::function<ProfileEditResult (const std::wstring & name, ProfileSource source, const std::wstring & copySource)>;
    using DeclineFn = std::function<void ()>;

    void  SetHwnd           (HWND hwnd) { m_name.SetHwnd (hwnd); }

    // The window the copy drop-down opens its list in, so the list is not
    // clipped to the sheet; with none it is painted in the sheet.
    void  SetPopupHost      (DxuiHwndSource * host) { m_copySource.SetPopupHost (host); }

    // The name field measures glyphs through this to place the caret under a
    // click and to extend a drag selection.
    void     SetTextRenderer   (IDxuiTextRenderer * renderer) { m_name.SetTextRenderer (renderer); }
    LPCWSTR  GetCursorForPoint (POINT clientPx) const;

    void  OpenNew           (const std::vector<ProfileSource> & sources,
                             const std::vector<std::wstring>  & copySources,
                             size_t                             copySelected,
                             AcceptFn                           onAccept);
    void  OpenRename        (const std::wstring & currentName, AcceptFn onAccept);
    void  OpenConfirmDelete (const std::wstring & name, AcceptFn onAccept);
    void  OpenSaveOrDiscard (const std::wstring & name, AcceptFn onSave, DeclineFn onDiscard, DeclineFn onCancel);
    void  Close             ()       { m_copySource.Close(); m_open = false; }
    bool  IsOpen            () const { return m_open; }
    Kind  GetKind           () const { return m_kind; }

    void  Layout            (const RECT & panelRect, const DxuiDpiScaler & scaler);
    void  OnLButtonDown     (int x, int y);
    void  OnLButtonUp       (int x, int y);
    void  OnMouseMove       (int x, int y);
    bool  OnKey             (WPARAM vk);
    bool  OnChar            (wchar_t ch);
    void  Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);

private:

    enum class Focus
    {
        Name,
        Source,
        CopySource,
        Primary,
        Secondary,
        Tertiary,
    };

    void                Open           (Kind kind, const std::wstring & name, AcceptFn onAccept, DeclineFn onDecline);
    void                Accept         ();
    void                Decline        ();
    void                Dismiss        ();
    bool                HasCancel      () const { return m_kind == Kind::SaveOrDiscard; }
    void                MoveFocus      (int delta);
    void                ApplyFocus     ();
    std::vector<Focus>  GetFocusOrder  () const;
    bool                HasNameField   () const;
    bool                HasCopyList    () const;
    int                 FindCopyIndex  () const;
    void                ChooseCopy     ();

    static RECT         MakeRect       (int l, int t, int w, int h);

    Kind                m_kind         = Kind::NewProfile;
    bool                m_open         = false;
    Focus               m_focus        = Focus::Primary;
    AcceptFn            m_onAccept;
    DeclineFn           m_onDecline;
    DeclineFn           m_onCancel;
    std::wstring        m_subject;
    RECT                m_panelRect    = {};
    RECT                m_dialogRect   = {};
    DxuiDpiScaler       m_scaler;
    bool                m_hasLayout    = false;

    std::vector<ProfileSource>  m_sources;
    std::vector<std::wstring>   m_copySources;

    DxuiLabel           m_title;
    DxuiLabel           m_nameLabel;
    DxuiTextInput       m_name;
    DxuiLabel           m_sourceLabel;
    DxuiRadioGroup      m_source;
    DxuiComboBox        m_copySource;
    DxuiLabel           m_errorLabel;
    DxuiLabel           m_errorRule;
    DxuiButton          m_primary;
    DxuiButton          m_secondary;
    DxuiButton          m_tertiary;
};
