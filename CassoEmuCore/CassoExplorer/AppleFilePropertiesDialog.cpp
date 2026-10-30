#include "Pch.h"

#include "CassoExplorer/AppleFilePropertiesDialog.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void AppleFilePropertiesPanel::Init (std::vector<Row> rows)
{
    m_rows = std::move (rows);

    for (const Row & row : m_rows)
    {
        if (row.label != nullptr)
        {
            Adopt (*row.label);
        }

        for (const Field & field : row.fields)
        {
            Adopt (*field.control);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesPanel::IsRowShown
//
////////////////////////////////////////////////////////////////////////////////

bool AppleFilePropertiesPanel::IsRowShown (const Row & row) const
{
    return row.fields.empty() ? (row.label != nullptr && row.label->IsVisible())
                              : std::any_of (row.fields.begin(), row.fields.end(), [] (const Field & field) { return field.control->IsVisible(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesPanel::GetRequiredHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int AppleFilePropertiesPanel::GetRequiredHeightDip() const
{
    int  height = 0;



    for (const Row & row : m_rows)
    {
        if (IsRowShown (row))
        {
            height += (row.gapAbove ? kGroupGapDip : kRowGapDip) + row.heightDip;
        }
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesPanel::Layout
//
//  A row with fields has its label in the label column; a row with only a
//  label, a note, starts in the field column.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFilePropertiesPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  fieldX = boundsDip.left + scaler.ToPx (kLabelWidthDip);
    int  y      = boundsDip.top;



    SetBounds (boundsDip);

    for (const Row & row : m_rows)
    {
        int  height = scaler.ToPx (row.heightDip);
        int  x      = fieldX;

        if (!IsRowShown (row))
        {
            continue;
        }

        y += scaler.ToPx (row.gapAbove ? kGroupGapDip : kRowGapDip);

        if (row.label != nullptr)
        {
            row.label->Layout (row.fields.empty() ? RECT { fieldX, y, boundsDip.right, y + height } : RECT { boundsDip.left, y, fieldX, y + height }, scaler);
        }

        for (const Field & field : row.fields)
        {
            int  right = (field.widthDip == 0) ? boundsDip.right : (std::min) ((int) boundsDip.right, x + scaler.ToPx (field.widthDip));

            if (field.control->IsVisible())
            {
                field.control->Layout (RECT { x, y, right, y + height }, scaler);
                x = right + scaler.ToPx (kFieldGapDip);
            }
        }

        y += height;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool AppleFilePropertiesPanel::OnMouse (const DxuiMouseEvent & ev)
{
    for (const Row & row : m_rows)
    {
        for (const Field & field : row.fields)
        {
            if (!field.control->IsVisible() || !field.control->OnMouse (ev))
            {
                continue;
            }

            if (ev.kind == DxuiMouseEventKind::Down && m_onPressed)
            {
                m_onPressed (field.control);
            }

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::Show
//
////////////////////////////////////////////////////////////////////////////////

void AppleFilePropertiesDialog::Show (HWND owner, const IDxuiTheme * theme, const std::vector<FileEntry> & entries, VolumeKind kind, bool readOnly, ApplyFn apply,
                                      const HostOptions * host)
{
    HRESULT                    hr     = S_OK;
    AppleFilePropertiesDialog  dialog;
    DxuiWindow::CreateParams   params;



    if (entries.empty())
    {
        return;
    }

    dialog.m_theme    = theme;
    dialog.m_entries  = entries;
    dialog.m_kind     = kind;
    dialog.m_readOnly = readOnly;
    dialog.m_apply    = std::move (apply);
    dialog.m_hasHost  = host != nullptr;
    dialog.m_host     = (host != nullptr) ? *host : HostOptions();

    //  A file on the PC is captioned with its name there, as Windows' sheet is.
    params.title                    = (entries.size() != 1) ? std::wstring (L"Properties")
                                    : ((host != nullptr) ? host->leafName : TextEncoding::NarrowToWide (entries[0].name)) + L" properties";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { s_kWidthDip, 480 };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return;
    }

    dialog.SetTheme (theme);
    dialog.FitToRows();
    dialog.ShowModalDialog (IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void AppleFilePropertiesDialog::OnCreate()
{
    bool                                         proDos   = m_kind == VolumeKind::ProDos;
    bool                                         single   = m_entries.size() == 1;
    const FileEntry                            & first    = m_entries[0];
    uint64_t                                     bytes    = 0;
    std::vector<std::wstring>                    items;
    std::vector<AppleFilePropertiesPanel::Row>   rows;
    std::pair<DxuiCheckbox *, Byte>              bits[]   = { { &m_read, AppleFileProperties::kAccessRead },     { &m_write,   AppleFileProperties::kAccessWrite },
                                                              { &m_rename, AppleFileProperties::kAccessRename }, { &m_destroy, AppleFileProperties::kAccessDestroy },
                                                              { &m_backup, AppleFileProperties::kAccessBackup } };
    DxuiButton                                 * ok       = nullptr;



    for (DxuiLabel * label : { &m_nameLabel, &m_typeLabel, &m_auxLabel, &m_accessLabel, &m_createdLabel, &m_modifiedLabel, &m_sizeLabel, &m_size })
    {
        ExplorerTextStyles::Apply (*label, ExplorerTextStyles::kFormLabel);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    ExplorerTextStyles::Apply (m_typeNote, ExplorerTextStyles::kFormNote);
    m_typeNote.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_typeNote.SetText      (L"Changing the type doesn't convert the file's data.");

    m_nameLabel.SetText     (L"Name:");
    m_typeLabel.SetText     (L"Type:");
    m_accessLabel.SetText   (L"Access:");
    m_createdLabel.SetText  (L"Created:");
    m_modifiedLabel.SetText (L"Modified:");
    m_sizeLabel.SetText     (L"Size:");

    for (DxuiTextInput * input : { &m_name, &m_typeHex, &m_aux, &m_created, &m_modified })
    {
        input->SetTheme        (m_theme);
        input->SetHwnd         (GetHwnd());
        input->SetTextRenderer (GetTextRenderer());
        input->SetPlaceholder  (single ? std::wstring() : std::wstring (s_kMultiple));
        input->SetEnabled      (!m_readOnly);
    }

    //  The name, only for one file.
    m_nameShown = single ? TextEncoding::NarrowToWide (first.name) : std::wstring();
    m_name.SetText    (m_nameShown);
    m_name.SetEnabled (single && !m_readOnly);

    //  An AppleSingle file on the PC is renamed to match, checked when its name
    //  on the PC is its Apple name with .as added.
    m_renameHost.SetSingleLineLabel (true);
    m_renameHost.SetVisible (m_hasHost && m_host.offerRename && single);
    m_renameHost.SetChecked (m_hasHost && _wcsicmp (m_host.leafName.c_str(), HostAppleFile::MakeAppleSingleName (first.name).c_str()) == 0);
    m_name.SetOnChange      ([this] (const std::wstring &) { UpdateRenameBox(); });
    UpdateRenameBox();
    //  The type list, (multiple) first when the files' types differ, and on
    //  ProDOS, Other with its hex byte for a type with no name.
    m_sameType   = IsSame (m_entries, [] (const FileEntry & e) { return e.type; });
    m_typeOffset = !m_sameType;
    m_choices    = AppleFileProperties::GetTypeChoices (m_kind, first.type);

    if (m_typeOffset)
    {
        items.push_back (s_kMultiple);
    }

    for (const AppleFileProperties::TypeChoice & choice : m_choices)
    {
        items.push_back (choice.label);
    }

    if (proDos)
    {
        items.push_back (L"Other");
    }

    m_type.SetPopupHost (GetPopupHost());
    m_type.SetItems     (items);
    m_type.SetEnabled   (!m_readOnly);
    m_typeHex.SetMaxLength (2);

    if (!m_sameType)
    {
        m_type.SetSelected (0);
    }
    else if (AppleFileProperties::FindChoice (m_choices, first.type) >= 0)
    {
        m_type.SetSelected (AppleFileProperties::FindChoice (m_choices, first.type));
    }
    else
    {
        m_type.SetSelected ((int) m_choices.size());
        m_typeHex.SetText  (AppleFileProperties::FormatHex (first.type, 2));
    }

    m_type.SetSelect     ([this] (int) { m_typeEdited = true; UpdateTypeRow(); });
    m_typeHex.SetOnChange ([this] (const std::wstring &) { m_typeEdited = true; UpdateTypeRow(); });

    //  The second field, only when every file has the same type.
    m_aux.SetMaxLength (4);

    if (IsSame (m_entries, [] (const FileEntry & e) { return e.hasLoadAddress ? e.loadAddress : e.auxType; }))
    {
        m_aux.SetText (AppleFileProperties::FormatHex (first.hasLoadAddress ? first.loadAddress : first.auxType, 4));
    }

    m_aux.SetOnChange ([this] (const std::wstring &) { m_auxEdited = true; });

    //  Access: Locked on DOS 3.3, ProDOS's bits otherwise. A bit that differs
    //  between the files starts clear and changes only when it is clicked.
    m_locked.SetChecked  (std::all_of (m_entries.begin(), m_entries.end(), [] (const FileEntry & e) { return e.isLocked; }));
    m_locked.SetEnabled  (!m_readOnly);
    m_locked.SetVisible  (!proDos && (!m_hasHost || m_host.showAccess));
    m_locked.SetOnChange ([this] (bool) { m_lockedEdited = true; });

    for (auto [box, bit] : bits)
    {
        Byte  mask = bit;

        box->SetChecked  (std::all_of (m_entries.begin(), m_entries.end(), [mask] (const FileEntry & e) { return (AppleFileProperties::GetAccess (e) & mask) != 0; }));
        box->SetEnabled  (!m_readOnly);
        box->SetVisible  (proDos && (!m_hasHost || m_host.showAccess));
        box->SetSingleLineLabel (true);
        box->SetOnChange ([this, mask] (bool) { m_accessMask |= mask; });
    }

    //  The dates, which only ProDOS stores.
    if (IsSame (m_entries, [] (const FileEntry & e) { return ((uint32_t) e.createdDate << 16) | e.createdTime; }))
    {
        m_created.SetText (AppleFileProperties::FormatDate (first.createdDate, first.createdTime));
    }

    if (IsSame (m_entries, [] (const FileEntry & e) { return ((uint32_t) e.modifiedDate << 16) | e.modifiedTime; }))
    {
        m_modified.SetText (AppleFileProperties::FormatDate (first.modifiedDate, first.modifiedTime));
    }

    m_created.SetOnChange  ([this] (const std::wstring &) { m_createdEdited  = true; });
    m_modified.SetOnChange ([this] (const std::wstring &) { m_modifiedEdited = true; });

    //  Each date drops a calendar, as Windows' date picker does; the time
    //  is typed.
    for (auto [pick, field, edited] : { std::tuple<DxuiIconButton *, DxuiTextInput *, bool *> { &m_createdPick,  &m_created,  &m_createdEdited },
                                        std::tuple<DxuiIconButton *, DxuiTextInput *, bool *> { &m_modifiedPick, &m_modified, &m_modifiedEdited } })
    {
        DxuiTextInput  * target = field;
        bool           * flag   = edited;

        pick->SetGlyph          (s_kpszMdl2Calendar);
        pick->SetAccessibleName (L"Choose a date");
        pick->SetEnabled        (!m_readOnly);
        pick->SetOnClick        ([this, target, flag]() { PickDate (*target, *flag); });
    }

    for (IDxuiControl * control : std::initializer_list<IDxuiControl *> { &m_createdLabel, &m_created, &m_createdPick, &m_modifiedLabel, &m_modified, &m_modifiedPick })
    {
        control->SetVisible (proDos && (!m_hasHost || m_host.showDates));
    }

    for (const FileEntry & entry : m_entries)
    {
        bytes += entry.hasEofBytes ? entry.eofBytes : (uint64_t) entry.sizeUnits * CatalogModel::GetUnitBytes (m_kind);
    }

    m_size.SetText (CassoExplorerProperties::FormatBytes (bytes));

    rows.push_back ({ &m_nameLabel,     { { &m_name } } });
    rows.push_back ({ nullptr,          { { &m_renameHost } } });
    rows.push_back ({ &m_typeLabel,     { { &m_type }, { &m_typeHex, s_kHexWidthDip } }, true });
    rows.push_back ({ &m_typeNote,      {}, false, 20 });
    rows.push_back ({ &m_auxLabel,      { { &m_aux, s_kHexWidthDip } } });
    rows.push_back ({ &m_accessLabel,   { { &m_locked, 90 }, { &m_read, 70 }, { &m_write, 72 }, { &m_rename, 86 }, { &m_destroy, 86 }, { &m_backup, 86 } }, true });
    rows.push_back ({ &m_createdLabel,  { { &m_created, s_kDateWidthDip }, { &m_createdPick, s_kPickWidthDip } }, true });
    rows.push_back ({ &m_modifiedLabel, { { &m_modified, s_kDateWidthDip }, { &m_modifiedPick, s_kPickWidthDip } } });
    rows.push_back ({ &m_sizeLabel,     { { &m_size } }, true });

    m_body = CreateDialogContent<AppleFilePropertiesPanel>();
    m_body->Init (std::move (rows));
    m_body->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });

    UpdateTypeRow();
    ResetEdited();

    if (m_readOnly)
    {
        ok = AddDialogButton (L"Close", IDCANCEL);
        ok->SetOnClick ([this]() { EndDialog (IDCANCEL); });
    }
    else
    {
        ok = AddDialogButton (L"OK", IDOK);
        ok->SetOnClick ([this]() { if (Apply()) { EndDialog (IDOK); } });
        AddDialogButton (L"Cancel", IDCANCEL);
        AddDialogButton (L"Apply", DxuiButtonRow::kApplyCommandId)->SetOnClick ([this]() { Apply(); });
    }

    SetInitialFocus (ok);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::GetChosenType
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleFilePropertiesDialog::GetChosenType (bool & outKnown) const
{
    int       index = m_type.GetSelectedIndex() - (m_typeOffset ? 1 : 0);
    unsigned  value = 0;



    outKnown = true;

    if (index >= 0 && index < (int) m_choices.size())
    {
        return m_choices[(size_t) index].type;
    }

    //  Other: the hex byte typed beside it.
    outKnown = index == (int) m_choices.size() && AppleFileProperties::TryParseHex (m_typeHex.GetText(), 2, value);

    return (Byte) value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::UpdateTypeRow
//
//  The hex byte is visible for Other, and the second field's label and row
//  match the type chosen.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFilePropertiesDialog::UpdateTypeRow()
{
    bool                          known = false;
    bool                          multi = m_typeOffset && m_type.GetSelectedIndex() == 0;
    Byte                          type  = GetChosenType (known);
    AppleFileProperties::AuxKind  aux   = multi ? AppleFileProperties::AuxKind::None : AppleFileProperties::GetAuxKind (m_kind, type);



    m_typeHex.SetVisible (!multi && m_type.GetSelectedIndex() - (m_typeOffset ? 1 : 0) == (int) m_choices.size());
    m_auxLabel.SetText   (AppleFileProperties::GetAuxLabel (aux));
    m_auxLabel.SetVisible (aux != AppleFileProperties::AuxKind::None);
    m_aux.SetVisible     (aux != AppleFileProperties::AuxKind::None);

    if (m_body != nullptr)
    {
        FitToRows();
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::PickDate
//
//  The calendar opens on the field's date, and the day picked replaces the
//  date; the time typed is unaffected. A date ProDOS can't store is ignored.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFilePropertiesDialog::PickDate (DxuiTextInput & field, bool & edited)
{
    Word               date  = 0;
    Word               time  = 0;
    DxuiCalendar::Day  shown;
    int                two   = 0;
    RECT               where = field.GetRect();



    if (AppleFileProperties::TryParseDate (field.GetText(), date, time) && date != 0)
    {
        two   = (date >> 9) & 0x7F;
        shown = DxuiCalendar::Day { two + ((two < AppleFileProperties::kPivot) ? 2000 : 1900), (date >> 5) & 0x0F, date & 0x1F };
    }

    if (m_theme == nullptr || GetPopupHost() == nullptr)
    {
        return;
    }

    m_calendar.Show (*GetPopupHost(), *m_theme, where, shown, [this, &field, &edited, time] (const DxuiCalendar::Day & day)
    {
        Word  picked = 0;

        if (day.year < AppleFileProperties::kFirstYear || day.year > AppleFileProperties::kLastYear)
        {
            return;
        }

        picked = (Word) (((day.year % 100) << 9) | (day.month << 5) | day.day);

        field.SetText (AppleFileProperties::FormatDate (picked, time));
        edited = true;
        Invalidate();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::FitToRows
//
//  The window's height is the visible rows, the caption, the margins and the
//  button row.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFilePropertiesDialog::FitToRows()
{
    RECT  window = {};
    UINT  dpi    = 0;
    int   height = 0;



    if (m_body == nullptr || GetHwnd() == nullptr || !GetWindowRect (GetHwnd(), &window))
    {
        return;
    }

    dpi    = GetDpiForWindow (GetHwnd());
    height = MulDiv (m_body->GetRequiredHeightDip() + s_kChromeDip, (int) dpi, USER_DEFAULT_SCREEN_DPI);

    if (height != window.bottom - window.top)
    {
        SetWindowPos (GetHwnd(), nullptr, 0, 0, window.right - window.left, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::UpdateRenameBox
//
//  The box is disabled, with the reason, when the file's new name on the PC
//  is another file's.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFilePropertiesDialog::UpdateRenameBox()
{
    std::wstring  name  = m_name.GetText();
    std::wstring  leaf;
    bool          taken = false;



    if (!m_renameHost.IsVisible())
    {
        return;
    }

    for (wchar_t & c : name)
    {
        c = (wchar_t) towupper (c);
    }

    leaf  = HostAppleFile::MakeAppleSingleName (TextEncoding::WideToNarrow (name));
    taken = _wcsicmp (leaf.c_str(), m_host.leafName.c_str()) != 0 && m_host.isTaken && m_host.isTaken (leaf);

    m_renameHost.SetLabel   (taken ? std::format (L"Also rename {} ({} already exists)", m_host.leafName, leaf) : L"Also rename " + m_host.leafName);
    m_renameHost.SetEnabled (!taken && !m_readOnly);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::ResetEdited
//
////////////////////////////////////////////////////////////////////////////////

void AppleFilePropertiesDialog::ResetEdited()
{
    m_nameShown      = m_name.GetText();
    m_typeEdited     = false;
    m_auxEdited      = false;
    m_accessMask     = 0;
    m_lockedEdited   = false;
    m_createdEdited  = false;
    m_modifiedEdited = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::CollectEdits
//
////////////////////////////////////////////////////////////////////////////////

bool AppleFilePropertiesDialog::CollectEdits (AppleFileProperties::Edits & outEdits, std::wstring & outError) const
{
    bool      known = false;
    unsigned  value = 0;



    outEdits = AppleFileProperties::Edits();
    outError.clear();

    outEdits.nameEdited = m_entries.size() == 1 && m_name.GetText() != m_nameShown;
    outEdits.name       = m_name.GetText();

    for (wchar_t & c : outEdits.name)
    {
        c = (wchar_t) towupper (c);
    }

    if (outEdits.nameEdited && !AppleFileProperties::IsValidName (m_hasHost ? m_host.nameKind : m_kind, outEdits.name))
    {
        outError = ((m_hasHost ? m_host.nameKind : m_kind) == VolumeKind::ProDos) ? L"A ProDOS name has up to 15 letters, digits and periods, and starts with a letter."
                                                  : L"A DOS 3.3 name has up to 30 characters, starts with a letter, and has no comma.";
        return false;
    }

    outEdits.renameHostFile = m_renameHost.IsVisible() && m_renameHost.IsEnabled() && m_renameHost.IsChecked();

    outEdits.type       = GetChosenType (known);
    outEdits.typeEdited = m_typeEdited && !(m_typeOffset && m_type.GetSelectedIndex() == 0);

    if (outEdits.typeEdited && !known)
    {
        outError = L"Enter the type as two hex digits, such as C1.";
        return false;
    }

    outEdits.auxEdited = m_auxEdited && m_aux.IsVisible() && !m_aux.GetText().empty();

    if (outEdits.auxEdited && !AppleFileProperties::TryParseHex (m_aux.GetText(), 4, value))
    {
        outError = L"Enter the " + AppleFileProperties::GetAuxLabel (AppleFileProperties::GetAuxKind (m_kind, outEdits.type)) + L" as up to four hex digits, such as 0803.";
        return false;
    }

    outEdits.aux = (Word) value;

    outEdits.accessMask = m_accessMask;
    outEdits.access     = (Byte) ((m_read.IsChecked()    ? AppleFileProperties::kAccessRead    : 0) |
                                  (m_write.IsChecked()   ? AppleFileProperties::kAccessWrite   : 0) |
                                  (m_rename.IsChecked()  ? AppleFileProperties::kAccessRename  : 0) |
                                  (m_destroy.IsChecked() ? AppleFileProperties::kAccessDestroy : 0) |
                                  (m_backup.IsChecked()  ? AppleFileProperties::kAccessBackup  : 0));

    outEdits.lockedEdited = m_lockedEdited;
    outEdits.locked       = m_locked.IsChecked();

    outEdits.createdEdited  = m_createdEdited;
    outEdits.modifiedEdited = m_modifiedEdited;

    if ((outEdits.createdEdited  && !AppleFileProperties::TryParseDate (m_created.GetText(),  outEdits.createdDate,  outEdits.createdTime)) ||
        (outEdits.modifiedEdited && !AppleFileProperties::TryParseDate (m_modified.GetText(), outEdits.modifiedDate, outEdits.modifiedTime)))
    {
        outError = std::format (L"Enter a date from {} to {}, such as 3/5/1988 2:54 PM, or leave it empty for none.", AppleFileProperties::kFirstYear, AppleFileProperties::kLastYear);
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog::Apply
//
////////////////////////////////////////////////////////////////////////////////

bool AppleFilePropertiesDialog::Apply()
{
    AppleFileProperties::Edits  edits;
    std::wstring                error;



    if (!CollectEdits (edits, error))
    {
        DxuiMessageBox (GetHwnd(), m_theme, error.c_str(), L"Properties", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (!edits.nameEdited && !edits.ChangesAttributes())
    {
        return true;
    }

    error = m_apply ? m_apply (edits, m_entries) : std::wstring();

    if (!error.empty())
    {
        DxuiMessageBox (GetHwnd(), m_theme, error.c_str(), L"Properties", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (edits.renameHostFile)
    {
        m_host.leafName = HostAppleFile::MakeAppleSingleName (TextEncoding::WideToNarrow (edits.name));
    }

    m_name.SetText (edits.name);
    ResetEdited();
    UpdateRenameBox();

    return true;
}
