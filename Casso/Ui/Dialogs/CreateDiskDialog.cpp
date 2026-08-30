#include "Pch.h"

#include "Devices/Disk/MountDiagnosis.h"

#include "CreateDiskDialog.h"

#include "Window/DxuiButtonRow.h"
#include "Window/DxuiMessageBox.h"
#include "Widgets/DxuiButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Configure
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::Configure (
    FileBrowseModel  * model,
    const IDxuiTheme * theme,
    AvailableFn        payloadAvailable,
    DownloadFn         downloadPayload)
{
    m_model            = model;
    m_theme            = theme;
    m_payloadAvailable = std::move (payloadAvailable);
    m_downloadPayload  = std::move (downloadPayload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
//  Builds the body (path label + listing + name row), the Create / Cancel
//  buttons, and seeds everything from the model: the listing shows the
//  current folder, the name field starts on the first non-colliding default
//  name, and selecting a file row copies its name into the field.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::OnCreate()
{
    std::vector<DxuiListView::Column>  cols;
    DxuiButton *                       createButton = nullptr;



    // No per-control SetDpi here: DPI flows through the panel tree's layout
    // pass, so every child picks it up before first paint.
    m_pathLabel.SetTextRole  (DxuiTextRole::Body);
    m_pathLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    cols.push_back ({ L"Name",     0, false, DxuiTextRenderer::HAlign::Left });
    cols.push_back ({ L"Size",     0, false, DxuiTextRenderer::HAlign::Right });
    cols.push_back ({ L"Modified", 0, false, DxuiTextRenderer::HAlign::Left });

    m_list.SetTheme         (m_theme);
    m_list.SetShowHeader    (true);
    m_list.SetColumns       (std::move (cols));
    m_list.SetPreciseAutoFit (true);
    m_list.SetKeyboardColumnNav (true);
    m_list.EnableStickyTail (false);
    m_list.SetActivateOnDoubleClick (true);
    m_list.SetAlwaysShowSelection (true);
    m_list.SetOnSelectionChanged ([this] (int row)
    {
        const auto &  entries = m_model->Entries();

        if (row >= 0 && row < (int) entries.size() && !entries[(size_t) row].isFolder)
        {
            m_nameInput.SetText (entries[(size_t) row].name);
            Invalidate();
        }
    });
    m_list.SetOnActivateRow ([this] (int row) { OnRowActivated (row); });

    m_formatLabel.SetTextRole  (DxuiTextRole::Body);
    m_formatLabel.SetText      (L"Format:");
    m_formatLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_formatDropdown.SetPopupHost (PopupHost());
    m_formatDropdown.SetItems     ({ BlankDiskBuilder::GetContentsCaption (BlankDiskContents::Dos33),
                                     BlankDiskBuilder::GetContentsCaption (BlankDiskContents::ProDos),
                                     BlankDiskBuilder::GetContentsCaption (BlankDiskContents::Unformatted) });
    m_formatDropdown.SetSelected  (0);
    m_formatDropdown.SetSelect    ([this] (int index) { OnFormatChanged (index); });

    m_imageTypeLabel.SetTextRole  (DxuiTextRole::Body);
    m_imageTypeLabel.SetText      (L"Image type:");
    m_imageTypeLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_imageTypeDropdown.SetPopupHost (PopupHost());
    m_imageTypeDropdown.SetSelect    ([this] (int index) { OnImageTypeChanged (index); });

    RebuildImageTypeChoices();

    m_bootableCheck.SetSingleLineLabel (true);

    m_downloadButton.SetOnClick ([this] () { OnDownloadClicked(); });

    UpdateBootableRow();

    m_nameLabel.SetTextRole  (DxuiTextRole::Body);
    m_nameLabel.SetText      (L"Name:");
    m_nameLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_nameInput.SetTheme (m_theme);
    m_nameInput.SetHwnd  (Hwnd());
    m_nameInput.SetMaxLength (128);
    m_nameInput.SetTextRenderer (TextRenderer());

    {
        CreateDiskBodyPanel::Children  kids;

        kids.pathLabel      = &m_pathLabel;
        kids.list           = &m_list;
        kids.formatLabel    = &m_formatLabel;
        kids.format         = &m_formatDropdown;
        kids.imageTypeLabel = &m_imageTypeLabel;
        kids.imageType      = &m_imageTypeDropdown;
        kids.bootable       = &m_bootableCheck;
        kids.download       = &m_downloadButton;
        kids.nameLabel      = &m_nameLabel;
        kids.nameInput      = &m_nameInput;

        m_body = CreateDialogContent<CreateDiskBodyPanel>();
        m_body->Init (kids);
        m_body->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });
    }

    createButton = AddDialogButton (L"Create", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    // A custom click keeps the dialog open through validation; only a clean
    // (or explicitly confirmed) target ends the modal.
    createButton->SetOnClick ([this] () { OnCreateClicked(); });

    SetInitialFocus (&m_nameInput);

    RefreshFromModel();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshListing
//
//  Rebuilds the path label and file rows from the model without touching
//  the name field -- format changes re-filter the listing but must not
//  clobber what the user typed.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::RefreshListing()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    if (m_model == nullptr)
    {
        return;
    }

    m_pathLabel.SetText (m_model->CurrentFolder());

    rows.reserve (m_model->Entries().size());

    for (const FileBrowseEntry & entry : m_model->Entries())
    {
        rows.push_back ({ { entry.name,                     false, {} },
                          { FormatSize (entry),             true,  {} },
                          { FormatModified (entry.modifiedUnix), true, {} } });
    }

    m_list.SetRows (std::move (rows));
    m_list.UpdateAutoFitFromRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshFromModel
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::RefreshFromModel()
{
    if (m_model == nullptr)
    {
        return;
    }

    RefreshListing();

    m_nameInput.SetText (m_model->UniqueDefaultName (L"Blank Disk"));

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplaceExtension
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CreateDiskDialog::ReplaceExtension (const std::wstring & name, const wchar_t * ext)
{
    size_t  dot = name.find_last_of (L'.');



    return ((dot == std::wstring::npos) ? name : name.substr (0, dot)) + ext;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildImageTypeChoices
//
//  The Format choice (the primary pick) drives which image types can carry
//  it, so an illegal pairing is never even listed. The current image type is
//  preserved by value when it stays legal and snaps to the first offered when
//  not; a snap re-applies the name extension and filter.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::RebuildImageTypeChoices()
{
    std::vector<std::wstring>  captions;
    DiskFormat                 before   = m_imageType;
    int                        selected = 0;
    size_t                     i        = 0;



    m_imageTypeChoices = BlankDiskBuilder::ContainersFor (m_contents);

    for (i = 0; i < m_imageTypeChoices.size(); i++)
    {
        captions.push_back (MountDiagnosis::GetContainerCaption (m_imageTypeChoices[i]));

        if (m_imageTypeChoices[i] == m_imageType)
        {
            selected = (int) i;
        }
    }

    m_imageType = m_imageTypeChoices[(size_t) selected];

    m_imageTypeDropdown.SetItems    (captions);
    m_imageTypeDropdown.SetSelected (selected);

    if (m_imageType != before)
    {
        ApplyImageTypeExtension();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyImageTypeExtension
//
//  The name field's extension and the listing's filter both follow the
//  image type.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::ApplyImageTypeExtension()
{
    m_nameInput.SetText (ReplaceExtension (
        m_nameInput.Text(),
        MountDiagnosis::GetPrimaryExtensionText (m_imageType).c_str()));

    if (m_model != nullptr)
    {
        m_model->SetExtensionFilter (
            MountDiagnosis::GetPrimaryExtensionText (m_imageType).c_str());
        RefreshListing();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFormatChanged
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::OnFormatChanged (int index)
{
    switch (index)
    {
        case 1:  m_contents = BlankDiskContents::ProDos;      break;
        case 2:  m_contents = BlankDiskContents::Unformatted; break;
        default: m_contents = BlankDiskContents::Dos33;       break;
    }

    RebuildImageTypeChoices();
    UpdateBootableRow();

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnImageTypeChanged
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::OnImageTypeChanged (int index)
{
    if (index >= 0 && index < (int) m_imageTypeChoices.size())
    {
        m_imageType = m_imageTypeChoices[(size_t) index];
        ApplyImageTypeExtension();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateBootableRow
//
//  The checkbox label carries its own explanation. Three states: raw media
//  cannot boot (disabled, says why); the OS master is cached (live, says
//  what checking installs); or the master is missing (disabled short
//  label + an explicit Download button -- FR-017).
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::UpdateBootableRow()
{
    bool          formatted = (m_contents != BlankDiskContents::Unformatted);
    bool          available = false;
    std::wstring  os        = BlankDiskBuilder::GetContentsCaption (m_contents);



    if (formatted && m_payloadAvailable)
    {
        available = m_payloadAvailable (m_contents);
    }

    if (!formatted)
    {
        m_bootableCheck.SetChecked (false);
        m_bootableCheck.SetEnabled (false);
        m_bootableCheck.SetLabel (L"Make bootable (unformatted media cannot carry an OS)");
        m_downloadButton.SetVisible (false);
    }
    else if (available)
    {
        m_bootableCheck.SetEnabled (true);
        m_bootableCheck.SetLabel (L"Make bootable (installs " + os
                                  + L" from the downloaded master disk)");
        m_downloadButton.SetVisible (false);
    }
    else
    {
        m_bootableCheck.SetChecked (false);
        m_bootableCheck.SetEnabled (false);
        m_bootableCheck.SetLabel (L"Make bootable");
        m_downloadButton.SetVisible (true);
        m_downloadButton.SetEnabled (true);
        m_downloadButton.SetLabel (L"Download " + os + L"...");
    }

    // The strip re-flows around the download button's visibility.
    if (m_body != nullptr)
    {
        m_body->Relayout();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDownloadClicked
//
//  The explicit click is the user's download consent. Blocking fetch;
//  success re-evaluates the row so the toggle goes live.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::OnDownloadClicked()
{
    HRESULT  hr = S_OK;



    if (!m_downloadPayload)
    {
        return;
    }

    m_downloadButton.SetEnabled (false);
    Invalidate();

    hr = m_downloadPayload (m_contents);

    if (FAILED (hr))
    {
        DxuiMessageBox (Hwnd(), m_theme,
                        L"The download failed. Check your connection and try again.",
                        L"Create New Disk", MB_OK | MB_ICONWARNING);
    }

    UpdateBootableRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRowActivated
//
//  Double-click / Enter on a folder row (the synthetic ".." included)
//  navigates; activating a file row is covered by selection, which already
//  copied its name into the field.
//
//  Going up selects and centers the folder just left, the way a real file
//  dialog keeps the user oriented; going down starts the child folder's
//  listing at the top with nothing selected.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::OnRowActivated (int row)
{
    HRESULT       hr = S_OK;
    bool          up = false;
    std::wstring  cameFrom;



    if (m_model == nullptr || row < 0 || row >= (int) m_model->Entries().size())
    {
        return;
    }

    if (!m_model->Entries()[(size_t) row].isFolder)
    {
        // Activating a file row is choosing it, save-dialog style: its name
        // is already in the field (selection copied it), so run Create --
        // the overwrite confirm still stands between the click and the disk.
        OnCreateClicked();
        return;
    }

    up = (m_model->Entries()[(size_t) row].name == L"..");

    if (up)
    {
        cameFrom = std::filesystem::path (m_model->CurrentFolder()).filename().wstring();
    }

    hr = m_model->NavigateInto ((size_t) row);

    if (FAILED (hr))
    {
        return;
    }

    RefreshListing();

    if (up)
    {
        const auto &  entries = m_model->Entries();
        size_t        i       = 0;

        for (i = 0; i < entries.size(); i++)
        {
            if (entries[i].isFolder && entries[i].name == cameFrom)
            {
                m_list.SetSelectedRow ((int) i);
                m_list.CenterOnRow ((int) i);
                break;
            }
        }
    }
    else
    {
        m_list.SetTopRow (0);
        m_list.SetSelectedRow (-1);
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreateClicked
//
//  Validation order comes from the model: an invalid name and a currently-
//  mounted target are hard refusals (the latter names the drive), an
//  existing file needs the user's explicit yes, and only then does the
//  modal end confirmed.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::OnCreateClicked()
{
    std::wstring   name    = m_nameInput.Text();
    int            drive   = -1;
    TargetVerdict  verdict = TargetVerdict::InvalidName;
    std::wstring   message;
    int            choice  = IDYES;



    if (m_model == nullptr)
    {
        return;
    }

    // The file's extension always matches the chosen format.
    {
        std::wstring    extText = MountDiagnosis::GetPrimaryExtensionText (m_imageType);
        const wchar_t * ext     = extText.c_str();
        size_t          extLen  = wcslen (ext);
        bool            matches = name.size() >= extLen;
        size_t          i       = 0;

        for (i = 0; matches && i < extLen; i++)
        {
            matches = towlower (name[name.size() - extLen + i]) == (wint_t) ext[i];
        }

        if (!matches)
        {
            name += ext;
            m_nameInput.SetText (name);
        }
    }

    verdict = m_model->ValidateTarget (name, drive);

    switch (verdict)
    {
        case TargetVerdict::InvalidName:
            message = L"\"" + name + L"\" is not a valid file name.";
            DxuiMessageBox (Hwnd(), m_theme, message.c_str(),
                            L"Create New Disk", MB_OK | MB_ICONWARNING);
            break;

        case TargetVerdict::MountedInDrive:
            message = L"\"" + name + L"\" is mounted in Drive "
                    + std::to_wstring (drive + 1)
                    + L". Eject it before overwriting it with a new disk.";
            DxuiMessageBox (Hwnd(), m_theme, message.c_str(),
                            L"Create New Disk", MB_OK | MB_ICONWARNING);
            break;

        case TargetVerdict::Exists:
            message = L"\"" + name + L"\" already exists. Replace it?";
            choice  = DxuiMessageBox (Hwnd(), m_theme, message.c_str(),
                                      L"Create New Disk",
                                      MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING);

            if (choice != IDYES)
            {
                break;
            }

            [[fallthrough]];

        case TargetVerdict::Ok:
            m_result.spec.format   = m_imageType;
            m_result.spec.contents = m_contents;
            m_result.spec.bootable = m_bootableCheck.Enabled() && m_bootableCheck.Checked();
            m_result.targetPath    = m_model->ComposeTargetPath (name);
            m_result.confirmed     = true;
            EndDialog (IDOK);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatSize
//
//  Folders show no size; files show a compact KB figure (disk images are
//  small enough that KB never misleads).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CreateDiskDialog::FormatSize (const FileBrowseEntry & entry)
{
    constexpr uint64_t  kBytesPerKb = 1024;
    uint64_t            kb          = 0;



    if (entry.isFolder)
    {
        return std::wstring();
    }

    kb = (entry.sizeBytes + kBytesPerKb - 1) / kBytesPerKb;

    return std::to_wstring (kb) + L" KB";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatModified
//
//  Short date + time in the user's configured locale (12-hour regions get
//  AM/PM, date order follows regional settings); empty when the entry
//  carries no stamp.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CreateDiskDialog::FormatModified (int64_t modifiedUnix)
{
    constexpr size_t  kStampChars           = 64;
    wchar_t           dateText[kStampChars] = {};
    wchar_t           timeText[kStampChars] = {};
    tm                local                 = {};
    SYSTEMTIME        st                    = {};
    time_t            when                  = (time_t) modifiedUnix;
    int               dateOk                = 0;
    int               timeOk                = 0;



    if (modifiedUnix == 0 || localtime_s (&local, &when) != 0)
    {
        return std::wstring();
    }

    st.wYear   = (WORD) (local.tm_year + 1900);
    st.wMonth  = (WORD) (local.tm_mon + 1);
    st.wDay    = (WORD) local.tm_mday;
    st.wHour   = (WORD) local.tm_hour;
    st.wMinute = (WORD) local.tm_min;
    st.wSecond = (WORD) local.tm_sec;

    dateOk = GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st,
                              nullptr, dateText, (int) kStampChars, nullptr);
    timeOk = GetTimeFormatEx (LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &st,
                              nullptr, timeText, (int) kStampChars);

    if (dateOk == 0 || timeOk == 0)
    {
        return std::wstring();
    }

    return std::wstring (dateText) + L" " + timeText;
}
