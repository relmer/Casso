#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerOpen.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "Seams/IShellItemVerbs.h"
#include "Seams/IShellIcons.h"
#include "Core/AppMessages.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "Seams/IHostDialogs.h"
#include "Seams/RawDevice/RawDeviceServer.h"
#include "Seams/RawDevice/Fat32Format.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"
#include "CassoExplorer/CassoExplorerInitializeCardDialog.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::ExplorerOpen
//
////////////////////////////////////////////////////////////////////////////////

ExplorerOpen::ExplorerOpen (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::~ExplorerOpen
//
//  A card's write is never cut off, so a window closed during one waits
//  for it to end.
//
////////////////////////////////////////////////////////////////////////////////

ExplorerOpen::~ExplorerOpen()
{
    if (m_initThread.joinable())
    {
        m_initThread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::OpenSelectedEntries
//
//  Files inside an image open in the programs Windows has for them, from
//  copies in a folder of their own, as Explorer opens a file inside a zip.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::OpenSelectedEntries()
{
    std::vector<std::wstring>  paths;
    HRESULT                    hr    = CassoExplorerDragOut::WriteToTempFolder (m_context.browser, m_context.commandManager.GetNamingStyle(), paths);



    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"The files could not be read out of the image to open.", MB_ICONWARNING);
        return;
    }

    for (const std::wstring & path : paths)
    {
        hr = m_context.shellVerbs.Open (m_context.window.GetHwnd(), path);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::OpenEachSelected
//
//  Several items open each on its own, as Explorer's do: a folder or an image
//  in a tab of its own, a file in its program. All are read before the first
//  tab opens, since a new tab lists something else.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::OpenEachSelected()
{
    HRESULT                    hr      = S_OK;
    std::vector<Location>      places;
    std::vector<std::wstring>  files;
    Location                   place;
    std::wstring               path;
    bool                       written = m_context.browser.PrepareCassoLinkItems();



    for (int row : m_context.browser.GetSelectedRows())
    {
        if (m_context.browser.TryGetRowLocation (row, place))
        {
            places.push_back (place);
        }
        else if (written && !m_context.browser.IsImageLocation() && m_context.browser.TryGetRowPath (row, path))
        {
            files.push_back (path);
        }
    }

    for (const std::wstring & file : files)
    {
        hr = m_context.shellVerbs.Open (m_context.window.GetHwnd(), file);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (const Location & each : places)
    {
        m_context.browser.OpenInNewTab (each);
    }

    if (!places.empty())
    {
        m_context.listPane.FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::PickPartitionImage
//
//  A card has no folder on the PC to write beside, so the image goes where
//  the user picks.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerOpen::PickPartitionImage (const std::wstring & partition, std::wstring & outTarget)
{
    FileDialogSpec         spec;
    std::filesystem::path  target;
    bool                   picked = false;
    int                    drive  = 0;
    int                    index  = 0;
    HRESULT                hr     = S_OK;



    outTarget.clear();

    if (PhysicalCards::ParsePath (TextEncoding::WideToUtf8 (partition), drive, index) != PhysicalCards::PathKind::Partition)
    {
        return true;
    }

    spec.defaultExtension = L"po";
    spec.defaultFileName  = std::format (L"Physical drive {} partition {}.po", drive, index);
    spec.askToReplace     = true;
    spec.title            = std::format (L"Create image from partition {}", index);

    hr = m_context.dialogs.PickFileToSave (m_context.window.GetHwnd(), spec, target, picked);

    if (FAILED (hr) || !picked)
    {
        return false;
    }

    outTarget = target.wstring();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::SetCardWriteConfirmed
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::SetCardWriteConfirmed (bool confirmed)
{
    if (m_context.app.cards != nullptr)
    {
        m_context.app.cards->SetConfirmed (confirmed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::InitializeCard
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::InitializeCard (const std::wstring & cardPath)
{
    HRESULT                            hr        = S_OK;
    int                                drive     = 0;
    int                                partition = 0;
    PhysicalDrive                      found;
    std::wstring                       question;
    std::wstring                       contents;
    uint64_t                           layout    = 0;
    std::unique_ptr<CardLayoutModel>   model;
    CardLayoutModel::Adapter           adapter   = CardLayoutModel::Adapter::Unsure;
    bool                               confirmed = false;
    std::vector<PhysicalDrive>         drives;
    std::vector<WindowsVolume>         volumes;
    std::vector<PartitionEntry>        apple;
    PartitionLayoutKind                kind      = PartitionLayoutKind::None;
    int                                count     = 0;



    if (m_context.app.cards == nullptr ||
        PhysicalCards::ParsePath (TextEncoding::WideToUtf8 (cardPath), drive, partition) != PhysicalCards::PathKind::Card)
    {
        return;
    }

    hr = m_context.app.cards->ListRemovableDrives (drives);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const PhysicalDrive & each : drives)
    {
        found = each.number == drive ? each : found;
    }

    if (found.number != drive)
    {
        m_context.window.ShowMessage (L"The storage card is no longer in its drive.", MB_ICONWARNING);
        return;
    }

    hr = m_context.app.cards->ListVolumes (drive, volumes);
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  A card open already lists its Apple partitions too.
    if (m_context.app.cards->IsOpen (drive))
    {
        hr = m_context.app.cards->ListPartitions (drive, kind, apple);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    count = RawDeviceServer::CountInitialVolumes (found.bytes / IRawDevice::kBlockBytes);

    model = std::make_unique<CardLayoutModel> (found.bytes, count, Fat32Format::CanFormat (found.bytes / IRawDevice::kBlockBytes));

    if (model->GetPositionCount() == 0)
    {
        m_context.window.ShowMessage (std::format (L"{} is too small for a CFFA adapter.", PhysicalCards::FormatDrive (found)), MB_ICONWARNING);
        return;
    }

    question = std::format (L"Initialize {} for use with an Apple CFFA adapter?", PhysicalCards::FormatDrive (found));
    contents = L"This card currently contains these partitions:";

    //  A card with no partition table, as a CFFA card, is one volume to
    //  Windows, which it can't read. Its Apple partitions are listed instead.
    for (const WindowsVolume & volume : volumes)
    {
        std::wstring  where = volume.letter.empty() ? std::wstring (L"No drive letter") : volume.letter;

        if (volume.fileSystem.empty())
        {
            if (apple.empty())
            {
                contents += std::format (L"\r\n    {} (a file system Windows doesn't recognize)", where);
            }
        }
        else if (volume.label.empty())
        {
            contents += std::format (L"\r\n    {} ({})", where, volume.fileSystem);
        }
        else
        {
            contents += std::format (L"\r\n    {} {} ({})", where, volume.label, volume.fileSystem);
        }
    }

    for (const PartitionEntry & entry : apple)
    {
        contents += L"\r\n    " + TextEncoding::NarrowToWide (PartitionAccess::GetLabel (entry, true));
    }

    if (volumes.empty() && apple.empty())
    {
        contents += L"\r\n    None that Windows or Casso recognizes";
    }

    adapter   = (CardLayoutModel::Adapter) m_context.prefs.cffaAdapter;
    confirmed = CassoExplorerInitializeCardDialog::Confirm (m_context.window.GetHwnd(), m_context.theme, question, *model, contents, adapter, layout);

    //  Saved now rather than at exit, so the next initialize starts with it
    //  however Casso Explorer ends.
    if (m_context.prefs.cffaAdapter != (int) adapter)
    {
        m_context.prefs.cffaAdapter = (int) adapter;
        m_context.window.SaveSession();
    }

    if (!confirmed)
    {
        return;
    }

    if (m_initThread.joinable())
    {
        m_context.window.ShowMessage (L"A storage card is being initialized. Initialize this card after it finishes.", MB_ICONWARNING);
        return;
    }

    StartInitialize (cardPath, found, layout, std::move (volumes));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::StartInitialize
//
//  The write runs on a separate thread, which posts ExplorerCardProgress to the
//  window at its start, at most every kProgressPostMs while it runs, and at
//  its end.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::StartInitialize (const std::wstring & cardPath, const PhysicalDrive & drive, uint64_t layout, std::vector<WindowsVolume> volumes)
{
    HWND             window = m_context.window.GetHwnd();
    PhysicalCards  * cards  = m_context.app.cards;
    int              number = drive.number;



    m_initCard        = cardPath;
    m_initDrive       = drive;
    m_initLayout      = layout;
    m_initVolumes     = std::move (volumes);
    m_initDone        = false;
    m_initResult      = S_OK;
    m_lastPostMs      = 0;
    m_navigatePending = true;

    m_initThread = std::thread ([this, window, cards, number, layout] ()
    {
        HRESULT  hr = cards->Initialize (number, window, layout, [this, window] (uint64_t, uint64_t totalBytes)
        {
            ULONGLONG  now = GetTickCount64();

            if (totalBytes == 0 || now - m_lastPostMs >= kProgressPostMs)
            {
                m_lastPostMs = now;
                PostMessageW (window, (UINT) AppMessage::ExplorerCardProgress, 0, 0);
            }
        });

        m_initResult = hr;
        m_initDone   = true;
        PostMessageW (window, (UINT) AppMessage::ExplorerCardProgress, 0, 0);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::HandleCardProgress
//
//  The card is opened in the list once its write starts, and its list is
//  refreshed with each report while it is the location in the list.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::HandleCardProgress()
{
    Location  card = Location::MakeDiskImage (m_initCard);



    if (m_initDone)
    {
        FinishInitialize();
    }
    else if (m_navigatePending)
    {
        m_navigatePending = false;
        m_context.browser.NavigateToLocation (card);
        m_context.listPane.FillList();
    }
    else if (m_context.browser.GetLocation() == card)
    {
        HRESULT  hr = m_context.browser.Refresh();

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_context.listPane.FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::FinishInitialize
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::FinishInitialize()
{
    HRESULT       hr        = m_initResult;
    HRESULT       hrRefresh = S_OK;
    std::wstring  root;



    if (m_initThread.joinable())
    {
        m_initThread.join();
    }

    m_initDone        = false;
    m_navigatePending = false;

    //  After a write declined at the UAC prompt, or one that failed, the
    //  card's list is reloaded.
    if (FAILED (hr))
    {
        hrRefresh = m_context.browser.Refresh();
        IGNORE_RETURN_VALUE (hrRefresh, S_OK);
        m_context.listPane.FillList();
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        return;
    }

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (std::format (L"Unable to initialize {} (error 0x{:08X}).", PhysicalCards::FormatDrive (m_initDrive), (uint32_t) hr), MB_ICONWARNING);
        return;
    }

    //  The drive's folders and files in the tree are from before the erase.
    for (const WindowsVolume & volume : m_initVolumes)
    {
        if (!volume.letter.empty())
        {
            std::wstring  node = TreeModel::MakeFolderId (false, volume.letter + L"\\");

            m_context.browser.GetTreeModel().Invalidate (node);
            m_context.treePane.ReloadChildren (node);
            root = root.empty() ? volume.letter + L"\\" : root;
        }
    }

    m_context.browser.GetTreeModel().Invalidate (TreeModel::kThisPcRootId);
    m_context.treePane.ReloadChildren (TreeModel::kThisPcRootId);

    //  A FAT32 card is opened at its drive letter, and a CFFA card as a card.
    if (m_initLayout == RawDeviceProtocol::kLayoutFat32 && !root.empty())
    {
        m_context.browser.NavigateToLocation (Location::MakeHostFolder (root));
    }
    else
    {
        m_context.browser.NavigateToLocation (Location::MakeDiskImage (m_initCard));
    }

    m_context.listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::AddInitializeRow
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::AddInitializeRow (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder)
{
    int           number = -1;
    bool          isRoot = folder.size() <= 3 && folder.size() >= 2 && folder[1] == L':';
    std::wstring  card;



    if (m_context.app.cards == nullptr)
    {
        return;
    }

    if (PhysicalCards::IsCardPath (folder))
    {
        card = folder;
    }
    else if (isRoot && m_context.app.cards->TryGetVolumeCard (folder, number))
    {
        card = TextEncoding::Utf8ToWide (PhysicalCards::MakeCardPath (number));
    }
    else
    {
        return;
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    m_context.menus.AddMenuCommand (items, L"&Initialize card for Apple...", [this, card]() { InitializeCard (card); });
    m_context.menus.GetMenuCommands().back()->menuImage = m_context.shellIcons.GetForKind (IShellIcons::Kind::AppleMachine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen::InitializeShownCard
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerOpen::InitializeShownCard()
{
    Location      location = m_context.browser.GetLocation();
    std::wstring  root     = location.path.substr (0, std::min<size_t> (location.path.size(), 3));
    int           number   = -1;



    if (PhysicalCards::IsCardPath (location.path))
    {
        InitializeCard (location.path);
    }
    else if (location.kind == Location::Kind::HostFolder && m_context.app.cards != nullptr && m_context.app.cards->TryGetVolumeCard (root, number))
    {
        InitializeCard (TextEncoding::Utf8ToWide (PhysicalCards::MakeCardPath (number)));
    }
}