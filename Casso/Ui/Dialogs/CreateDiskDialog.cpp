#include "Pch.h"

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
    UINT               dpi,
    AvailableFn        payloadAvailable,
    DownloadFn         downloadPayload)
{
    m_model            = model;
    m_theme            = theme;
    m_dpi              = dpi;
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



    m_pathLabel.SetDpi       (m_dpi);
    m_pathLabel.SetTextRole  (DxuiTextRole::Body);
    m_pathLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    cols.push_back ({ L"Name",     0, false, DxuiTextRenderer::HAlign::Left });
    cols.push_back ({ L"Size",     0, false, DxuiTextRenderer::HAlign::Right });
    cols.push_back ({ L"Modified", 0, false, DxuiTextRenderer::HAlign::Left });

    m_list.SetDpi           (m_dpi);
    m_list.SetTheme         (m_theme);
    m_list.SetShowHeader    (true);
    m_list.SetColumns       (std::move (cols));
    m_list.SetPreciseAutoFit (true);
    m_list.SetKeyboardColumnNav (true);
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

    m_formatLabel.SetDpi       (m_dpi);
    m_formatLabel.SetTextRole  (DxuiTextRole::Body);
    m_formatLabel.SetText      (L"Format:");
    m_formatLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_formatDropdown.SetDpi       (m_dpi);
    m_formatDropdown.SetPopupHost (PopupHost());
    m_formatDropdown.SetItems     ({ L"WOZ", L"DSK", L"PO" });
    m_formatDropdown.SetSelected  (0);
    m_formatDropdown.SetSelect    ([this] (int index) { OnFormatChanged (index); });

    m_contentsLabel.SetDpi       (m_dpi);
    m_contentsLabel.SetTextRole  (DxuiTextRole::Body);
    m_contentsLabel.SetText      (L"Contents:");
    m_contentsLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_contentsDropdown.SetDpi       (m_dpi);
    m_contentsDropdown.SetPopupHost (PopupHost());
    m_contentsDropdown.SetSelect    ([this] (int index) { OnContentsChanged (index); });

    RebuildContentsChoices();

    m_bootableCheck.SetDpi   (m_dpi);
    m_bootableCheck.SetLabel (L"Bootable");
    m_bootableCheck.SetSingleLineLabel (true);

    m_downloadButton.SetDpi     (m_dpi);
    m_downloadButton.SetOnClick ([this] () { OnDownloadClicked(); });

    m_bootHint.SetDpi       (m_dpi);
    m_bootHint.SetTextRole  (DxuiTextRole::Body);
    m_bootHint.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    UpdateBootableRow();

    m_nameLabel.SetDpi       (m_dpi);
    m_nameLabel.SetTextRole  (DxuiTextRole::Body);
    m_nameLabel.SetText      (L"Name:");
    m_nameLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_nameInput.SetDpi   (m_dpi);
    m_nameInput.SetTheme (m_theme);
    m_nameInput.SetHwnd  (Hwnd());
    m_nameInput.SetMaxLength (128);

    {
        CreateDiskBodyPanel::Children  kids;

        kids.pathLabel     = &m_pathLabel;
        kids.list          = &m_list;
        kids.formatLabel   = &m_formatLabel;
        kids.format        = &m_formatDropdown;
        kids.contentsLabel = &m_contentsLabel;
        kids.contents      = &m_contentsDropdown;
        kids.bootable      = &m_bootableCheck;
        kids.download      = &m_downloadButton;
        kids.bootHint      = &m_bootHint;
        kids.nameLabel     = &m_nameLabel;
        kids.nameInput     = &m_nameInput;

        m_body = CreateDialogContent<CreateDiskBodyPanel>();
        m_body->Init (kids);
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
//  FormatExtension
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CreateDiskDialog::FormatExtension (DiskFormat format)
{
    switch (format)
    {
        case DiskFormat::Dsk: return L".dsk";
        case DiskFormat::Po:  return L".po";
        default:              return L".woz";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContentsCaption
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CreateDiskDialog::ContentsCaption (BlankDiskContents contents)
{
    switch (contents)
    {
        case BlankDiskContents::ProDos:      return L"ProDOS 1.1.1";
        case BlankDiskContents::Unformatted: return L"Unformatted";
        default:                             return L"DOS 3.3";
    }
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
//  RebuildContentsChoices
//
//  The Format choice drives what Contents can pair with it (WOZ takes
//  anything; DSK is DOS-order, PO is ProDOS-order), so an illegal pairing
//  is never even listed. The current selection is preserved by value when
//  it stays legal and snaps to the format's canonical filesystem when not.
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::RebuildContentsChoices()
{
    std::vector<std::wstring>  captions;
    int                        selected = 0;
    size_t                     i        = 0;



    switch (m_format)
    {
        case DiskFormat::Dsk:
            m_contentsChoices = { BlankDiskContents::Dos33, BlankDiskContents::Unformatted };
            break;

        case DiskFormat::Po:
            m_contentsChoices = { BlankDiskContents::ProDos, BlankDiskContents::Unformatted };
            break;

        default:
            m_contentsChoices = { BlankDiskContents::Dos33, BlankDiskContents::ProDos,
                                  BlankDiskContents::Unformatted };
            break;
    }

    for (i = 0; i < m_contentsChoices.size(); i++)
    {
        captions.push_back (ContentsCaption (m_contentsChoices[i]));

        if (m_contentsChoices[i] == m_contents)
        {
            selected = (int) i;
        }
    }

    m_contents = m_contentsChoices[(size_t) selected];

    m_contentsDropdown.SetItems    (captions);
    m_contentsDropdown.SetSelected (selected);
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
        case 1:  m_format = DiskFormat::Dsk; break;
        case 2:  m_format = DiskFormat::Po;  break;
        default: m_format = DiskFormat::Woz; break;
    }

    RebuildContentsChoices();
    UpdateBootableRow();

    m_nameInput.SetText (ReplaceExtension (m_nameInput.Text(), FormatExtension (m_format)));

    if (m_model != nullptr)
    {
        m_model->SetExtensionFilter (FormatExtension (m_format));
        RefreshListing();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnContentsChanged
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::OnContentsChanged (int index)
{
    if (index >= 0 && index < (int) m_contentsChoices.size())
    {
        m_contents = m_contentsChoices[(size_t) index];
    }

    UpdateBootableRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateBootableRow
//
//  Three states: raw media cannot boot (toggle disabled, no affordance);
//  the OS master is cached (toggle live); or the master is missing (toggle
//  disabled, an explicit Download button plus the reason -- FR-017).
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::UpdateBootableRow()
{
    bool          formatted = (m_contents != BlankDiskContents::Unformatted);
    bool          available = false;
    std::wstring  os        = ContentsCaption (m_contents);



    if (formatted && m_payloadAvailable)
    {
        available = m_payloadAvailable (m_contents);
    }

    if (!formatted)
    {
        m_bootableCheck.SetChecked (false);
        m_bootableCheck.SetEnabled (false);
        m_downloadButton.SetVisible (false);
        m_bootHint.SetText (L"Raw media cannot carry an operating system.");
    }
    else if (available)
    {
        m_bootableCheck.SetEnabled (true);
        m_downloadButton.SetVisible (false);
        m_bootHint.SetText (L"Installs " + os + L" from the downloaded master disk.");
    }
    else
    {
        m_bootableCheck.SetChecked (false);
        m_bootableCheck.SetEnabled (false);
        m_downloadButton.SetVisible (true);
        m_downloadButton.SetEnabled (true);
        m_downloadButton.SetLabel (L"Download " + os + L"...");
        m_bootHint.SetText (L"The " + os + L" master disk is not downloaded yet.");
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
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::OnRowActivated (int row)
{
    HRESULT  hr = S_OK;



    if (m_model == nullptr || row < 0 || row >= (int) m_model->Entries().size())
    {
        return;
    }

    if (m_model->Entries()[(size_t) row].isFolder)
    {
        hr = m_model->NavigateInto ((size_t) row);

        if (SUCCEEDED (hr))
        {
            RefreshListing();
            Invalidate();
        }
    }
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
        const wchar_t * ext     = FormatExtension (m_format);
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
            m_result.spec.format   = m_format;
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
//  Local "YYYY-MM-DD HH:MM" from a Unix-seconds stamp; empty when the
//  entry carries no stamp.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CreateDiskDialog::FormatModified (int64_t modifiedUnix)
{
    constexpr size_t  kStampChars         = 32;
    wchar_t           buffer[kStampChars] = {};
    tm                local               = {};
    time_t            when                = (time_t) modifiedUnix;



    if (modifiedUnix == 0 || localtime_s (&local, &when) != 0)
    {
        return std::wstring();
    }

    swprintf_s (buffer, L"%04d-%02d-%02d %02d:%02d",
                local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                local.tm_hour, local.tm_min);

    return buffer;
}
