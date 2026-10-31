#include "Pch.h"

#include "CassoExplorer/Actions/ActionsAppleDrive.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "CassoExplorer/Model/DragPayload.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "CassoExplorer/CassoExplorerBrowser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::ActionsAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsAppleDrive::ActionsAppleDrive (CassoExplorerActions & actions)
    : m_actions (actions)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::TryGetAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsAppleDrive::TryGetAppleDrive (std::string & outLink, int & outDrive, std::string & outDirectory) const
{
    Location      location = m_actions.m_browser.GetLocation();
    std::wstring  link;
    int           slot     = 0;
    bool          found    = false;



    found = location.kind == Location::Kind::AppleDrive && m_actions.m_browser.GetLinks() != nullptr
         && TreeModel::TryParseLinkId (location.path, link, slot, outDrive) && outDrive != 0;

    outLink      = TextEncoding::WideToNarrow (link);
    outDirectory = location.innerPath;

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::ToHostBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsAppleDrive::ToHostBytes (FilePayload payload, ActionsTypes::Encoding encoding, bool appleSingle, const AppleSingleFile & single, std::vector<Byte> & outBytes)
{
    HRESULT             hr      = S_OK;
    CommandLineOptions  options;
    DiskCommandResult   result;
    AppleSingleFile     wrapped = single;



    outBytes.clear();

    if (appleSingle)
    {
        wrapped.data = std::move (payload.bytes);
        AppleSingleCodec::Encode (wrapped, outBytes);
        BAIL_OUT_IF (true, S_OK);
    }

    options.disk.encoding = encoding;

    hr = DiskCommandRunner::ApplyEncoding (options, payload, result);
    CHR (hr);

    outBytes = std::move (payload.bytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::MakePayload
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsAppleDrive::MakePayload (const std::wstring & hostPath, const ActionsTypes::PutPlan & plan, const std::vector<Byte> & bytes, VolumeKind kind, FilePayload & outPayload)
{
    HRESULT             hr = S_OK;
    CommandLineOptions  options;
    DiskCommandResult   result;



    if (plan.usePayload)
    {
        outPayload = plan.payload;
        BAIL_OUT_IF (true, S_OK);
    }

    options.disk.hostFile       = TextEncoding::WideToNarrow (hostPath);
    options.disk.path           = plan.catalogName;
    options.disk.typeName       = plan.typeName;
    options.disk.hasLoadAddress = plan.hasLoadAddress;
    options.disk.loadAddress    = plan.loadAddress;
    options.disk.encoding       = plan.encoding;

    hr = DiskCommandRunner::BuildPutPayload (options, kind, bytes, outPayload, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::CopyEntriesToAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::CopyEntriesToAppleDrive (const std::string & sourceImage, VolumeKind sourceKind, const std::vector<std::string> & catalogPaths)
{
    ActionsTypes::Outcome     outcome;
    std::string               link;
    std::string               directory;
    std::vector<std::string>  taken;
    int                       drive      = 0;
    VolumeKind                targetKind = m_actions.m_browser.GetVolumeKind();



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr = E_UNEXPECTED;
        return outcome;
    }

    for (const FileEntry & entry : m_actions.m_browser.GetListing().entries)
    {
        taken.push_back (entry.name);
    }

    for (const std::string & catalogPath : catalogPaths)
    {
        size_t       slash = catalogPath.find_last_of ('/');
        std::string  leaf  = (slash == std::string::npos) ? catalogPath : catalogPath.substr (slash + 1);
        std::string  name  = CassoExplorerActions::MakeFreeEntryName (leaf, taken, targetKind, false);
        std::string             path = directory.empty() ? name : directory + "/" + name;
        FilePayload             payload;
        DiskOperations::Result  result = m_actions.m_browser.GetOperations().Read (sourceImage, catalogPath, payload);
        Byte                    mapped = 0;

        if (!result.Succeeded())
        {
            CassoExplorerActions::Append (outcome, result);
            continue;
        }

        if (sourceKind == VolumeKind::Dos33 && targetKind == VolumeKind::ProDos)
        {
            payload.type = HostFileNaming::MapDos33ToProDosType (payload.type);
        }
        else if (sourceKind == VolumeKind::ProDos && targetKind == VolumeKind::Dos33)
        {
            payload.type = HostFileNaming::TryMapProDosToDos33 (payload.type, mapped) ? mapped : Dos33Volume::kTypeBinary;
        }

        taken.push_back (name);
        SubmitWrite (link, drive, [drive, path, payload] (CassoLinkSession & session) { return session.WriteFile (drive, FilePath::Parse (path), payload); }, outcome);
        outcome.created.push_back (path);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::GetFromAppleDrive
//
//  Each file is read over the link and written to the host folder when it
//  arrives, with the host name and conversion of a copy out of an image.
//  Directories are skipped, as they are from an image.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::GetFromAppleDrive (const std::wstring & hostFolder, HostFileNaming::Style style, const ActionsTypes::CollisionFn & collision)
{
    ActionsTypes::Outcome      outcome;
    std::vector<FileEntry>     entries;
    std::vector<std::wstring>  hostNames;
    std::string                link;
    std::string                directory;
    int                        drive     = 0;
    bool                       converted = false;
    VolumeKind                 kind      = m_actions.m_browser.GetVolumeKind();
    int                        index     = 0;
    int                        count     = 0;
    size_t                     i         = 0;



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr = E_UNEXPECTED;
        return outcome;
    }

    m_actions.m_browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        hostNames.push_back (entry.isDirectory ? std::wstring() : DragPayload::GetHostName (entry, kind, style, converted));
    }

    count = m_actions.m_copier.CountCollisions (hostFolder, hostNames);

    for (const FileEntry & entry : entries)
    {
        size_t           at                     = i++;
        std::wstring            hostPath    = entry.isDirectory ? std::wstring() : m_actions.m_copier.ResolveCopyOutPath (hostFolder, hostNames[at], collision, index, count);
        std::string             entryPath   = m_actions.m_browser.GetEntryPath (entry);
        ActionsTypes::Encoding  encoding    = CassoExplorerActions::GetEncoding (entry, kind);
        bool                    appleSingle = style == HostFileNaming::Style::AppleSingle;
        AppleSingleFile         single      = appleSingle ? DragPayload::MakeAppleSingle (entry, kind) : AppleSingleFile();
        HRESULT                 hr          = S_OK;

        if (entry.isDirectory || hostPath.empty())
        {
            outcome.skipped |= !entry.isDirectory;
            continue;
        }

        hr = m_actions.m_browser.GetLinks()->Submit (link,
            [drive, entryPath] (CassoLinkSession & session) { return session.ReadFile (drive, FilePath::Parse (entryPath)); },
            Guard ([this, hostPath, encoding, appleSingle, single] (const LinkOperation & operation) -> std::wstring
            {
                std::vector<Byte>  bytes;
                HRESULT            hrHost = operation.result;

                if (FAILED (hrHost))
                {
                    return CassoLinkConnections::DescribeFailure (operation);
                }

                hrHost = ToHostBytes (operation.file, encoding, appleSingle, single, bytes);

                if (SUCCEEDED (hrHost))
                {
                    hrHost = m_actions.m_fs.WriteAllText (hostPath, std::string (bytes.begin(), bytes.end()));
                }

                return FAILED (hrHost) ? CassoExplorerActions::GetLeafName (hostPath) + L" could not be written." : std::wstring();
            }));

        if (SUCCEEDED (hr))
        {
            outcome.written++;
        }
        else
        {
            ActionsCopy::AppendMessage (outcome, hr, L"CassoLink could not be started.");
        }
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::PutOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::PutOnAppleDrive (const std::vector<std::wstring> & hostPaths, const ActionsTypes::AddressFn & askAddress)
{
    ActionsTypes::Outcome     outcome;
    std::string               link;
    std::string               directory;
    std::vector<std::string>  taken;
    int                       drive     = 0;



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr = E_UNEXPECTED;
        return outcome;
    }

    for (const FileEntry & entry : m_actions.m_browser.GetListing().entries)
    {
        taken.push_back (entry.name);
    }

    PutItemsOnAppleDrive (link, drive, directory, taken, hostPaths, askAddress, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::PutItemsOnAppleDrive
//
//  Each file by the put rules, under a name nobody in the directory has; a
//  folder, on ProDOS, as a directory made first and then filled. The link
//  runs the writes in the order they are queued.
//
////////////////////////////////////////////////////////////////////////////////

void ActionsAppleDrive::PutItemsOnAppleDrive (const std::string & link, int drive, const std::string & directory, std::vector<std::string> taken,
                                              const std::vector<std::wstring> & hostPaths, const ActionsTypes::AddressFn & askAddress, ActionsTypes::Outcome & inOutOutcome)
{
    VolumeKind  kind = m_actions.m_browser.GetVolumeKind();



    for (const std::wstring & hostPath : hostPaths)
    {
        std::string                   content;
        std::vector<Byte>             bytes;
        std::vector<FileSystemEntry>  children;
        std::vector<std::wstring>     childPaths;
        ActionsTypes::PutPlan         plan;
        FilePayload                   payload;
        std::string                   name;
        std::string                   path;
        HRESULT                       hr         = S_OK;

        if (m_actions.m_copier.IsHostFolder (hostPath))
        {
            if (kind != VolumeKind::ProDos)
            {
                ActionsCopy::AppendMessage (inOutOutcome, E_FAIL, CassoExplorerActions::GetLeafName (hostPath) + L": a DOS 3.3 disk has no folders.");
                continue;
            }

            name = CassoExplorerActions::MakeFreeEntryName (CassoExplorerActions::SanitizeCatalogName (CassoExplorerActions::GetLeafName (hostPath), kind), taken, kind, false);
            path = directory.empty() ? name : directory + "/" + name;
            taken.push_back (name);

            SubmitWrite (link, drive, [drive, path] (CassoLinkSession & session) { return session.MakeDirectory (drive, FilePath::Parse (path)); }, inOutOutcome);

            hr = m_actions.m_fs.EnumerateAllEntries (hostPath, children);

            for (const FileSystemEntry & child : children)
            {
                childPaths.push_back (CassoExplorerBrowser::JoinPath (hostPath, child.name));
            }

            if (SUCCEEDED (hr))
            {
                PutItemsOnAppleDrive (link, drive, path, {}, childPaths, askAddress, inOutOutcome);
            }
            else
            {
                ActionsCopy::AppendMessage (inOutOutcome, hr, CassoExplorerActions::GetLeafName (hostPath) + L" could not be read.");
            }

            continue;
        }

        hr = m_actions.m_fs.ReadAllText (hostPath, content);

        if (FAILED (hr))
        {
            ActionsCopy::AppendMessage (inOutOutcome, hr, CassoExplorerActions::GetLeafName (hostPath) + L" could not be read.");
            continue;
        }

        bytes.assign (content.begin(), content.end());
        plan = CassoExplorerActions::PlanPut (hostPath, bytes, kind);

        if (!plan.refusal.empty())
        {
            ActionsCopy::AppendMessage (inOutOutcome, E_FAIL, CassoExplorerActions::GetLeafName (hostPath) + L": " + plan.refusal);
            continue;
        }

        if (plan.guessedAddress && askAddress && !askAddress (CassoExplorerActions::GetLeafName (hostPath), plan.loadAddress, plan.loadAddress))
        {
            inOutOutcome.skipped = true;
            continue;
        }

        plan.catalogName = CassoExplorerActions::MakeFreeEntryName (plan.catalogName, taken, kind, false);
        taken.push_back (plan.catalogName);
        path = directory.empty() ? plan.catalogName : directory + "/" + plan.catalogName;

        hr = MakePayload (hostPath, plan, bytes, kind, payload);

        if (FAILED (hr))
        {
            ActionsCopy::AppendMessage (inOutOutcome, hr, CassoExplorerActions::GetLeafName (hostPath) + L" could not be converted.");
            continue;
        }

        SubmitWrite (link, drive, [drive, path, payload] (CassoLinkSession & session) { return session.WriteFile (drive, FilePath::Parse (path), payload); }, inOutOutcome);
        inOutOutcome.created.push_back (path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::DeleteOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::DeleteOnAppleDrive (std::vector<FileEntry> entries)
{
    ActionsTypes::Outcome  outcome;
    DriveDeletion          plan;



    if (!TryPlanDeletion (std::move (entries), plan))
    {
        outcome.hr = E_UNEXPECTED;
        return outcome;
    }

    return RunDeletion (plan);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::TryPlanDeletion
//
//  None given is the selection. False when no Apple drive is on display.
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsAppleDrive::TryPlanDeletion (std::vector<FileEntry> entries, DriveDeletion & outPlan) const
{
    outPlan = DriveDeletion();

    if (!TryGetAppleDrive (outPlan.link, outPlan.drive, outPlan.directory))
    {
        return false;
    }

    if (entries.empty())
    {
        m_actions.m_browser.GetSelectedEntries (entries);
    }

    for (const FileEntry & entry : entries)
    {
        outPlan.items.push_back (DriveDeletion::Item { m_actions.m_browser.GetEntryPath (entry), entry.name, entry.isDirectory });
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::RunDeletion
//
//  Each entry is removed from the drive's listing once it is deleted, before
//  the drive is read again.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::RunDeletion (const DriveDeletion & plan)
{
    ActionsTypes::Outcome    outcome;
    CassoLinkConnections   * links     = m_actions.m_browser.GetLinks();
    std::string              link      = plan.link;
    int                      drive     = plan.drive;
    std::string              directory = plan.directory;



    for (const DriveDeletion::Item & item : plan.items)
    {
        FilePath     path     = FilePath::Parse (item.path);
        bool         isFolder = item.isFolder;
        std::string  name     = item.name;
        HRESULT      hr       = S_OK;

        hr = links->Submit (link, [drive, path, isFolder] (CassoLinkSession & session)
        {
            return isFolder ? session.RemoveDirectory (drive, path) : session.Delete (drive, path);
        },
        [links, link, drive, directory, name] (const LinkOperation & operation) -> std::wstring
        {
            if (SUCCEEDED (operation.result))
            {
                links->RemoveListed (link, drive, directory, name);
            }

            links->Invalidate (link, drive);

            return FAILED (operation.result) ? CassoLinkConnections::DescribeFailure (operation) : std::wstring();
        });

        if (SUCCEEDED (hr))
        {
            outcome.written++;
        }
        else
        {
            ActionsCopy::AppendMessage (outcome, hr, L"CassoLink could not be started.");
        }
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::RenameOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::RenameOnAppleDrive (const std::string & newName)
{
    ActionsTypes::Outcome   outcome;
    std::vector<FileEntry>  entries;
    std::string             link;
    std::string             directory;
    int                     drive     = 0;



    m_actions.m_browser.GetSelectedEntries (entries);

    if (!TryGetAppleDrive (link, drive, directory) || entries.size() != 1)
    {
        outcome.hr = E_INVALIDARG;
        return outcome;
    }

    {
        FilePath  path = FilePath::Parse (m_actions.m_browser.GetEntryPath (entries[0]));

        SubmitWrite (link, drive, [drive, path, newName] (CassoLinkSession & session) { return session.Rename (drive, path, newName); }, outcome);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::SetLockedOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::SetLockedOnAppleDrive (bool locked)
{
    ActionsTypes::Outcome   outcome;
    std::vector<FileEntry>  entries;
    std::string             link;
    std::string             directory;
    int                     drive     = 0;



    m_actions.m_browser.GetSelectedEntries (entries);

    if (!TryGetAppleDrive (link, drive, directory) || entries.empty())
    {
        outcome.hr = E_INVALIDARG;
        return outcome;
    }

    for (const FileEntry & entry : entries)
    {
        FilePath        path       = FilePath::Parse (m_actions.m_browser.GetEntryPath (entry));
        FileAttributes  attributes = CassoExplorerActions::MakeLockAttributes (entry, locked);

        SubmitWrite (link, drive, [drive, path, attributes] (CassoLinkSession & session) { return session.SetAttributes (drive, path, attributes); }, outcome);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::ApplyPropertiesOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::ApplyPropertiesOnAppleDrive (std::vector<FileEntry> entries, const AppleFileProperties::Edits & edits)
{
    ActionsTypes::Outcome   outcome;
    std::string             link;
    std::string             directory;
    int                     drive     = 0;
    VolumeKind              volume    = m_actions.m_browser.GetVolumeKind();



    if (!TryGetAppleDrive (link, drive, directory) || entries.empty())
    {
        outcome.hr = E_INVALIDARG;
        return outcome;
    }

    //  The name first, as a locked file can't be renamed; the link runs the
    //  writes in the order they are submitted.
    if (edits.nameEdited && entries.size() == 1)
    {
        FilePath     path = FilePath::Parse (m_actions.m_browser.GetEntryPath (entries[0]));
        std::string  name = TextEncoding::WideToNarrow (edits.name);

        SubmitWrite (link, drive, [drive, path, name] (CassoLinkSession & session) { return session.Rename (drive, path, name); }, outcome);

        entries[0].name = name;
    }

    for (const FileEntry & entry : entries)
    {
        FilePath        path       = FilePath::Parse (m_actions.m_browser.GetEntryPath (entry));
        FileAttributes  attributes = AppleFileProperties::MakeAttributes (entry, volume, edits);

        if (edits.ChangesAttributes())
        {
            SubmitWrite (link, drive, [drive, path, attributes] (CassoLinkSession & session) { return session.SetAttributes (drive, path, attributes); }, outcome);
        }
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::ConvertOnAppleDrive
//
//  The file is read over the link, and the converted file written once the
//  read is done; the new file's name is not one already in the list.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::ConvertOnAppleDrive (const FileEntry & entry, BasicConversion::Kind kind, std::string & outNewName)
{
    ActionsTypes::Outcome      outcome;
    std::string                link;
    std::string                directory;
    int                        drive     = 0;
    VolumeKind                 volume    = m_actions.m_browser.GetVolumeKind();
    std::vector<std::string>   taken;
    CassoLinkConnections     * links     = m_actions.m_browser.GetLinks();
    HRESULT                    hr        = S_OK;



    if (!TryGetAppleDrive (link, drive, directory) || links == nullptr)
    {
        outcome.hr = E_INVALIDARG;
        return outcome;
    }

    for (const CatalogRow & row : m_actions.m_browser.GetRows())
    {
        taken.push_back (TextEncoding::WideToNarrow (row.name));
    }

    outNewName = BasicConversion::MakeName (entry.name, volume, kind, taken);

    {
        FilePath     source = FilePath::Parse (m_actions.m_browser.GetEntryPath (entry));
        FilePath     target = FilePath::Parse (directory.empty() ? outNewName : directory + "/" + outNewName);
        std::string  name   = entry.name;

        hr = links->Submit (link, [drive, source] (CassoLinkSession & session) { return session.ReadFile (drive, source); },
                            Guard ([links, link, drive, target, volume, kind, name] (const LinkOperation & operation) -> std::wstring
        {
            FilePayload   converted;
            std::wstring  error;
            HRESULT       converts = S_OK;
            HRESULT       queued   = S_OK;

            if (FAILED (operation.result))
            {
                return CassoLinkConnections::DescribeFailure (operation);
            }

            converts = BasicConversion::Convert (operation.file, volume, kind, converted, error);

            if (FAILED (converts))
            {
                return std::format (L"{} wasn't converted. {}", TextEncoding::NarrowToWide (name), error);
            }

            queued = links->Submit (link, [drive, target, converted] (CassoLinkSession & session) { return session.WriteFile (drive, target, converted); },
                                    [links, link, drive] (const LinkOperation & written) -> std::wstring
            {
                links->Invalidate (link, drive);

                return FAILED (written.result) ? CassoLinkConnections::DescribeFailure (written) : std::wstring();
            });

            return FAILED (queued) ? std::wstring (L"CassoLink could not be started.") : std::wstring();
        }));
    }

    if (SUCCEEDED (hr))
    {
        outcome.written++;
    }
    else
    {
        ActionsCopy::AppendMessage (outcome, hr, L"CassoLink could not be started.");
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::CreateFolderOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::CreateFolderOnAppleDrive (const std::string & name)
{
    ActionsTypes::Outcome  outcome;
    std::string            link;
    std::string            directory;
    std::string            path;
    int                    drive     = 0;



    if (!TryGetAppleDrive (link, drive, directory) || m_actions.m_browser.GetVolumeKind() != VolumeKind::ProDos)
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Folders can only be made on a ProDOS disk.";
        return outcome;
    }

    path = directory.empty() ? name : directory + "/" + name;

    SubmitWrite (link, drive, [drive, path] (CassoLinkSession & session) { return session.MakeDirectory (drive, FilePath::Parse (path)); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::SubmitWrite
//
//  A write is reported when it is done; the drive's listings are read again
//  then, whether it succeeded or not.
//
////////////////////////////////////////////////////////////////////////////////

void ActionsAppleDrive::SubmitWrite (const std::string & link, int drive, CassoLinkConnections::Queuer queue, ActionsTypes::Outcome & inOutOutcome)
{
    CassoLinkConnections  * links = m_actions.m_browser.GetLinks();
    HRESULT                 hr    = S_OK;



    hr = links->Submit (link, queue, [links, link, drive] (const LinkOperation & operation) -> std::wstring
    {
        links->Invalidate (link, drive);

        return FAILED (operation.result) ? CassoLinkConnections::DescribeFailure (operation) : std::wstring();
    });

    if (SUCCEEDED (hr))
    {
        inOutOutcome.written++;
    }
    else
    {
        ActionsCopy::AppendMessage (inOutOutcome, hr, L"CassoLink could not be started.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::GetAppleSingle
//
//  The file's bytes as the image holds them, wrapped with its name, type,
//  aux type and date.
//
////////////////////////////////////////////////////////////////////////////////

void ActionsAppleDrive::GetAppleSingle (const std::string & image, const FileEntry & entry, const std::wstring & hostPath, ActionsTypes::Outcome & inOutOutcome)
{
    DiskOperations::Result  result = m_actions.m_browser.GetOperations().Get (image, m_actions.m_browser.GetEntryPath (entry), ActionsTypes::Encoding::Verbatim, "", entry.catalogIndex);
    AppleSingleFile         single;
    std::vector<Byte>       bytes;
    HRESULT                 hr     = S_OK;



    if (!result.Succeeded())
    {
        CassoExplorerActions::Append (inOutOutcome, result);
        return;
    }

    single      = DragPayload::MakeAppleSingle (entry, m_actions.m_browser.GetVolumeKind());
    single.data = std::move (result.payload);
    AppleSingleCodec::Encode (single, bytes);

    hr = m_actions.m_fs.WriteAllText (hostPath, std::string (bytes.begin(), bytes.end()));

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (inOutOutcome, hr, CassoExplorerActions::GetLeafName (hostPath) + L" could not be written.");
        return;
    }

    inOutOutcome.written++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::ReadSectors
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::ReadSectors (int track, int sector, int count, const std::wstring & hostPath, DiskOperations::Numbering numbering)
{
    ActionsTypes::Outcome  outcome;



    CassoExplorerActions::Append (outcome, m_actions.m_browser.GetOperations().SectorRead (TextEncoding::WideToNarrow (m_actions.GetFormatTarget()), numbering,
                                                          track, sector, count, TextEncoding::WideToNarrow (hostPath)));

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::WriteSectors
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::WriteSectors (int track, int sector, const std::wstring & hostPath, DiskOperations::Numbering numbering)
{
    ActionsTypes::Outcome  outcome;
    std::wstring           target  = m_actions.GetFormatTarget();



    CassoExplorerActions::Append (outcome, m_actions.m_browser.GetOperations().SectorWrite (TextEncoding::WideToNarrow (target), TextEncoding::WideToNarrow (hostPath),
                                                           numbering, track, sector));

    if (outcome.written > 0)
    {
        m_actions.FinishWrite (target);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::ReadBlocks (int block, int count, const std::wstring & hostPath)
{
    ActionsTypes::Outcome  outcome;



    CassoExplorerActions::Append (outcome, m_actions.m_browser.GetOperations().BlockRead (TextEncoding::WideToNarrow (m_actions.GetFormatTarget()), block, count,
                                                         TextEncoding::WideToNarrow (hostPath)));

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::WriteBlocks
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::WriteBlocks (int block, const std::wstring & hostPath)
{
    ActionsTypes::Outcome  outcome;
    std::wstring           target  = m_actions.GetFormatTarget();



    CassoExplorerActions::Append (outcome, m_actions.m_browser.GetOperations().BlockWrite (TextEncoding::WideToNarrow (target), TextEncoding::WideToNarrow (hostPath), block));

    if (outcome.written > 0)
    {
        m_actions.FinishWrite (target);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::ReadDiskToImage
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::ReadDiskToImage (const std::wstring & imagePath, bool blankUnreadable, ActionsTypes::UnreadableFn onUnreadable)
{
    ActionsTypes::Outcome  outcome;
    std::string            link;
    std::string            directory;
    int                    drive     = 0;



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Open a drive of a CassoLink connection first.";
        return outcome;
    }

    return ReadDiskToImage (link, drive, imagePath, blankUnreadable, std::move (onUnreadable));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::ReadDiskToImage
//
//  The offer to read again is given the link and drive this read went to.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::ReadDiskToImage (const std::string & link, int drive, const std::wstring & imagePath, bool blankUnreadable,
                                                                    ActionsTypes::UnreadableFn onUnreadable)
{
    ActionsTypes::Outcome    outcome;
    CassoLinkConnections   * links   = m_actions.m_browser.GetLinks();
    std::string              path    = TextEncoding::WideToNarrow (imagePath);
    HRESULT                  hr      = S_OK;



    hr = links->Submit (link, [drive, path, blankUnreadable] (CassoLinkSession & session) { return session.ReadDisk (drive, path, blankUnreadable); },
                        Guard ([onUnreadable, link, drive] (const LinkOperation & operation) -> std::wstring
    {
        bool  offer = operation.result == HRESULT_FROM_WIN32 (ERROR_CRC) && !operation.unreadable.empty() && !operation.blankUnreadable && onUnreadable;

        if (offer)
        {
            onUnreadable (operation, link, drive);
            return std::wstring();
        }

        return FAILED (operation.result) ? CassoLinkConnections::DescribeFailure (operation) : std::wstring();
    }));

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"CassoLink could not be started.");
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::StartFullSend
//
//  A session ends only by Escape, Cancel or a failure, so its result is always
//  shown: the counts and the reason.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::StartFullSend (const std::string & link, const std::wstring & folder, const std::wstring & extension)
{
    ActionsTypes::Outcome    outcome;
    CassoLinkConnections   * links = m_actions.m_browser.GetLinks();
    std::string              path  = TextEncoding::WideToNarrow (folder);
    std::string              type  = TextEncoding::WideToNarrow (extension);
    HRESULT                  hr    = S_OK;



    hr = links->Submit (link, [path, type] (CassoLinkSession & session) { return session.FullSend (path, type); },
                        Guard ([] (const LinkOperation & operation) -> std::wstring { return TextEncoding::NarrowToWide (operation.message); }));

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"CassoLink could not be started.");
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::WriteImageToDisk
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::WriteImageToDisk (const std::wstring & imagePath)
{
    ActionsTypes::Outcome  outcome;
    std::string            link;
    std::string            directory;
    int                    drive     = 0;



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Open a drive of a CassoLink connection first.";
        return outcome;
    }

    return WriteImageToAppleDrive (link, drive, imagePath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::WriteImageToAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::WriteImageToAppleDrive (const std::string & link, int drive, const std::wstring & imagePath)
{
    ActionsTypes::Outcome  outcome;
    std::string            path    = TextEncoding::WideToNarrow (imagePath);



    SubmitWrite (link, drive, [drive, path] (CassoLinkSession & session) { return session.WriteDisk (drive, path); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::CreateImageFromAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::CreateImageFromAppleDrive (const std::string & link, int drive, const std::wstring & hostFolder)
{
    ActionsTypes::Outcome    outcome;
    CassoLinkConnections   * links     = m_actions.m_browser.GetLinks();
    VolumeListing            listing;
    VolumeKind               kind      = VolumeKind::Unknown;
    HRESULT                  result    = S_OK;
    std::string              message;
    std::wstring             name;
    std::wstring             stem;
    std::wstring             extension;
    std::wstring             path;
    std::string              narrow;
    HRESULT                  hr        = S_OK;



    //  The volume's name, when the drive's catalog has been read.
    if (!links->TryGetListing (link, drive, std::string(), listing, kind, result, message) || FAILED (result))
    {
        listing = VolumeListing();
        kind    = VolumeKind::Unknown;
    }

    name      = MakeDriveImageName (listing, kind, 6, drive);
    stem      = name.substr (0, name.rfind (L'.'));
    extension = name.substr (name.rfind (L'.'));
    path      = hostFolder + L"\\" + name;

    for (int copy = 2; m_actions.m_browser.GetOperations().GetFileIo().Exists (TextEncoding::WideToNarrow (path)); copy++)
    {
        path = std::format (L"{}\\{} ({}){}", hostFolder, stem, copy, extension);
    }

    narrow = TextEncoding::WideToNarrow (path);

    hr = links->Submit (link, [drive, narrow] (CassoLinkSession & session) { return session.ReadDisk (drive, narrow, false); },
                        [] (const LinkOperation & operation) -> std::wstring
    {
        return FAILED (operation.result) ? CassoLinkConnections::DescribeFailure (operation) : std::wstring();
    });

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"CassoLink could not be started.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::Guard
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkConnections::Completion ActionsAppleDrive::Guard (CassoLinkConnections::Completion inner) const
{
    return [alive = std::weak_ptr<int> (m_alive), inner = std::move (inner)] (const LinkOperation & operation) -> std::wstring
    {
        return alive.expired() ? std::wstring() : inner (operation);
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::CheckLinkDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsAppleDrive::CheckLinkDisk (const std::string & link, int drive, ActionsTypes::LinkDiskFn then)
{
    return m_actions.m_browser.GetLinks()->Submit (link, [drive] (CassoLinkSession & session) { return session.CheckLinkDisk (drive); },
                                                   Guard ([then] (const LinkOperation & operation) -> std::wstring
    {
        then (operation.linkDisk, operation.result);
        return std::wstring();
    }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::MakeDriveImageName
//
//  Characters a file name cannot have are left out of a volume's name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsAppleDrive::MakeDriveImageName (const VolumeListing & listing, VolumeKind kind, int slot, int drive)
{
    std::wstring  name;



    if (kind == VolumeKind::ProDos && listing.hasVolumeName)
    {
        name = TextEncoding::NarrowToWide (listing.volumeName) + L".po";
    }
    else if (listing.hasVolumeNumber)
    {
        name = std::format (L"DOS 3.3 volume {}.dsk", listing.volumeNumber);
    }
    else
    {
        name = std::format (L"Slot {} Drive {}.dsk", slot, drive);
    }

    std::erase_if (name, [] (wchar_t ch) { return wcschr (L"\\/:*?\"<>|", ch) != nullptr; });

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::SaveLinkToDisk
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::SaveLinkToDisk (const std::string & link, int drive, const std::wstring & tempFolder)
{
    ActionsTypes::Outcome    outcome;
    CassoLinkConnections   * links     = m_actions.m_browser.GetLinks();
    CassoLinkSettings        chosen;
    std::vector<Byte>        disk;
    std::string              path;
    HRESULT                  hr        = S_OK;



    for (const CassoLinkSettings & settings : links->GetLinks())
    {
        if (settings.name == link)
        {
            chosen = settings;
        }
    }

    path = TextEncoding::WideToNarrow (std::format (L"{}\\casso-link-{}.dsk", tempFolder, drive));

    hr = LinkProgram::BuildUniversalBootDisk (CassoLinkConnections::MakeShimOptions (CassoLinkConnections::GetPcName(), chosen), disk);

    if (SUCCEEDED (hr))
    {
        hr = m_actions.m_browser.GetOperations().GetFileIo().WriteAllBytes (path, disk);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink disk could not be made.");
        return outcome;
    }

    SubmitWrite (link, drive, [drive, path] (CassoLinkSession & session) { return session.WriteDisk (drive, path); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::FormatAppleDrive
//
//  The disk is built as a 140 KB image of the format's own sector order and
//  formatted there, with the stock system files when it is to be bootable,
//  then written to the drive track by track. A disk with no file system has
//  nothing for the write to check its tracks against, so it is not offered.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::FormatAppleDrive (const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder)
{
    ActionsTypes::Outcome  outcome;
    std::string            link;
    std::string            directory;
    int                    drive     = 0;



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Open a drive of a CassoLink connection first.";
        return outcome;
    }

    return FormatAppleDrive (link, drive, request, tempFolder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::FormatAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::FormatAppleDrive (const std::string & link, int drive, const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder)
{
    ActionsTypes::Outcome           outcome;
    DiskOperations::NewDiskRequest  disk    = request;
    bool                            prodos   = request.formatName == "prodos";
    std::string                     path;
    std::string                     volume;
    VolumeListing                   listing;
    VolumeKind                      kind     = VolumeKind::Unknown;
    std::vector<Byte>               blank (NibblizationLayer::kImageByteSize, 0);
    HRESULT                         hr       = S_OK;



    if (request.formatName != "dos33" && !prodos)
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"A disk on the Apple can be formatted as DOS 3.3 or ProDOS.";
        return outcome;
    }

    disk.containerType = prodos ? "po" : "dsk";
    path               = TextEncoding::WideToNarrow (std::format (L"{}\\casso-format-{}.{}", tempFolder, drive, prodos ? L"po" : L"dsk"));

    hr = m_actions.m_browser.GetOperations().GetFileIo().WriteAllBytes (path, blank);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The new disk could not be made.");
        return outcome;
    }

    CassoExplorerActions::Append (outcome, m_actions.m_browser.GetOperations().Init (path, disk));

    if (!outcome.Succeeded())
    {
        return outcome;
    }

    outcome.written = 0;

    //  The new volume, as the Apple's label and history have it.
    CassoExplorerActions::Append (outcome, m_actions.m_browser.GetOperations().List (path, listing, kind));

    volume = listing.hasVolumeName   ? "/" + listing.volumeName
           : listing.hasVolumeNumber ? std::format ("VOL {}", listing.volumeNumber)
                                     : std::string (prodos ? "PRODOS" : "DOS 3.3");

    SubmitWrite (link, drive, [drive, path, volume] (CassoLinkSession & session) { return session.WriteDisk (drive, path, volume); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::SaveLinkDiskImage
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::SaveLinkDiskImage (const std::wstring & imagePath, const CassoLinkSettings & settings)
{
    ActionsTypes::Outcome  outcome;
    std::vector<Byte>      disk;
    HRESULT                hr      = LinkProgram::BuildUniversalBootDisk (CassoLinkConnections::MakeShimOptions (CassoLinkConnections::GetPcName(), settings), disk);



    if (SUCCEEDED (hr))
    {
        hr = m_actions.m_browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (imagePath), disk);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The CassoLink disk could not be saved.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::SaveCassetteBootstrap
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsAppleDrive::SaveCassetteBootstrap (const std::wstring & wavPath, const CassoLinkSettings & settings, std::string & outCommand)
{
    ActionsTypes::Outcome  outcome;
    std::vector<Byte>      wav;
    HRESULT                hr      = LinkAudioBootstrap::BuildWav (CassoLinkConnections::MakeShimOptions (CassoLinkConnections::GetPcName(), settings), wav, outCommand);



    if (SUCCEEDED (hr))
    {
        hr = m_actions.m_browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (wavPath), wav);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The cassette bootstrap could not be saved.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::DescribeAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsAppleDrive::DescribeAppleDrive() const
{
    std::wstring            link;
    int                     slot    = 0;
    int                     drive   = 0;
    const VolumeListing   & listing = m_actions.m_browser.GetListing();
    std::wstring            text;



    if (!TreeModel::TryParseLinkId (m_actions.m_browser.GetLocation().path, link, slot, drive))
    {
        return std::wstring();
    }

    text = TreeModel::GetLinkDriveLabel (slot, drive);

    if (listing.hasVolumeName)
    {
        text += std::format (L" (/{})", TextEncoding::NarrowToWide (listing.volumeName));
    }
    else if (listing.hasVolumeNumber)
    {
        text += std::format (L" (DOS 3.3 volume {})", listing.volumeNumber);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive::CancelLinkOperation
//
////////////////////////////////////////////////////////////////////////////////

void ActionsAppleDrive::CancelLinkOperation()
{
    std::string         link;
    std::string         directory;
    int                 drive   = 0;
    CassoLinkSession  * session = nullptr;



    if (TryGetAppleDrive (link, drive, directory))
    {
        session = m_actions.m_browser.GetLinks()->GetSession (link);
    }

    if (session != nullptr)
    {
        session->Cancel();
    }
}
