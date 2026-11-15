#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMode.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Panes/ExplorerPickerBar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::ExplorerPickerMode
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPickerMode::ExplorerPickerMode (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::Start
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMode::Start()
{
    if (!m_context.IsPicker())
    {
        return;
    }

    m_scope = PickerScope (*m_context.app.picker);

    m_context.browser.SetRowScope ([this] (const CatalogRow & row) { return m_scope.Passes (row); });
    m_context.browser.SetMountedImages (m_context.app.picker->mounted);
    m_context.browser.SetStockDisks (m_context.app.picker->stockDisks);
    m_context.browser.GetTreeModel().SetStockDisksShown (!m_context.app.picker->stockDisks.empty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::TryGetRowFile
//
//  A file row of a folder on the disk, or of the Casso node's list or the
//  stock disks' list, where each row has its file's path. A row inside a
//  disk image is no file to pick.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerMode::TryGetRowFile (int row, std::wstring & outPath) const
{
    const std::vector<CatalogRow> &  rows     = m_context.browser.GetRows();
    Location                         location = m_context.browser.GetLocation();
    bool                             inFolder = location.kind == Location::Kind::HostFolder && !location.path.empty();
    bool                             inCasso  = location.kind == Location::Kind::Root && (location.path == TreeModel::kCassoRootId || location.path == TreeModel::kStockDisksId);



    outPath.clear();

    if (row < 0 || (size_t) row >= rows.size() || rows[(size_t) row].isDirectory || !(inFolder || inCasso))
    {
        return false;
    }

    outPath = !rows[(size_t) row].hostPath.empty() ? rows[(size_t) row].hostPath
            : inFolder                             ? CassoExplorerBrowser::JoinPath (location.path, rows[(size_t) row].name)
                                                   : std::wstring();

    return !outPath.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::TryGetSelectedFile
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerMode::TryGetSelectedFile (std::wstring & outPath) const
{
    const std::vector<int> &  selected = m_context.browser.GetSelectedRows();



    outPath.clear();

    return m_context.IsPicker() && selected.size() == 1 && TryGetRowFile (selected[0], outPath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::CanConfirm
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerMode::CanConfirm() const
{
    std::wstring  path;



    return TryGetSelectedFile (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::Confirm
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMode::Confirm()
{
    const std::vector<int> &  selected = m_context.browser.GetSelectedRows();
    std::wstring              path;



    if (!TryGetSelectedFile (path))
    {
        return;
    }

    //  A stock disk not yet on the disk is downloaded first. A download
    //  that fails is reported by the request, and the picker stays open.
    if (IsStockDiskToFetch (selected[0]) && (!m_context.app.picker->onFetchStock || !m_context.app.picker->onFetchStock (path)))
    {
        return;
    }

    ConfirmPath (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::IsStockDiskToFetch
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerMode::IsStockDiskToFetch (int row) const
{
    const std::vector<CatalogRow> &  rows = m_context.browser.GetRows();



    return row >= 0 && (size_t) row < rows.size() && rows[(size_t) row].isStockDisk && !rows[(size_t) row].stockDownloaded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::ConfirmPath
//
//  A drive's picker stays open when the request does not take the image.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMode::ConfirmPath (const std::wstring & path)
{
    const PickerRequest *  request = m_context.app.picker;
    bool                   isTaken = true;



    if (request->form == PickerForm::DriveWindow && request->onConfirm)
    {
        isTaken = request->onConfirm (path);
    }

    if (!isTaken)
    {
        return;
    }

    if (m_context.app.outcome != nullptr)
    {
        m_context.app.outcome->isConfirmed = true;
        m_context.app.outcome->path        = path;
    }

    m_context.window.CloseWindow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::CreateNew
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMode::CreateNew()
{
    const PickerRequest *  request   = m_context.app.picker;
    std::wstring           path;
    bool                   isCreated = false;



    if (!CanCreateNew())
    {
        return;
    }

    isCreated = request->onCreateNew ? request->onCreateNew (path) : m_context.commandManager.CreateDiskForPicker (path);

    if (isCreated && !path.empty())
    {
        ConfirmPath (path);
    }
    else if (isCreated)
    {
        //  The caller's flow inserted what it created.
        if (m_context.app.outcome != nullptr)
        {
            m_context.app.outcome->isConfirmed = true;
        }

        m_context.window.CloseWindow();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::CanCreateNew
//
//  A drive's picker creates what the drive takes: a disk through the New
//  disk image dialog, or what the request's create flow makes.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerMode::CanCreateNew() const
{
    const PickerRequest *  request = m_context.app.picker;



    if (request == nullptr || request->form != PickerForm::DriveWindow)
    {
        return false;
    }

    return request->onCreateNew || (request->IsForDrive() && request->receivingDrive->kind != ReceivingDriveKind::Recorder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::Cancel
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMode::Cancel()
{
    if (m_context.IsPicker())
    {
        m_context.window.CloseWindow();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::HandleActivateRow
//
//  The row is selected by the click or the key that opened it.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerMode::HandleActivateRow (int row)
{
    std::wstring  path;



    if (!m_context.IsPicker() || !TryGetRowFile (row, path))
    {
        //  A file inside a disk image isn't opened from a picker either.
        return m_context.IsPicker() && m_context.browser.IsImageLocation();
    }

    m_context.browser.SetSelectedRows ({ row });
    Confirm();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerMode::HandleKey (const DxuiKeyEvent & ev)
{
    bool  isEscape = m_context.IsPicker() && ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && !ev.ctrl && !ev.alt && !ev.shift;



    if (isEscape)
    {
        Cancel();
    }

    return isEscape;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::SetActiveFilter
//
//  The folder is listed again, by the new entry's masks.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMode::SetActiveFilter (size_t index)
{
    HRESULT  hr = S_OK;



    if (!m_context.IsPicker() || index == m_scope.GetActiveFilter() || index >= m_scope.GetFilterCount())
    {
        return;
    }

    m_scope.SetActiveFilter (index);

    hr = m_context.browser.Refresh();
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_context.listPane.FillList();
    m_context.pickerBar.Update();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::GetHeaderText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerPickerMode::GetHeaderText() const
{
    return m_context.IsPicker() ? MakeHeaderText (*m_context.app.picker) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::MakeHeaderText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerPickerMode::MakeHeaderText (const PickerRequest & request)
{
    std::wstring  accepted = request.receivingDrive.has_value() ? DriveImageRule::GetAcceptedText (*request.receivingDrive) : std::wstring();



    if (request.driveLabel.empty())
    {
        return accepted;
    }

    return accepted.empty() ? request.driveLabel : request.driveLabel + L": " + accepted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::GetConfirmLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerPickerMode::GetConfirmLabel() const
{
    const PickerRequest *  request = m_context.app.picker;



    if (request == nullptr)
    {
        return std::wstring();
    }

    return request->IsSave() ? L"Save" : (request->form == PickerForm::DriveWindow) ? L"Insert" : L"Open";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode::GetFilterLabel
//
//  An entry's label with its masks, as the Windows dialogs list them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerPickerMode::GetFilterLabel (size_t index) const
{
    const PickerRequest *  request = m_context.app.picker;



    if (request == nullptr || index >= request->filters.size())
    {
        return std::wstring();
    }

    return std::format (L"{} ({})", request->filters[index].name, request->filters[index].pattern);
}
