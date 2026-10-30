#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/BasicConversion.h"
#include "CassoExplorer/Model/ContentSniffer.h"
#include "CassoExplorer/Model/DragPayload.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "CassoExplorer/CassoExplorerBrowser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CassoExplorerActions
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::CassoExplorerActions (CassoExplorerBrowser & browser, IFileSystem & fs)
    : m_browser (browser),
      m_fs      (fs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::GetListVerbs
//
//  Files in an image can be copied out, deleted, renamed and booted; the
//  image itself always takes more files. Disk images in a host folder can be
//  opened and handed to Casso.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ActionsTypes::Verb> CassoExplorerActions::GetListVerbs() const
{
    std::vector<Verb>       verbs;
    std::vector<FileEntry>  entries;
    size_t                  selected = m_browser.GetSelectedRows().size();
    Location                location = m_browser.GetLocation();
    bool                    nested   = location.kind == Location::Kind::DiskDirectory && !location.innerPath.empty();



    if (m_browser.IsImageLocation())
    {
        bool  writable = !m_browser.IsWriteProtected();

        m_browser.GetSelectedEntries (entries);

        //  A file opens in the program Windows has for it, from a copy, as
        //  Explorer opens a file inside a zip; a folder opens here.
        if (std::any_of (entries.begin(), entries.end(), [] (const FileEntry & entry) { return !entry.isDirectory; }))
        {
            verbs.push_back (Verb::Open);
        }

        if (!entries.empty())
        {
            verbs.push_back (Verb::Get);

            //  The clipboard row's Copy, as for a real file: the entries go on
            //  as the files a paste would write.
            verbs.push_back (Verb::Copy);

            if (writable)
            {
                verbs.push_back (Verb::Delete);
            }
        }

        //  Rename takes the focused entry, so any selection offers it.
        if (!entries.empty() && writable)
        {
            verbs.push_back (Verb::Rename);
            AddFileVerbs (entries, verbs);
        }

        if (entries.size() == 1 && writable)
        {
            //  A boot file is found in the volume directory, so one in a
            //  subdirectory would never be run.
            if (!entries[0].isDirectory && !nested)
            {
                verbs.push_back (Verb::Boot);
            }
        }

        if (writable)
        {
            verbs.push_back (Verb::Put);
        }

        //  Formatting takes the whole disk, which is not what a subdirectory
        //  shows.
        if (!nested && writable)
        {
            verbs.push_back (Verb::Format);
        }
    }
    else if (m_browser.IsAppleDriveLocation())
    {
        //  A linked Apple's drive: files copy out, in and about as an image's
        //  do. A write to a write-protected disk fails with the reason.
        m_browser.GetSelectedEntries (entries);

        if (!entries.empty())
        {
            verbs.push_back (Verb::Get);
            verbs.push_back (Verb::Copy);
            verbs.push_back (Verb::Delete);
            verbs.push_back (Verb::Rename);
            AddFileVerbs (entries, verbs);
        }

        verbs.push_back (Verb::Put);
    }
    else if (m_browser.AreSelectedRowsImages() && selected == 1)
    {
        verbs.push_back (Verb::Open);
        verbs.push_back (Verb::InsertDrive1);
        verbs.push_back (Verb::InsertDrive2);
        verbs.push_back (Verb::OpenInNewCasso);
        verbs.push_back (Verb::ExtractFiles);
        verbs.push_back (Verb::Format);
    }

    //  A real file opens in its own program, and anything real has the shell's
    //  own menu. A folder or an image opens here instead.
    if (!m_browser.IsImageLocation() && location.kind == Location::Kind::HostFolder && selected > 0)
    {
        const CatalogRow &  row = m_browser.GetRows()[(size_t) m_browser.GetSelectedRows()[0]];

        if (selected == 1 && !row.isDirectory && !row.isDiskImage)
        {
            verbs.push_back (Verb::Open);
            verbs.push_back (Verb::OpenWith);
        }

        if (selected == 1 && row.isArchive)
        {
            verbs.push_back (Verb::ExtractAll);
        }
        else if (selected > 1)
        {
            //  Several open each on its own, as Explorer's Open does.
            verbs.push_back (Verb::Open);
        }

        verbs.push_back (Verb::Cut);
        verbs.push_back (Verb::Copy);

        //  Into the folder selected, as Explorer pastes into a folder row.
        if (selected == 1 && row.isDirectory)
        {
            verbs.push_back (Verb::Paste);
        }

        verbs.push_back (Verb::Delete);

        //  Rename takes the focused item, so any selection offers it.
        verbs.push_back (Verb::Rename);

        //  The share sheet takes files, not folders.
        if (!row.isDirectory)
        {
            verbs.push_back (Verb::Share);
        }

        verbs.push_back (Verb::MoreOptions);
    }

    //  What the shell lists, such as the files inside a zip, copies, cuts and
    //  deletes through the shell, and has the shell's own menu; a match inside
    //  a disk image is the image's to act on.
    if (location.kind == Location::Kind::ShellFolder && selected > 0 && !m_browser.AreSelectedRowsSearchMatches())
    {
        verbs.push_back (Verb::Cut);
        verbs.push_back (Verb::Copy);
        verbs.push_back (Verb::Delete);
        verbs.push_back (Verb::MoreOptions);
    }

    //  Into the folder shown, from its background.
    if (!m_browser.IsImageLocation() && location.kind == Location::Kind::HostFolder && selected == 0)
    {
        verbs.push_back (Verb::Paste);
    }

    //  New, from the background only, as in File Explorer: folders and disk
    //  images in a host folder, folders in a ProDOS image. A DOS 3.3 image has
    //  New disabled, which the window sets from the volume.
    if (!m_browser.IsImageLocation() && location.kind == Location::Kind::HostFolder && selected == 0)
    {
        verbs.push_back (Verb::NewFolder);
        verbs.push_back (Verb::NewDisk);
    }
    else if (m_browser.IsVolumeLocation() && selected == 0)
    {
        verbs.push_back (Verb::NewFolder);
    }

    //  The Recycle Bin's items go back or go for good, and the shell's menu
    //  has the rest; its background empties it.
    if (location.kind == Location::Kind::RecycleBin)
    {
        if (selected > 0)
        {
            verbs.push_back (Verb::Restore);
            verbs.push_back (Verb::Delete);
            verbs.push_back (Verb::MoreOptions);
        }
        else if (!m_browser.GetRows().empty())
        {
            verbs.push_back (Verb::EmptyRecycleBin);
        }
    }

    verbs.push_back (Verb::Refresh);

    return verbs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::GetEncoding
//
//  BASIC to a listing and text to text; everything else verbatim.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Encoding CassoExplorerActions::GetEncoding (const FileEntry & entry, VolumeKind kind)
{
    static constexpr Byte  kProDosInteger = 0xFA;
    bool                   isDos          = kind == VolumeKind::Dos33;



    if (isDos ? (entry.type == Dos33Volume::kTypeApplesoft || entry.type == Dos33Volume::kTypeInteger)
              : (entry.type == ProDosVolume::kTypeBasic || entry.type == kProDosInteger))
    {
        return Encoding::Basic;
    }

    if (entry.type == (isDos ? Dos33Volume::kTypeText : ProDosVolume::kTypeText))
    {
        return Encoding::Text;
    }

    return Encoding::Verbatim;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::GetLeafName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::GetLeafName (const std::wstring & path)
{
    size_t  slash = path.find_last_of (L"\\/");



    return (slash == std::wstring::npos) ? path : path.substr (slash + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::MakeCatalogName
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoExplorerActions::MakeCatalogName (const std::wstring & hostName, VolumeKind kind)
{
    std::wstring  stem = GetLeafName (hostName);
    size_t        dot  = stem.rfind (L'.');



    if (dot != std::wstring::npos && dot > 0)
    {
        stem = stem.substr (0, dot);
    }

    return SanitizeCatalogName (stem, kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::SanitizeCatalogName
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoExplorerActions::SanitizeCatalogName (const std::wstring & stem, VolumeKind kind)
{
    static constexpr size_t  kDosMaxName    = 30;
    static constexpr size_t  kProDosMaxName = 15;
    std::string              name;



    for (wchar_t ch : stem)
    {
        char  c = (ch >= 0x20 && ch < 0x7F) ? (char) ch : '.';

        if (c >= 'a' && c <= 'z')
        {
            c = (char) (c - 'a' + 'A');
        }

        if (kind == VolumeKind::ProDos && !((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.'))
        {
            c = '.';
        }

        if (kind == VolumeKind::Dos33 && c == ',')
        {
            c = '.';
        }

        name.push_back (c);
    }

    if (kind == VolumeKind::ProDos)
    {
        if (name.empty() || name[0] < 'A' || name[0] > 'Z')
        {
            name.insert (name.begin(), 'A');
        }

        name.resize ((std::min) (name.size(), kProDosMaxName));
    }
    else
    {
        if (name.empty())
        {
            name = "FILE";
        }

        name.resize ((std::min) (name.size(), kDosMaxName));
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::SetTypedPayload
//
//  A file whose type is known goes in as a payload of that type, mapped to
//  DOS 3.3's when the image is one; on a binary file the aux type is the
//  load address.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::SetTypedPayload (PutPlan & plan, const std::vector<Byte> & bytes, Byte type, bool hasAux, Word aux, bool isDos)
{
    Byte  dosType = 0;



    plan.usePayload    = true;
    plan.payload.bytes = bytes;
    plan.payload.type  = type;

    if (isDos && !HostFileNaming::TryMapProDosToDos33 (type, dosType))
    {
        plan.refusal = L"DOS 3.3 has no file type matching this file's ProDOS type.";
        return;
    }

    if (isDos)
    {
        plan.payload.type = dosType;
    }

    if (hasAux)
    {
        plan.payload.hasAuxType     = !isDos;
        plan.payload.auxType        = aux;
        plan.payload.hasLoadAddress = plan.payload.type == (isDos ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary);
        plan.payload.loadAddress    = aux;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::PlanForced
//
//  The conversion a right-drag's menu chose, in place of the rule the
//  content would follow. A binary still asks for its load address, since
//  nothing in the file records one.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::PutPlan CassoExplorerActions::PlanForced (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind, Conversion conversion)
{
    PutPlan                  plan;
    Word                     suggested = 0;
    ContentSniffer::Verdict  verdict   = ContentSniffer::Classify (std::span<const Byte> (bytes.data(), bytes.size()), suggested);



    IGNORE_RETURN_VALUE (verdict, ContentSniffer::Verdict::Binary);

    plan.catalogName = MakeCatalogName (hostName, kind);

    switch (conversion)
    {
        case Conversion::Text:
            plan.encoding = Encoding::Text;
            break;

        case Conversion::Applesoft:
            plan.encoding = Encoding::Basic;
            break;

        default:
            plan.encoding       = Encoding::Verbatim;
            plan.typeName       = "B";
            plan.hasLoadAddress = true;
            plan.loadAddress    = suggested;
            plan.guessedAddress = true;
            break;
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::DescribeDrop
//
//  A file whose name or container records its type says what it is, and a
//  name promising a conversion says which; only a file that records nothing
//  leaves the choice open. A drop of several is determined when every one of
//  them is.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::DropKind CassoExplorerActions::DescribeDrop (const std::vector<std::wstring> & hostPaths, VolumeKind kind)
{
    DropKind  outcome;



    for (const std::wstring & hostPath : hostPaths)
    {
        std::string        content;
        std::vector<Byte>  bytes;
        PutPlan            plan;
        HRESULT            hr    = S_OK;
        std::wstring       label;

        if (m_copier.IsHostFolder (hostPath))
        {
            label = L"the files it contains";
        }
        else
        {
            hr = m_fs.ReadAllText (hostPath, content);

            if (FAILED (hr))
            {
                return DropKind();
            }

            bytes.assign (content.begin(), content.end());
            plan  = PlanPut (hostPath, bytes, kind);
            label = DescribePlan (plan, kind);
        }

        if (label.empty())
        {
            return DropKind();
        }

        //  Several files agree only when they describe themselves the same
        //  way; otherwise the menu offers the conversions instead.
        if (outcome.determined && outcome.label != label)
        {
            return DropKind();
        }

        outcome.determined = true;
        outcome.label      = label;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::DescribePlan
//
//  What a plan says the file is, for a menu row: the type a payload holds
//  with its address, or the conversion a name promised. Empty when the
//  content rule did the deciding, which is a guess and not a description.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::DescribePlan (const PutPlan & plan, VolumeKind kind)
{
    wchar_t  address[16] = {};



    if (!plan.namedItself)
    {
        return std::wstring();
    }

    if (plan.usePayload)
    {
        swprintf_s (address, L" $%04X", (unsigned) plan.payload.auxType);

        return CatalogModel::GetTypeText (plan.payload.type, kind) + (plan.payload.hasLoadAddress ? address : L"");
    }

    if (plan.encoding == Encoding::Basic)
    {
        return L"Applesoft BASIC";
    }

    if (plan.encoding == Encoding::Text)
    {
        return L"text";
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::PlanAppleSingle
//
//  The data fork under the real name, with the ProDOS type and aux type the
//  container records and no dialog. A container without them holds a binary
//  file, at the address a binary put would suggest.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::PutPlan CassoExplorerActions::PlanAppleSingle (const std::wstring & hostName, const AppleSingleFile & file, VolumeKind kind)
{
    PutPlan       plan;
    std::wstring  name  = TextEncoding::NarrowToWide (file.realName);
    bool          isDos = kind == VolumeKind::Dos33;
    Word          load  = (file.data.size() == 8192) ? 0x2000 : 0x0803;



    if (name.empty())
    {
        name = std::filesystem::path (GetLeafName (hostName)).stem().wstring();
    }

    plan.catalogName = SanitizeCatalogName (name, kind);
    plan.namedItself = true;

    if (file.hasProDosInfo)
    {
        SetTypedPayload (plan, file.data, (Byte) file.fileType, true, (Word) file.auxType, isDos);
    }
    else
    {
        SetTypedPayload (plan, file.data, ProDosVolume::kTypeBinary, true, load, isDos);
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::PlanPut
//
//  The suffix decides first, since it says what the file was; the content
//  decides only for a file that says nothing. A binary with no suffix has no
//  recorded address, so it takes the sniffer's suggestion and says so.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::PutPlan CassoExplorerActions::PlanPut (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind)
{
    PutPlan                  plan;
    ParsedHostName           parsed;
    bool                     isDos     = kind == VolumeKind::Dos33;
    ContentSniffer::Verdict  verdict   = ContentSniffer::Verdict::Binary;
    Word                     suggested = 0;
    AppleSingleFile          single;
    std::string              error;
    HRESULT                  hr        = S_OK;



    //  An AppleSingle container says what it holds, so it goes in as that
    //  file whatever it is called.
    if (AppleSingleCodec::IsAppleSingle (std::span<const Byte> (bytes.data(), bytes.size())))
    {
        hr = AppleSingleCodec::Decode (std::span<const Byte> (bytes.data(), bytes.size()), single, error);

        if (SUCCEEDED (hr))
        {
            return PlanAppleSingle (hostName, single, kind);
        }
    }

    if (HostFileNaming::Parse (GetLeafName (hostName), parsed))
    {
        plan.catalogName = SanitizeCatalogName (TextEncoding::NarrowToWide (parsed.catalogName), kind);

        if (parsed.converted == ParsedHostName::ConvertedKind::ApplesoftListing)
        {
            plan.encoding    = Encoding::Basic;
            plan.namedItself = true;
            return plan;
        }

        if (parsed.converted != ParsedHostName::ConvertedKind::None)
        {
            plan.encoding    = Encoding::Text;
            plan.namedItself = true;
            return plan;
        }

        if (parsed.hasType)
        {
            SetTypedPayload (plan, bytes, parsed.type, parsed.hasAux, parsed.aux, isDos);
            plan.namedItself = true;
            return plan;
        }
    }
    else
    {
        plan.catalogName = MakeCatalogName (hostName, kind);
    }

    verdict = ContentSniffer::Classify (std::span<const Byte> (bytes.data(), bytes.size()), suggested);

    switch (verdict)
    {
        case ContentSniffer::Verdict::Applesoft:
            plan.encoding = Encoding::Basic;
            break;

        case ContentSniffer::Verdict::Text:
            plan.encoding = Encoding::Text;
            break;

        default:
            plan.encoding       = Encoding::Verbatim;
            plan.typeName       = "B";
            plan.hasLoadAddress = true;
            plan.loadAddress    = suggested;
            plan.guessedAddress = true;
            break;
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::Append
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::Append (Outcome & inOutOutcome, const DiskOperations::Result & result)
{
    if (!result.message.empty())
    {
        if (!inOutOutcome.message.empty())
        {
            inOutOutcome.message += L"\n";
        }

        inOutOutcome.message += BrowserCells::ShortenPaths (TextEncoding::NarrowToWide (result.message));
    }

    if (!result.Succeeded())
    {
        inOutOutcome.hr = result.hr;
    }
    else
    {
        inOutOutcome.written++;
    }

    //  The runner finished but complained, as a get from a disk with sectors
    //  it could not decode does: the complaint is shown as a warning.
    if (result.Succeeded() && result.exitStatus == DiskCommandResult::kWithComplaints && !result.message.empty())
    {
        ActionsCopy::AppendWarning (inOutOutcome, BrowserCells::ShortenPaths (TextEncoding::NarrowToWide (result.message)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::FinishWrite
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::FinishWrite (const std::wstring & imagePath)
{
    HRESULT  hr = S_OK;



    m_browser.GetBrowserModel().InvalidateCatalog (imagePath);
    hr = m_browser.Refresh();
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_onWritten)
    {
        m_onWritten (imagePath);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::GetSelected
//
//  Directories are skipped: a directory copies out through a drag, which
//  walks it. A file of the same name in the folder is replaced, skipped or
//  kept beside the copy, by the choice `collision` returns.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::GetSelected (const std::wstring & hostFolder, HostFileNaming::Style style, const CollisionFn & collision)
{
    Outcome                    outcome;
    std::vector<FileEntry>     entries;
    std::vector<std::wstring>  hostNames;
    std::string                image     = TextEncoding::WideToNarrow (m_browser.GetLocation().path);
    bool                       converted = false;
    int                        index     = 0;
    int                        count     = 0;
    size_t                     i         = 0;



    if (m_browser.IsAppleDriveLocation())
    {
        return m_appleDrive.GetFromAppleDrive (hostFolder, style, collision);
    }

    m_browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        hostNames.push_back (entry.isDirectory ? std::wstring() : DragPayload::GetHostName (entry, m_browser.GetVolumeKind(), style, converted));
    }

    count = m_copier.CountCollisions (hostFolder, hostNames);

    for (const FileEntry & entry : entries)
    {
        size_t        at       = i++;
        std::wstring  hostPath;

        if (entry.isDirectory)
        {
            continue;
        }

        hostPath = m_copier.ResolveCopyOutPath (hostFolder, hostNames[at], collision, index, count);

        if (hostPath.empty())
        {
            outcome.skipped = true;
            continue;
        }

        if (style == HostFileNaming::Style::AppleSingle)
        {
            m_appleDrive.GetAppleSingle (image, entry, hostPath, outcome);
            continue;
        }

        Append (outcome, m_browser.GetOperations().Get (image, m_browser.GetEntryPath (entry), GetEncoding (entry, m_browser.GetVolumeKind()),
                                                        TextEncoding::WideToNarrow (hostPath), entry.catalogIndex));
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::MakeUnusedName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::MakeUnusedName (const std::wstring & base, const std::vector<std::wstring> & taken, bool host)
{
    std::wstring  candidate = base;
    int           n         = 1;



    for (;;)
    {
        bool  inUse = false;

        for (const std::wstring & name : taken)
        {
            inUse = inUse || _wcsicmp (name.c_str(), candidate.c_str()) == 0;
        }

        if (!inUse)
        {
            return candidate;
        }

        n++;

        if (host)
        {
            candidate = std::format (L"{} ({})", base, n);
        }
        else
        {
            std::wstring  suffix = std::format (L".{}", n);

            candidate = base.substr (0, kProDosNameMax - suffix.size()) + suffix;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::MakeFreeEntryName
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoExplorerActions::MakeFreeEntryName (const std::string & desired, const std::vector<std::string> & taken, VolumeKind kind, bool copy)
{
    bool         proDos    = kind == VolumeKind::ProDos;
    size_t       maxLength = proDos ? kProDosNameMax : kDos33NameMax;
    std::string  base      = desired;
    std::string  candidate;
    int          n         = 1;



    auto  isTaken = [&taken] (const std::string & name)
    {
        return std::any_of (taken.begin(), taken.end(), [&name] (const std::string & one) { return _stricmp (one.c_str(), name.c_str()) == 0; });
    };

    if (!isTaken (desired))
    {
        return desired;
    }

    if (copy)
    {
        std::string  suffix = proDos ? ".COPY" : " - COPY";

        base = desired.substr (0, (std::min) (desired.size(), maxLength - suffix.size())) + suffix;
    }

    candidate = base;

    while (isTaken (candidate))
    {
        std::string  suffix = proDos ? std::format (".{}", ++n) : std::format (" ({})", ++n);

        candidate = base.substr (0, (std::min) (base.size(), maxLength - suffix.size())) + suffix;
    }

    return candidate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::GetEntryNames
//
//  The names already in an image's directory, which an entry going in must
//  not take; none when the directory cannot be listed.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CassoExplorerActions::GetEntryNames (const std::string & image, const std::string & directory) const
{
    std::vector<std::string>  names;
    VolumeListing             listing;
    VolumeKind                kind = VolumeKind::Unknown;



    if (m_browser.GetOperations().List (image, directory, listing, kind).Succeeded())
    {
        for (const FileEntry & entry : listing.entries)
        {
            names.push_back (entry.name);
        }
    }

    return names;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::GetNewFolderName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::GetNewFolderName() const
{
    std::vector<std::wstring>  taken;
    bool                       host = !m_browser.IsVolumeLocation();



    for (const CatalogRow & row : m_browser.GetRows())
    {
        taken.push_back (row.name);
    }

    return MakeUnusedName (host ? kHostFolderBase : kProDosFolderBase, taken, host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::DeleteSelected
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::DeleteSelected()
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::wstring            imagePath = m_browser.GetLocation().path;



    if (m_browser.IsAppleDriveLocation())
    {
        return m_appleDrive.DeleteOnAppleDrive();
    }

    m_browser.GetSelectedEntries (entries);

    //  Last in the catalog first: deleting an entry moves the ones after it
    //  up, so an earlier entry's index is good until it is deleted. A DOS 3.3
    //  disk can have several entries with one name and different indexes.
    std::ranges::stable_sort (entries, [] (const FileEntry & a, const FileEntry & b) { return a.catalogIndex > b.catalogIndex; });

    for (const FileEntry & entry : entries)
    {
        std::string  image = TextEncoding::WideToNarrow (imagePath);

        //  A directory goes with everything below it, through the plan the
        //  window has already shown and been answered on. A file goes by name.
        if (entry.isDirectory)
        {
            Append (outcome, m_browser.GetOperations().Rmdir (image, m_browser.GetEntryPath (entry), true));
        }
        else
        {
            Append (outcome, m_browser.GetOperations().Delete (image, m_browser.GetEntryPath (entry), entry.catalogIndex));
        }
    }

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::DeleteEntries
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::DeleteEntries (const std::string & image, const std::vector<std::string> & catalogPaths)
{
    Outcome        outcome;
    VolumeListing  listing;
    VolumeKind     kind = VolumeKind::Unknown;



    for (const std::string & path : catalogPaths)
    {
        //  Not forced: a move has no confirmation for a locked entry, so a locked
        //  one stays where it was, and the failure is reported.
        if (m_browser.GetOperations().List (image, path, listing, kind).Succeeded())
        {
            Append (outcome, m_browser.GetOperations().Rmdir (image, path, false));
        }
        else
        {
            Append (outcome, m_browser.GetOperations().Delete (image, path));
        }
    }

    if (outcome.written > 0)
    {
        FinishWrite (TextEncoding::NarrowToWide (image));
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::DescribeDeletePlan
//
//  A DIRECTORY'S EXTENT IS THE ONE THING THE WINDOW CANNOT SHOW BY ITSELF. The
//  list shows a row; what goes with it may be hundreds of entries in
//  directories below. So the lines a confirmation needs are built here, from
//  the same plan the removal applies.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerActions::DescribeDeletePlan() const
{
    std::vector<std::wstring>  lines;
    std::vector<FileEntry>     entries;
    std::wstring               imagePath = m_browser.GetLocation().path;
    uint32_t                   blocks    = 0;
    size_t                     rows      = 0;



    m_browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        DirectoryRemovalPlan    plan;
        DiskOperations::Result  built;

        if (!entry.isDirectory)
        {
            continue;
        }

        built = m_browser.GetOperations().BuildRemovalPlan (TextEncoding::WideToNarrow (imagePath),
                                                            m_browser.GetEntryPath (entry), plan);

        if (!built.Succeeded())
        {
            continue;
        }

        for (const DirectoryRemovalEntry & listed : plan.entries)
        {
            lines.push_back (TextEncoding::NarrowToWide (listed.path)
                             + (listed.isLocked ? L"   (locked)" : L""));
        }

        blocks += plan.blocksFreed;
        rows   += plan.entries.size();
    }

    if (!lines.empty())
    {
        lines.push_back (std::format (L"{} entries, {} blocks", rows, blocks));
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CreateFolder
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::CreateFolder (const std::wstring & name)
{
    Outcome       outcome;
    Location      location  = m_browser.GetLocation();
    std::wstring  imagePath = location.path;
    std::string   inner     = location.innerPath;
    std::string   leaf      = TextEncoding::WideToNarrow (name);



    if (!leaf.empty() && m_browser.IsAppleDriveLocation())
    {
        return m_appleDrive.CreateFolderOnAppleDrive (leaf);
    }

    if (leaf.empty() || !m_browser.IsImageLocation())
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Open a ProDOS disk image to make a folder on it.";

        return outcome;
    }

    Append (outcome, m_browser.GetOperations().Mkdir (TextEncoding::WideToNarrow (imagePath),
                                                      inner.empty() ? leaf : inner + "/" + leaf));

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::BootSelected
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::BootSelected()
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::wstring            imagePath = m_browser.GetLocation().path;



    m_browser.GetSelectedEntries (entries);

    if (entries.size() != 1)
    {
        outcome.hr = E_INVALIDARG;
        return outcome;
    }

    Append (outcome, m_browser.GetOperations().Boot (TextEncoding::WideToNarrow (imagePath), entries[0].name, entries[0].catalogIndex));

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::RenameSelected
//
//  The name is taken as typed, uppercased, and the file system's rules are
//  left to the volume, which refuses what it cannot hold.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::RenameSelected (const std::wstring & newName)
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::wstring            imagePath = m_browser.GetLocation().path;
    std::string             target    = TextEncoding::WideToNarrow (newName);



    m_browser.GetSelectedEntries (entries);

    if (entries.size() != 1 || target.empty())
    {
        outcome.hr = E_INVALIDARG;
        return outcome;
    }

    for (char & c : target)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = (char) (c - 'a' + 'A');
        }
    }

    if (m_browser.IsAppleDriveLocation())
    {
        return m_appleDrive.RenameOnAppleDrive (target);
    }

    Append (outcome, m_browser.GetOperations().Rename (TextEncoding::WideToNarrow (imagePath), m_browser.GetEntryPath (entries[0]), target,
                                                       entries[0].catalogIndex));

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::AddFileVerbs
//
//  Lock when an entry is unlocked, Unlock when one is locked, and Convert to
//  for one BASIC program or text file.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::AddFileVerbs (const std::vector<FileEntry> & entries, std::vector<Verb> & inOutVerbs) const
{
    if (std::any_of (entries.begin(), entries.end(), [] (const FileEntry & entry) { return !entry.isLocked; }))
    {
        inOutVerbs.push_back (Verb::Lock);
    }

    if (std::any_of (entries.begin(), entries.end(), [] (const FileEntry & entry) { return entry.isLocked; }))
    {
        inOutVerbs.push_back (Verb::Unlock);
    }

    if (entries.size() == 1 && !entries[0].isDirectory && BasicConversion::GetKind (m_browser.GetVolumeKind(), entries[0].type) != BasicConversion::Kind::None)
    {
        inOutVerbs.push_back (Verb::ConvertTo);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::MakeLockAttributes
//
////////////////////////////////////////////////////////////////////////////////

FileAttributes CassoExplorerActions::MakeLockAttributes (const FileEntry & entry, bool locked)
{
    static constexpr Byte  kLockBits = 0x80 | 0x40 | FilePayload::kAccessWriteEnable;   // destroy, rename and write
    FileAttributes         attributes;
    Byte                   access    = entry.hasAccess ? entry.access : (entry.isLocked ? FilePayload::kAccessLocked : FilePayload::kAccessUnlocked);



    attributes.access    = locked ? (Byte) (access & ~kLockBits) : (Byte) (access | FilePayload::kAccessUnlocked);
    attributes.hasAccess = true;

    return attributes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::SetLockedSelected
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::SetLockedSelected (bool locked)
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::wstring            imagePath = m_browser.GetLocation().path;
    std::string             image     = TextEncoding::WideToNarrow (imagePath);



    if (m_browser.IsAppleDriveLocation())
    {
        return m_appleDrive.SetLockedOnAppleDrive (locked);
    }

    m_browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        Append (outcome, m_browser.GetOperations().SetAttributes (image, m_browser.GetEntryPath (entry), MakeLockAttributes (entry, locked), entry.catalogIndex));
    }

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::ConvertSelected
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::ConvertSelected (std::string & outNewName)
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::wstring            imagePath = m_browser.GetLocation().path;
    std::string             image     = TextEncoding::WideToNarrow (imagePath);
    std::string             directory = m_browser.GetLocation().innerPath;
    VolumeKind              volume    = m_browser.GetVolumeKind();
    BasicConversion::Kind   kind      = BasicConversion::Kind::None;
    FilePayload             source;
    FilePayload             converted;
    std::wstring            error;
    HRESULT                 hr        = S_OK;



    outNewName.clear();
    m_browser.GetSelectedEntries (entries);

    kind = (entries.size() == 1) ? BasicConversion::GetKind (volume, entries[0].type) : BasicConversion::Kind::None;

    if (kind == BasicConversion::Kind::None)
    {
        outcome.hr = E_INVALIDARG;
        return outcome;
    }

    if (m_browser.IsAppleDriveLocation())
    {
        return m_appleDrive.ConvertOnAppleDrive (entries[0], kind, outNewName);
    }

    Append (outcome, m_browser.GetOperations().Read (image, m_browser.GetEntryPath (entries[0]), source, entries[0].catalogIndex));

    if (!outcome.Succeeded())
    {
        return outcome;
    }

    hr = BasicConversion::Convert (source, volume, kind, converted, error);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, std::format (L"{} wasn't converted. {}", TextEncoding::NarrowToWide (entries[0].name), error));
        return outcome;
    }

    outNewName = BasicConversion::MakeName (entries[0].name, volume, kind, GetEntryNames (image, directory));

    Append (outcome, m_browser.GetOperations().WritePayload (image, directory.empty() ? outNewName : directory + "/" + outNewName, converted));

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::IsNameTaken
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::IsNameTaken (const std::wstring & folder, const std::wstring & fileName) const
{
    return !folder.empty() && !fileName.empty() && m_fs.Exists (CassoExplorerBrowser::JoinPath (folder, fileName));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CreateImage
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::CreateImage (const std::wstring & folder, const std::wstring & fileName, const DiskOperations::NewDiskRequest & request)
{
    Outcome       outcome;
    std::wstring  path = CassoExplorerBrowser::JoinPath (folder, fileName);
    HRESULT       hr   = S_OK;



    if (fileName.empty() || m_fs.Exists (path))
    {
        outcome.hr      = HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS);
        outcome.message = fileName.empty() ? L"Enter a file name for the new disk." : fileName + L" already exists.";
        return outcome;
    }

    Append (outcome, m_browser.GetOperations().Create (TextEncoding::WideToNarrow (path), request));

    if (outcome.written > 0 && m_browser.GetLocation() == Location::MakeHostFolder (folder))
    {
        hr = m_browser.Reload (false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::GetFormatTarget
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::GetFormatTarget() const
{
    std::vector<std::wstring>  paths;



    if (m_browser.IsImageLocation())
    {
        return m_browser.GetLocation().path;
    }

    if (m_browser.AreSelectedRowsImages())
    {
        m_browser.GetSelectedHostPaths (paths);

        if (paths.size() == 1)
        {
            return paths[0];
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::FormatImage
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome CassoExplorerActions::FormatImage (const DiskOperations::NewDiskRequest & request)
{
    Outcome       outcome;
    std::wstring  target = GetFormatTarget();



    if (target.empty())
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Select one disk image to format.";
        return outcome;
    }

    Append (outcome, m_browser.GetOperations().Init (TextEncoding::WideToNarrow (target), request));

    if (outcome.written > 0)
    {
        FinishWrite (target);
    }

    return outcome;
}
