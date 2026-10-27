#include "Pch.h"

#include "CassoExplorer/Actions/ActionsCopy.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Volume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::ActionsCopy
//
////////////////////////////////////////////////////////////////////////////////

ActionsCopy::ActionsCopy (CassoExplorerActions & actions)
    : m_actions (actions)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::PasteVirtualFiles
//
//  A folder in the descriptor is made, and its files follow it by their
//  relative names. One file that cannot be read is reported and the rest
//  are still written.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsCopy::PasteVirtualFiles (IDataObject * data, const std::wstring & hostFolder, const ActionsTypes::CollisionFn & collision,
                                                                     std::vector<std::wstring> & outCreated)
{
    ActionsTypes::Outcome           outcome;
    FORMATETC                       format    = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM                       medium    = {};
    const FILEGROUPDESCRIPTORW    * group     = nullptr;
    std::vector<FILEDESCRIPTORW>    items;
    std::vector<std::wstring>       hostNames;
    int                             index     = 0;
    int                             count     = 0;



    outCreated.clear();

    outcome.hr = data->GetData (&format, &medium);

    if (FAILED (outcome.hr))
    {
        return outcome;
    }

    group = (const FILEGROUPDESCRIPTORW *) GlobalLock (medium.hGlobal);

    for (UINT i = 0; group != nullptr && i < group->cItems; i++)
    {
        items.push_back (group->fgd[i]);
    }

    GlobalUnlock (medium.hGlobal);
    ReleaseStgMedium (&medium);

    for (const FILEDESCRIPTORW & item : items)
    {
        bool  folder = (item.dwFlags & FD_ATTRIBUTES) != 0 && (item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        hostNames.push_back (folder ? std::wstring() : std::wstring (item.cFileName));
    }

    count = CountCollisions (hostFolder, hostNames);

    for (size_t i = 0; i < items.size(); i++)
    {
        std::wstring       hostPath;
        std::vector<Byte>  bytes;
        HRESULT            hr       = S_OK;

        if (hostNames[i].empty())
        {
            hostPath = CassoExplorerBrowser::JoinPath (hostFolder, items[i].cFileName);
            hr       = m_actions.m_fs.DirectoryExists (hostPath) ? S_OK : m_actions.m_fs.MakeDirectory (hostPath);
        }
        else
        {
            hostPath = ResolveCopyOutPath (hostFolder, hostNames[i], collision, index, count);

            if (hostPath.empty())
            {
                outcome.skipped = true;
                continue;
            }

            hr = ReadFileContents (data, (LONG) i, items[i], bytes);

            if (SUCCEEDED (hr))
            {
                hr = m_actions.m_fs.WriteAllText (hostPath, std::string (bytes.begin(), bytes.end()));
            }

            if (SUCCEEDED (hr))
            {
                outcome.written++;
                outCreated.push_back (hostPath);
            }
        }

        if (FAILED (hr))
        {
            outcome.hr       = hr;
            outcome.message += std::format (L"{}Unable to copy {}: {}", outcome.message.empty() ? L"" : L"\n", CassoExplorerActions::GetLeafName (hostPath),
                                            ChangePrompt::DescribeError (hr));
        }
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::IsCassoCopy
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsCopy::IsCassoCopy (IDataObject * data)
{
    FORMATETC  descriptor = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    FORMATETC  entries    = { (CLIPFORMAT) RegisterClipboardFormatA (DragPayload::kPrivateFormatName), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    FORMATETC  drive      = { (CLIPFORMAT) RegisterClipboardFormatW (CassoExplorerDragOut::kDriveFormatName), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    FORMATETC  files      = { (CLIPFORMAT) RegisterClipboardFormatW (CassoExplorerDragOut::kFilesFormatName), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };



    return data != nullptr && data->QueryGetData (&descriptor) == S_OK
        && (data->QueryGetData (&files) == S_OK || data->QueryGetData (&entries) == S_OK || data->QueryGetData (&drive) == S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::ReadFileContents
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsCopy::ReadFileContents (IDataObject * data, LONG index, const FILEDESCRIPTORW & item, std::vector<Byte> & outBytes)
{
    HRESULT          hr     = S_OK;
    FORMATETC        format = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILECONTENTS), nullptr, DVASPECT_CONTENT, index, TYMED_HGLOBAL | TYMED_ISTREAM };
    STGMEDIUM        medium = {};
    const Byte     * locked = nullptr;
    STATSTG          stat   = {};
    LARGE_INTEGER    start  = {};
    ULONG            read   = 0;
    bool             sized  = (item.dwFlags & FD_FILESIZE) != 0;
    ULONGLONG        size   = ((ULONGLONG) item.nFileSizeHigh << 32) | item.nFileSizeLow;



    outBytes.clear();

    hr = data->GetData (&format, &medium);
    CHR (hr);

    if ((medium.tymed & TYMED_HGLOBAL) != 0)
    {
        locked = (const Byte *) GlobalLock (medium.hGlobal);
        CBREx (locked != nullptr, E_OUTOFMEMORY);

        //  Global memory can be larger than asked for, so the descriptor's
        //  size is the file's.
        outBytes.assign (locked, locked + (sized ? (std::min) ((SIZE_T) size, GlobalSize (medium.hGlobal)) : GlobalSize (medium.hGlobal)));
        GlobalUnlock (medium.hGlobal);
    }
    else
    {
        CBREx ((medium.tymed & TYMED_ISTREAM) != 0 && medium.pstm != nullptr, DV_E_TYMED);

        hr = medium.pstm->Stat (&stat, STATFLAG_NONAME);
        CHR (hr);

        hr = medium.pstm->Seek (start, STREAM_SEEK_SET, nullptr);
        CHR (hr);

        outBytes.resize ((size_t) stat.cbSize.QuadPart);

        if (!outBytes.empty())
        {
            hr = medium.pstm->Read (outBytes.data(), (ULONG) outBytes.size(), &read);
            CHR (hr);

            outBytes.resize (read);
        }
    }

Error:
    ReleaseStgMedium (&medium);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::ExtractImage
//
//  Each file takes the host name and conversion of a copy out of the image,
//  in the host file style the Options dialog sets. A folder of that name that
//  is there already is left alone, and the files go into "name (2)".
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsCopy::ExtractImage (const std::wstring & imagePath, HostFileNaming::Style style, std::wstring & outFolder)
{
    ActionsTypes::Outcome   outcome;
    std::filesystem::path   path    = imagePath;
    std::string             image   = TextEncoding::WideToNarrow (imagePath);
    VolumeListing           listing;
    VolumeKind              kind    = VolumeKind::Unknown;
    DiskOperations::Result  listed  = m_actions.m_browser.GetOperations().List (image, listing, kind);
    HRESULT                 hr      = S_OK;



    outFolder.clear();

    if (!listed.Succeeded())
    {
        CassoExplorerActions::Append (outcome, listed);
        return outcome;
    }

    outFolder = MakeFreeFolderPath (path.parent_path().wstring(), path.stem().wstring());
    hr        = m_actions.m_fs.MakeDirectory (outFolder);

    if (FAILED (hr))
    {
        AppendMessage (outcome, hr, CassoExplorerActions::GetLeafName (outFolder) + L" could not be created.");
        return outcome;
    }

    ExtractDirectory (image, kind, std::string(), outFolder, style, 0, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::ExtractDirectory
//
////////////////////////////////////////////////////////////////////////////////

void ActionsCopy::ExtractDirectory (const std::string & image, VolumeKind kind, const std::string & directory, const std::wstring & hostFolder,
                                    HostFileNaming::Style style, int depth, ActionsTypes::Outcome & inOutOutcome)
{
    VolumeListing           listing;
    VolumeKind              listedKind = kind;
    DiskOperations::Result  listed     = m_actions.m_browser.GetOperations().List (image, directory, listing, listedKind);



    if (!listed.Succeeded())
    {
        CassoExplorerActions::Append (inOutOutcome, listed);
        return;
    }

    for (const FileEntry & entry : listing.entries)
    {
        std::string             catalogPath = directory.empty() ? entry.name : directory + "/" + entry.name;
        bool                    altered     = false;
        bool                    appleSingle = style == HostFileNaming::Style::AppleSingle;
        std::wstring            hostName    = entry.isDirectory ? HostFileNaming::MakeHostFolderName (entry.name, altered)
                                                                : DragPayload::GetHostName (entry, kind, style, altered);
        std::wstring            hostPath    = entry.isDirectory ? CassoExplorerBrowser::JoinPath (hostFolder, hostName) : MakeFreeFilePath (hostFolder, hostName);
        DiskOperations::Result  result;
        AppleSingleFile         single;
        std::vector<Byte>       bytes;
        HRESULT                 hr          = S_OK;

        if (entry.isDirectory)
        {
            hr = m_actions.m_fs.MakeDirectory (hostPath);

            if (FAILED (hr))
            {
                AppendMessage (inOutOutcome, hr, hostName + L" could not be created.");
            }
            else if (depth < kMaxCopyDepth)
            {
                ExtractDirectory (image, kind, catalogPath, hostPath, style, depth + 1, inOutOutcome);
            }

            continue;
        }

        //  Written where a Copy writes it, through the same get.
        if (!appleSingle)
        {
            CassoExplorerActions::Append (inOutOutcome, m_actions.m_browser.GetOperations().Get (image, catalogPath, CassoExplorerActions::GetEncoding (entry, kind), TextEncoding::WideToNarrow (hostPath), entry.catalogIndex));
            continue;
        }

        result = m_actions.m_browser.GetOperations().Get (image, catalogPath, ActionsTypes::Encoding::Verbatim, "", entry.catalogIndex);

        if (!result.Succeeded())
        {
            CassoExplorerActions::Append (inOutOutcome, result);
            continue;
        }

        single      = DragPayload::MakeAppleSingle (entry, kind);
        single.data = std::move (result.payload);
        AppleSingleCodec::Encode (single, bytes);

        hr = m_actions.m_fs.WriteAllText (hostPath, std::string (bytes.begin(), bytes.end()));

        if (FAILED (hr))
        {
            AppendMessage (inOutOutcome, hr, hostName + L" could not be written.");
            continue;
        }

        inOutOutcome.written++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::MakeFreeFolderPath
//
//  `name` in `parent`, or "name (2)", "name (3)" and so on, the first that
//  nothing is called.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsCopy::MakeFreeFolderPath (const std::wstring & parent, const std::wstring & name)
{
    std::wstring  path = CassoExplorerBrowser::JoinPath (parent, name);



    for (int copy = 2; m_actions.m_fs.Exists (path) || m_actions.m_fs.DirectoryExists (path); copy++)
    {
        path = CassoExplorerBrowser::JoinPath (parent, std::format (L"{} ({})", name, copy));
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::MakeFreeFilePath
//
//  `name` in `folder`, or with " (2)", " (3)" and so on after its stem, the
//  first that nothing is called, so a copy out never replaces a file. The
//  number goes before the first period or hash, so the suffix with the
//  file's type parses.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsCopy::MakeFreeFilePath (const std::wstring & folder, const std::wstring & name)
{
    size_t        split = name.find_first_of (L".#");
    std::wstring  stem  = name.substr (0, split);
    std::wstring  rest  = (split == std::wstring::npos) ? std::wstring() : name.substr (split);
    std::wstring  path  = CassoExplorerBrowser::JoinPath (folder, name);



    for (int copy = 2; m_actions.m_fs.Exists (path) || m_actions.m_fs.DirectoryExists (path); copy++)
    {
        path = CassoExplorerBrowser::JoinPath (folder, std::format (L"{} ({}){}", stem, copy, rest));
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::CountCollisions
//
//  How many of the names are files in the folder already; an empty name is
//  a directory, which is not copied.
//
////////////////////////////////////////////////////////////////////////////////

int ActionsCopy::CountCollisions (const std::wstring & hostFolder, const std::vector<std::wstring> & hostNames)
{
    int  count = 0;



    for (const std::wstring & name : hostNames)
    {
        bool  taken = !name.empty() && m_actions.m_fs.Exists (CassoExplorerBrowser::JoinPath (hostFolder, name));

        count += taken ? 1 : 0;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::ResolveCopyOutPath
//
//  The name's path in the folder when nothing is there, and otherwise the
//  collision choice: that path to write over the file, a numbered one to
//  keep both, or none to skip the copy. With no choice to ask for, both are
//  kept. A file an earlier copy in this one wrote counts as well, so the
//  count grows to cover it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsCopy::ResolveCopyOutPath (const std::wstring & hostFolder, const std::wstring & hostName, const ActionsTypes::CollisionFn & collision,
                                              int & inOutIndex, int count)
{
    std::wstring             path   = CassoExplorerBrowser::JoinPath (hostFolder, hostName);
    bool                     taken  = m_actions.m_fs.Exists (path);
    ActionsTypes::Collision  choice = ActionsTypes::Collision::KeepBoth;



    if (taken)
    {
        inOutIndex++;
        choice = collision ? collision (path, inOutIndex, std::max (count, inOutIndex)) : ActionsTypes::Collision::KeepBoth;
    }

    if (taken && choice == ActionsTypes::Collision::Skip)
    {
        path.clear();
    }
    else if (taken && choice == ActionsTypes::Collision::KeepBoth)
    {
        path = MakeFreeFilePath (hostFolder, hostName);
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::PutFiles
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsCopy::PutFiles (const std::vector<std::wstring> & hostPaths, const ActionsTypes::AddressFn & askAddress)
{
    ActionsTypes::Outcome  outcome;



    if (m_actions.m_browser.IsAppleDriveLocation())
    {
        return m_actions.m_appleDrive.PutOnAppleDrive (hostPaths, askAddress);
    }

    if (!m_actions.m_browser.IsImageLocation())
    {
        outcome.hr      = E_UNEXPECTED;
        outcome.message = L"Open a disk image to put files into it.";
        return outcome;
    }

    return PutInto (m_actions.m_browser.GetLocation().path, m_actions.m_browser.GetVolumeKind(), m_actions.m_browser.GetLocation().innerPath, hostPaths, askAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::PutInto
//
//  The image is reported written once, after everything has gone in.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsCopy::PutInto (
    const std::wstring               & imagePath,
    VolumeKind                         kind,
    const std::string                & directory,
    const std::vector<std::wstring>  & hostPaths,
    const ActionsTypes::AddressFn                  & askAddress,
    ActionsTypes::Conversion                         conversion)
{
    ActionsTypes::Outcome  outcome;



    PutItems (TextEncoding::WideToNarrow (imagePath), kind, directory, hostPaths, askAddress, conversion, outcome);
    KeepCreatedIn (outcome, directory);

    if (outcome.written > 0)
    {
        m_actions.FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::AppendMessage
//
////////////////////////////////////////////////////////////////////////////////

void ActionsCopy::AppendMessage (ActionsTypes::Outcome & inOutOutcome, HRESULT hr, const std::wstring & message)
{
    inOutOutcome.hr       = hr;
    inOutOutcome.message += (inOutOutcome.message.empty() ? L"" : L"\n") + message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::AppendWarning
//
//  A warning about a copy that went ahead: the outcome stays a success,
//  marked damaged.
//
////////////////////////////////////////////////////////////////////////////////

void ActionsCopy::AppendWarning (ActionsTypes::Outcome & inOutOutcome, const std::wstring & warning)
{
    inOutOutcome.damaged  = true;
    inOutOutcome.warning += (inOutOutcome.warning.empty() ? L"" : L"\n") + warning;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::PutItems
//
////////////////////////////////////////////////////////////////////////////////

void ActionsCopy::PutItems (
    const std::string                & image,
    VolumeKind                         kind,
    const std::string                & directory,
    const std::vector<std::wstring>  & hostPaths,
    const ActionsTypes::AddressFn                  & askAddress,
    ActionsTypes::Conversion                         conversion,
    ActionsTypes::Outcome                          & inOutOutcome)
{
    for (const std::wstring & hostPath : hostPaths)
    {
        std::string            content;
        std::vector<Byte>      bytes;
        ActionsTypes::PutPlan  plan;
        HRESULT                hr      = S_OK;

        if (IsHostFolder (hostPath))
        {
            PutFolder (image, kind, directory, hostPath, askAddress, conversion, inOutOutcome);
            continue;
        }

        hr = m_actions.m_fs.ReadAllText (hostPath, content);

        if (FAILED (hr))
        {
            AppendMessage (inOutOutcome, hr, CassoExplorerActions::GetLeafName (hostPath) + L" could not be read.");
            continue;
        }

        bytes.assign (content.begin(), content.end());
        plan = (conversion == ActionsTypes::Conversion::ByContent) ? CassoExplorerActions::PlanPut (hostPath, bytes, kind)
                                                     : CassoExplorerActions::PlanForced (hostPath, bytes, kind, conversion);

        if (!plan.refusal.empty())
        {
            AppendMessage (inOutOutcome, E_FAIL, CassoExplorerActions::GetLeafName (hostPath) + L": " + plan.refusal);
            continue;
        }

        if (plan.guessedAddress && askAddress && !askAddress (CassoExplorerActions::GetLeafName (hostPath), plan.loadAddress, plan.loadAddress))
        {
            inOutOutcome.skipped = true;
            continue;
        }

        //  Into the directory named, which is the volume's own when empty,
        //  under a name no entry there has.
        plan.catalogName = CassoExplorerActions::MakeFreeEntryName (plan.catalogName, m_actions.GetEntryNames (image, directory), kind, false);
        plan.catalogName = directory.empty() ? plan.catalogName : directory + "/" + plan.catalogName;

        if (plan.usePayload)
        {
            Record (inOutOutcome, m_actions.m_browser.GetOperations().WritePayload (image, plan.catalogName, plan.payload), plan.catalogName);
        }
        else
        {
            Record (inOutOutcome, m_actions.m_browser.GetOperations().Put (image, TextEncoding::WideToNarrow (hostPath), plan.catalogName,
                                                                           plan.typeName, plan.hasLoadAddress, plan.loadAddress, plan.encoding),
                    plan.catalogName);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::Record
//
//  Appends the write's result and, when it made the entry, the entry.
//
////////////////////////////////////////////////////////////////////////////////

void ActionsCopy::Record (ActionsTypes::Outcome & inOutOutcome, const DiskOperations::Result & result, const std::string & catalogPath)
{
    CassoExplorerActions::Append (inOutOutcome, result);

    if (result.Succeeded())
    {
        inOutOutcome.created.push_back (catalogPath);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::KeepCreatedIn
//
//  The entries made directly in the directory, without those made inside a
//  directory that was made too, which go with it.
//
////////////////////////////////////////////////////////////////////////////////

void ActionsCopy::KeepCreatedIn (ActionsTypes::Outcome & inOutOutcome, const std::string & directory)
{
    std::erase_if (inOutOutcome.created, [&directory] (const std::string & path)
    {
        size_t  slash = path.find_last_of ('/');

        return ((slash == std::string::npos) ? std::string() : path.substr (0, slash)) != directory;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::TryGetHostEntry
//
//  An item's entry in its folder's listing, which says whether it is a
//  folder and how large a file is.
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsCopy::TryGetHostEntry (const std::wstring & path, FileSystemEntry & outEntry)
{
    std::vector<FileSystemEntry>  siblings;
    std::filesystem::path         item   (path);
    std::wstring                  leaf = item.filename().wstring();
    HRESULT                       hr   = m_actions.m_fs.EnumerateAllEntries (item.parent_path().wstring(), siblings);



    if (FAILED (hr))
    {
        return false;
    }

    for (const FileSystemEntry & sibling : siblings)
    {
        if (_wcsicmp (sibling.name.c_str(), leaf.c_str()) == 0)
        {
            outEntry = sibling;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::CopyEntriesInto
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsCopy::CopyEntriesInto (
    const std::string                & sourceImage,
    VolumeKind                         sourceKind,
    const std::vector<std::string>   & catalogPaths,
    const std::wstring               & targetImage,
    VolumeKind                         targetKind,
    const std::string                & directory)
{
    ActionsTypes::Outcome  outcome;
    std::string            target  = TextEncoding::WideToNarrow (targetImage);
    bool                   same    = _stricmp (sourceImage.c_str(), target.c_str()) == 0;



    for (const std::string & path : catalogPaths)
    {
        //  A folder copied into itself, or into a folder inside it, would copy
        //  its copies again until the depth limit or a full disk stopped it.
        //  File Explorer does not allow it either.
        bool  inside = same && (_stricmp (directory.c_str(), path.c_str()) == 0 || IsInsideFolder (directory, path));

        if (inside)
        {
            AppendMessage (outcome, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER),
                           TextEncoding::NarrowToWide (path) + L": a folder cannot be copied into itself.");
            continue;
        }

        CopyEntry (sourceImage, sourceKind, path, target, targetKind, directory, 0, outcome);
    }

    KeepCreatedIn (outcome, directory);

    if (outcome.written > 0)
    {
        m_actions.FinishWrite (targetImage);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::IsInsideFolder
//
//  Whether the catalog path `directory` is below `folder`, without regard
//  to case, the way both file systems compare entries.
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsCopy::IsInsideFolder (const std::string & directory, const std::string & folder)
{
    return directory.size() > folder.size()
        && directory[folder.size()] == '/'
        && _strnicmp (directory.c_str(), folder.c_str(), folder.size()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::CopyEntry
//
//  A path that lists is a directory; anything else is read as a file.
//
////////////////////////////////////////////////////////////////////////////////

void ActionsCopy::CopyEntry (
    const std::string    & sourceImage,
    VolumeKind             sourceKind,
    const std::string    & catalogPath,
    const std::string    & targetImage,
    VolumeKind             targetKind,
    const std::string    & directory,
    int                    depth,
    ActionsTypes::Outcome              & inOutOutcome)
{
    size_t                  slash   = catalogPath.find_last_of ('/');
    std::string             parent  = (slash == std::string::npos) ? std::string() : catalogPath.substr (0, slash);
    //  A copy beside its original is named as Explorer's is; anything else
    //  that collides takes the next free number. Below the top, the entries
    //  go into a directory just made, where nothing collides.
    bool                    beside  = sourceImage == targetImage && _stricmp (parent.c_str(), directory.c_str()) == 0;
    std::string             leaf    = (slash == std::string::npos) ? catalogPath : catalogPath.substr (slash + 1);
    std::string             name    = (depth == 0) ? CassoExplorerActions::MakeFreeEntryName (leaf, m_actions.GetEntryNames (targetImage, directory), targetKind, beside) : leaf;
    std::string             target  = directory.empty() ? name : directory + "/" + name;
    VolumeListing           listing;
    VolumeKind              listed  = VolumeKind::Unknown;
    DiskOperations::Result  result;
    FilePayload             payload;
    Byte                    mapped  = 0;
    DiskOperations::Result  listedResult;



    //  A failure, not a silent skip: a move takes the source away only when
    //  the whole copy succeeded.
    if (depth > kMaxCopyDepth)
    {
        AppendMessage (inOutOutcome, E_FAIL, TextEncoding::NarrowToWide (catalogPath) + L": folders nested this deep are not copied.");
        return;
    }

    listedResult = (sourceKind == VolumeKind::ProDos) ? m_actions.m_browser.GetOperations().List (sourceImage, catalogPath, listing, listed) : DiskOperations::Result { E_NOTIMPL };

    if (listedResult.Succeeded())
    {
        if (listedResult.listingDamaged)
        {
            AppendWarning (inOutOutcome, TextEncoding::NarrowToWide (leaf) + L": the folder's catalog is damaged, so some of its entries may not have been copied.");
        }

        if (targetKind != VolumeKind::ProDos)
        {
            AppendMessage (inOutOutcome, E_FAIL, TextEncoding::NarrowToWide (leaf) + L": DOS 3.3 has no folders, so a folder cannot go on this disk.");
            return;
        }

        Record (inOutOutcome, m_actions.m_browser.GetOperations().Mkdir (targetImage, target), target);

        for (const FileEntry & entry : listing.entries)
        {
            CopyEntry (sourceImage, sourceKind, catalogPath + "/" + entry.name, targetImage, targetKind, target, depth + 1, inOutOutcome);
        }

        return;
    }

    result = m_actions.m_browser.GetOperations().Read (sourceImage, catalogPath, payload);

    if (!result.Succeeded())
    {
        CassoExplorerActions::Append (inOutOutcome, result);
        return;
    }

    //  Salvaged: the copy is made, with zeros where the disk could not be read.
    if (result.unreadSectors > 0)
    {
        AppendWarning (inOutOutcome, std::format (L"{}: {} {} could not be read from the disk, and {} zeros in the copy.", TextEncoding::NarrowToWide (leaf),
                                                  result.unreadSectors, result.unreadSectors == 1 ? L"sector" : L"sectors", result.unreadSectors == 1 ? L"is" : L"are"));
    }

    //  The type means the same thing on the other file system.
    if (sourceKind == VolumeKind::Dos33 && targetKind == VolumeKind::ProDos)
    {
        payload.type = HostFileNaming::MapDos33ToProDosType (payload.type);
    }
    else if (sourceKind == VolumeKind::ProDos && targetKind == VolumeKind::Dos33)
    {
        payload.type = HostFileNaming::TryMapProDosToDos33 (payload.type, mapped) ? mapped : Dos33Volume::kTypeBinary;
    }

    Record (inOutOutcome, m_actions.m_browser.GetOperations().WritePayload (targetImage, target, payload), target);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::IsHostFolder
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsCopy::IsHostFolder (const std::wstring & path)
{
    FileSystemEntry  entry;
    bool             found = TryGetHostEntry (path, entry);



    return found && entry.isFolder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::PutFolder
//
////////////////////////////////////////////////////////////////////////////////

void ActionsCopy::PutFolder (
    const std::string    & image,
    VolumeKind             kind,
    const std::string    & directory,
    const std::wstring   & hostFolder,
    const ActionsTypes::AddressFn      & askAddress,
    ActionsTypes::Conversion             conversion,
    ActionsTypes::Outcome              & inOutOutcome)
{
    std::vector<FileSystemEntry>  entries;
    std::vector<std::wstring>     children;
    std::string                   name   = CassoExplorerActions::MakeFreeEntryName (CassoExplorerActions::MakeCatalogName (CassoExplorerActions::GetLeafName (hostFolder), kind), m_actions.GetEntryNames (image, directory), kind, false);
    std::string                   target = directory.empty() ? name : directory + "/" + name;
    HRESULT                       hr     = S_OK;



    if (kind != VolumeKind::ProDos)
    {
        AppendMessage (inOutOutcome, E_FAIL, CassoExplorerActions::GetLeafName (hostFolder) + L": DOS 3.3 has no folders, so a folder cannot go on this disk.");
        return;
    }

    hr = m_actions.m_fs.EnumerateEntries (hostFolder, entries);

    if (FAILED (hr))
    {
        AppendMessage (inOutOutcome, hr, CassoExplorerActions::GetLeafName (hostFolder) + L" could not be read.");
        return;
    }

    Record (inOutOutcome, m_actions.m_browser.GetOperations().Mkdir (image, target), target);

    for (const FileSystemEntry & entry : entries)
    {
        children.push_back (CassoExplorerBrowser::JoinPath (hostFolder, entry.name));
    }

    PutItems (image, kind, target, children, askAddress, conversion, inOutOutcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::CountUnitsForFile
//
//  ProDOS counts 512-byte blocks: one for a file up to a block, and an index
//  block for each 256 data blocks beyond that. DOS 3.3 counts 256-byte
//  sectors: the data, plus a track/sector list for each 122 of them.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ActionsCopy::CountUnitsForFile (VolumeKind kind, uint64_t sizeBytes)
{
    uint64_t  unit  = (kind == VolumeKind::ProDos) ? kProDosBlockBytes : kDos33SectorBytes;
    uint64_t  data  = (std::max) ((uint64_t) 1, (sizeBytes + unit - 1) / unit);
    uint64_t  index = 0;



    if (kind == VolumeKind::ProDos)
    {
        index = (data <= 1) ? 0 : (data + kProDosIndexEntries - 1) / kProDosIndexEntries;
    }
    else
    {
        index = (data + kDos33ListEntries - 1) / kDos33ListEntries;
    }

    return data + index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::CountUnitsForFolder
//
//  A ProDOS directory's key block. DOS 3.3 holds no folders.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ActionsCopy::CountUnitsForFolder (VolumeKind kind)
{
    return (kind == VolumeKind::ProDos) ? 1 : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::GetNewDiskFreeUnits
//
//  A 140K disk after its file system's own structures: ProDOS keeps its boot
//  blocks, volume directory and bitmap; DOS 3.3 keeps its three tracks and
//  the catalog track.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ActionsCopy::GetNewDiskFreeUnits (VolumeKind kind)
{
    return (kind == VolumeKind::ProDos) ? kProDosNewDiskFreeBlocks : kDos33NewDiskFreeSectors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy::CheckFitsNewDisk
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsCopy::CheckFitsNewDisk (VolumeKind kind, const std::vector<std::wstring> & hostPaths)
{
    uint64_t                   needed  = 0;
    uint64_t                   free    = GetNewDiskFreeUnits (kind);
    std::vector<std::wstring>  pending = hostPaths;



    while (!pending.empty())
    {
        std::wstring                  path = pending.back();
        std::vector<FileSystemEntry>  entries;
        FileSystemEntry               file;
        HRESULT                       hr   = S_OK;

        pending.pop_back();

        if (IsHostFolder (path))
        {
            if (kind != VolumeKind::ProDos)
            {
                return CassoExplorerActions::GetLeafName (path) + L" is a folder, and DOS 3.3 has no folders.";
            }

            needed += CountUnitsForFolder (kind);

            hr = m_actions.m_fs.EnumerateEntries (path, entries);
            IGNORE_RETURN_VALUE (hr, S_OK);

            for (const FileSystemEntry & entry : entries)
            {
                pending.push_back (CassoExplorerBrowser::JoinPath (path, entry.name));
            }

            continue;
        }

        if (TryGetHostEntry (path, file))
        {
            needed += CountUnitsForFile (kind, file.sizeBytes);
        }
    }

    if (needed > free)
    {
        return std::format (L"The selected files take up {} {}, and a new disk has room for {}.",
                            needed, (kind == VolumeKind::ProDos) ? L"blocks" : L"sectors", free);
    }

    return std::wstring();
}
