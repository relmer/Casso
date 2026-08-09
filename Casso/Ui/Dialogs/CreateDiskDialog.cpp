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

void CreateDiskDialog::Configure (FileBrowseModel * model, const IDxuiTheme * theme, UINT dpi)
{
    m_model = model;
    m_theme = theme;
    m_dpi   = dpi;
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
    m_list.SetOnSelectionChanged ([this] (int row)
    {
        const auto &  entries = m_model->Entries();

        if (row >= 0 && row < (int) entries.size() && !entries[(size_t) row].isFolder)
        {
            m_nameInput.SetText (entries[(size_t) row].name);
            Invalidate();
        }
    });

    m_nameLabel.SetDpi       (m_dpi);
    m_nameLabel.SetTextRole  (DxuiTextRole::Body);
    m_nameLabel.SetText      (L"Name:");
    m_nameLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_nameInput.SetDpi   (m_dpi);
    m_nameInput.SetTheme (m_theme);
    m_nameInput.SetHwnd  (Hwnd());
    m_nameInput.SetMaxLength (128);

    m_body = CreateDialogContent<CreateDiskBodyPanel>();
    m_body->Init (&m_pathLabel, &m_list, &m_nameLabel, &m_nameInput);

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
//  RefreshFromModel
//
////////////////////////////////////////////////////////////////////////////////

void CreateDiskDialog::RefreshFromModel()
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

    m_nameInput.SetText (m_model->UniqueDefaultName (L"Blank Disk"));

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
            m_result.targetPath = m_model->ComposeTargetPath (name);
            m_result.confirmed  = true;
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

    uint64_t  kb = 0;



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
    constexpr size_t  kStampChars = 32;

    wchar_t  buffer[kStampChars] = {};
    tm       local               = {};
    time_t   when                = (time_t) modifiedUnix;



    if (modifiedUnix == 0 || localtime_s (&local, &when) != 0)
    {
        return std::wstring();
    }

    swprintf_s (buffer, L"%04d-%02d-%02d %02d:%02d",
                local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                local.tm_hour, local.tm_min);

    return buffer;
}
