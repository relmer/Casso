#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/ContentSniffer.h"
#include "CassoExplorer/Model/DragPayload.h"
#include "Core/AppleSingleCodec.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





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

std::vector<CassoExplorerActions::Verb> CassoExplorerActions::GetListVerbs() const
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

CassoExplorerActions::Encoding CassoExplorerActions::GetEncoding (const FileEntry & entry, VolumeKind kind)
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

CassoExplorerActions::PutPlan CassoExplorerActions::PlanForced (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind, Conversion conversion)
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

CassoExplorerActions::DropKind CassoExplorerActions::DescribeDrop (const std::vector<std::wstring> & hostPaths, VolumeKind kind)
{
    DropKind  outcome;



    for (const std::wstring & hostPath : hostPaths)
    {
        std::string        content;
        std::vector<Byte>  bytes;
        PutPlan            plan;
        HRESULT            hr    = S_OK;
        std::wstring       label;

        if (IsHostFolder (hostPath))
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

CassoExplorerActions::PutPlan CassoExplorerActions::PlanAppleSingle (const std::wstring & hostName, const AppleSingleFile & file, VolumeKind kind)
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

CassoExplorerActions::PutPlan CassoExplorerActions::PlanPut (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind)
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

        inOutOutcome.message += CassoExplorerBrowser::ShortenPaths (TextEncoding::NarrowToWide (result.message));
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
        AppendWarning (inOutOutcome, CassoExplorerBrowser::ShortenPaths (TextEncoding::NarrowToWide (result.message)));
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

CassoExplorerActions::Outcome CassoExplorerActions::GetSelected (const std::wstring & hostFolder, HostFileNaming::Style style, const CollisionFn & collision)
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
        return GetFromAppleDrive (hostFolder, style, collision);
    }

    m_browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        hostNames.push_back (entry.isDirectory ? std::wstring() : DragPayload::GetHostName (entry, m_browser.GetVolumeKind(), style, converted));
    }

    count = CountCollisions (hostFolder, hostNames);

    for (const FileEntry & entry : entries)
    {
        size_t        at       = i++;
        std::wstring  hostPath;

        if (entry.isDirectory)
        {
            continue;
        }

        hostPath = ResolveCopyOutPath (hostFolder, hostNames[at], collision, index, count);

        if (hostPath.empty())
        {
            outcome.skipped = true;
            continue;
        }

        if (style == HostFileNaming::Style::AppleSingle)
        {
            GetAppleSingle (image, entry, hostPath, outcome);
            continue;
        }

        Append (outcome, m_browser.GetOperations().Get (image, m_browser.GetEntryPath (entry), GetEncoding (entry, m_browser.GetVolumeKind()),
                                                        TextEncoding::WideToNarrow (hostPath), entry.catalogIndex));
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::ExtractImage
//
//  Each file takes the host name and conversion of a copy out of the image,
//  in the host file style the Options dialog sets. A folder of that name that
//  is there already is left alone, and the files go into "name (2)".
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::ExtractImage (const std::wstring & imagePath, HostFileNaming::Style style, std::wstring & outFolder)
{
    Outcome                 outcome;
    std::filesystem::path   path   = imagePath;
    std::string             image  = TextEncoding::WideToNarrow (imagePath);
    VolumeListing           listing;
    VolumeKind              kind   = VolumeKind::Unknown;
    DiskOperations::Result  listed = m_browser.GetOperations().List (image, listing, kind);
    HRESULT                 hr     = S_OK;



    outFolder.clear();

    if (!listed.Succeeded())
    {
        Append (outcome, listed);
        return outcome;
    }

    outFolder = MakeFreeFolderPath (path.parent_path().wstring(), path.stem().wstring());
    hr        = m_fs.MakeDirectory (outFolder);

    if (FAILED (hr))
    {
        AppendMessage (outcome, hr, GetLeafName (outFolder) + L" could not be created.");
        return outcome;
    }

    ExtractDirectory (image, kind, std::string(), outFolder, style, 0, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::ExtractDirectory
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::ExtractDirectory (const std::string & image, VolumeKind kind, const std::string & directory, const std::wstring & hostFolder,
                                             HostFileNaming::Style style, int depth, Outcome & inOutOutcome)
{
    VolumeListing           listing;
    VolumeKind              listedKind = kind;
    DiskOperations::Result  listed     = m_browser.GetOperations().List (image, directory, listing, listedKind);



    if (!listed.Succeeded())
    {
        Append (inOutOutcome, listed);
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
            hr = m_fs.MakeDirectory (hostPath);

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
            Append (inOutOutcome, m_browser.GetOperations().Get (image, catalogPath, GetEncoding (entry, kind), TextEncoding::WideToNarrow (hostPath), entry.catalogIndex));
            continue;
        }

        result = m_browser.GetOperations().Get (image, catalogPath, Encoding::Verbatim, "", entry.catalogIndex);

        if (!result.Succeeded())
        {
            Append (inOutOutcome, result);
            continue;
        }

        single      = DragPayload::MakeAppleSingle (entry, kind);
        single.data = std::move (result.payload);
        AppleSingleCodec::Encode (single, bytes);

        hr = m_fs.WriteAllText (hostPath, std::string (bytes.begin(), bytes.end()));

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
//  CassoExplorerActions::MakeFreeFolderPath
//
//  `name` in `parent`, or "name (2)", "name (3)" and so on, the first that
//  nothing is called.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::MakeFreeFolderPath (const std::wstring & parent, const std::wstring & name)
{
    std::wstring  path = CassoExplorerBrowser::JoinPath (parent, name);



    for (int copy = 2; m_fs.Exists (path) || m_fs.DirectoryExists (path); copy++)
    {
        path = CassoExplorerBrowser::JoinPath (parent, std::format (L"{} ({})", name, copy));
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::MakeFreeFilePath
//
//  `name` in `folder`, or with " (2)", " (3)" and so on after its stem, the
//  first that nothing is called, so a copy out never replaces a file. The
//  number goes before the first period or hash, so the suffix with the
//  file's type parses.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::MakeFreeFilePath (const std::wstring & folder, const std::wstring & name)
{
    size_t        split = name.find_first_of (L".#");
    std::wstring  stem  = name.substr (0, split);
    std::wstring  rest  = (split == std::wstring::npos) ? std::wstring() : name.substr (split);
    std::wstring  path  = CassoExplorerBrowser::JoinPath (folder, name);



    for (int copy = 2; m_fs.Exists (path) || m_fs.DirectoryExists (path); copy++)
    {
        path = CassoExplorerBrowser::JoinPath (folder, std::format (L"{} ({}){}", stem, copy, rest));
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CountCollisions
//
//  How many of the names are files in the folder already; an empty name is
//  a directory, which is not copied.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerActions::CountCollisions (const std::wstring & hostFolder, const std::vector<std::wstring> & hostNames)
{
    int  count = 0;



    for (const std::wstring & name : hostNames)
    {
        bool  taken = !name.empty() && m_fs.Exists (CassoExplorerBrowser::JoinPath (hostFolder, name));

        count += taken ? 1 : 0;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::ResolveCopyOutPath
//
//  The name's path in the folder when nothing is there, and otherwise the
//  collision choice: that path to write over the file, a numbered one to
//  keep both, or none to skip the copy. With no choice to ask for, both are
//  kept. A file an earlier copy in this one wrote counts as well, so the
//  count grows to cover it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::ResolveCopyOutPath (const std::wstring & hostFolder, const std::wstring & hostName, const CollisionFn & collision,
                                                       int & inOutIndex, int count)
{
    std::wstring  path   = CassoExplorerBrowser::JoinPath (hostFolder, hostName);
    bool          taken  = m_fs.Exists (path);
    Collision     choice = Collision::KeepBoth;



    if (taken)
    {
        inOutIndex++;
        choice = collision ? collision (path, inOutIndex, std::max (count, inOutIndex)) : Collision::KeepBoth;
    }

    if (taken && choice == Collision::Skip)
    {
        path.clear();
    }
    else if (taken && choice == Collision::KeepBoth)
    {
        path = MakeFreeFilePath (hostFolder, hostName);
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::TryGetAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::TryGetAppleDrive (std::string & outLink, int & outDrive, std::string & outDirectory) const
{
    Location      location = m_browser.GetLocation();
    std::wstring  link;
    int           slot     = 0;
    bool          found    = false;



    found = location.kind == Location::Kind::AppleDrive && m_browser.GetLinks() != nullptr
         && TreeModel::TryParseLinkId (location.path, link, slot, outDrive) && outDrive != 0;

    outLink      = TextEncoding::WideToNarrow (link);
    outDirectory = location.innerPath;

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::ToHostBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerActions::ToHostBytes (FilePayload payload, Encoding encoding, bool appleSingle, const AppleSingleFile & single, std::vector<Byte> & outBytes)
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
//  CassoExplorerActions::MakePayload
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerActions::MakePayload (const std::wstring & hostPath, const PutPlan & plan, const std::vector<Byte> & bytes, VolumeKind kind, FilePayload & outPayload)
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
//  CassoExplorerActions::CopyEntriesToAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::CopyEntriesToAppleDrive (const std::string & sourceImage, VolumeKind sourceKind, const std::vector<std::string> & catalogPaths)
{
    Outcome                   outcome;
    std::string               link;
    std::string               directory;
    std::vector<std::string>  taken;
    int                       drive      = 0;
    VolumeKind                targetKind = m_browser.GetVolumeKind();



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr = E_UNEXPECTED;
        return outcome;
    }

    for (const FileEntry & entry : m_browser.GetListing().entries)
    {
        taken.push_back (entry.name);
    }

    for (const std::string & catalogPath : catalogPaths)
    {
        size_t                  slash   = catalogPath.find_last_of ('/');
        std::string             leaf    = (slash == std::string::npos) ? catalogPath : catalogPath.substr (slash + 1);
        std::string             name    = MakeFreeEntryName (leaf, taken, targetKind, false);
        std::string             path    = directory.empty() ? name : directory + "/" + name;
        FilePayload             payload;
        DiskOperations::Result  result  = m_browser.GetOperations().Read (sourceImage, catalogPath, payload);
        Byte                    mapped  = 0;

        if (!result.Succeeded())
        {
            Append (outcome, result);
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
//  CassoExplorerActions::GetFromAppleDrive
//
//  Each file is read over the link and written to the host folder when it
//  arrives, with the host name and conversion of a copy out of an image.
//  Directories are skipped, as they are from an image.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::GetFromAppleDrive (const std::wstring & hostFolder, HostFileNaming::Style style, const CollisionFn & collision)
{
    Outcome                    outcome;
    std::vector<FileEntry>     entries;
    std::vector<std::wstring>  hostNames;
    std::string                link;
    std::string                directory;
    int                        drive     = 0;
    bool                       converted = false;
    VolumeKind                 kind      = m_browser.GetVolumeKind();
    int                        index     = 0;
    int                        count     = 0;
    size_t                     i         = 0;



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr = E_UNEXPECTED;
        return outcome;
    }

    m_browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        hostNames.push_back (entry.isDirectory ? std::wstring() : DragPayload::GetHostName (entry, kind, style, converted));
    }

    count = CountCollisions (hostFolder, hostNames);

    for (const FileEntry & entry : entries)
    {
        size_t           at          = i++;
        std::wstring     hostPath    = entry.isDirectory ? std::wstring() : ResolveCopyOutPath (hostFolder, hostNames[at], collision, index, count);
        std::string      entryPath   = m_browser.GetEntryPath (entry);
        Encoding         encoding    = GetEncoding (entry, kind);
        bool             appleSingle = style == HostFileNaming::Style::AppleSingle;
        AppleSingleFile  single      = appleSingle ? DragPayload::MakeAppleSingle (entry, kind) : AppleSingleFile();
        HRESULT          hr          = S_OK;

        if (entry.isDirectory || hostPath.empty())
        {
            outcome.skipped |= !entry.isDirectory;
            continue;
        }

        hr = m_browser.GetLinks()->Submit (link,
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
                    hrHost = m_fs.WriteAllText (hostPath, std::string (bytes.begin(), bytes.end()));
                }

                return FAILED (hrHost) ? GetLeafName (hostPath) + L" could not be written." : std::wstring();
            }));

        if (SUCCEEDED (hr))
        {
            outcome.written++;
        }
        else
        {
            AppendMessage (outcome, hr, L"CassoLink could not be started.");
        }
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::PutOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::PutOnAppleDrive (const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress)
{
    Outcome                   outcome;
    std::string               link;
    std::string               directory;
    std::vector<std::string>  taken;
    int                       drive = 0;



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr = E_UNEXPECTED;
        return outcome;
    }

    for (const FileEntry & entry : m_browser.GetListing().entries)
    {
        taken.push_back (entry.name);
    }

    PutItemsOnAppleDrive (link, drive, directory, taken, hostPaths, askAddress, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::PutItemsOnAppleDrive
//
//  Each file by the put rules, under a name nobody in the directory has; a
//  folder, on ProDOS, as a directory made first and then filled. The link
//  runs the writes in the order they are queued.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::PutItemsOnAppleDrive (const std::string & link, int drive, const std::string & directory, std::vector<std::string> taken,
                                                 const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress, Outcome & inOutOutcome)
{
    VolumeKind  kind = m_browser.GetVolumeKind();



    for (const std::wstring & hostPath : hostPaths)
    {
        std::string                   content;
        std::vector<Byte>             bytes;
        std::vector<FileSystemEntry>  children;
        std::vector<std::wstring>     childPaths;
        PutPlan                       plan;
        FilePayload                   payload;
        std::string                   name;
        std::string                   path;
        HRESULT                       hr = S_OK;

        if (IsHostFolder (hostPath))
        {
            if (kind != VolumeKind::ProDos)
            {
                AppendMessage (inOutOutcome, E_FAIL, GetLeafName (hostPath) + L": a DOS 3.3 disk has no folders.");
                continue;
            }

            name = MakeFreeEntryName (SanitizeCatalogName (GetLeafName (hostPath), kind), taken, kind, false);
            path = directory.empty() ? name : directory + "/" + name;
            taken.push_back (name);

            SubmitWrite (link, drive, [drive, path] (CassoLinkSession & session) { return session.MakeDirectory (drive, FilePath::Parse (path)); }, inOutOutcome);

            hr = m_fs.EnumerateAllEntries (hostPath, children);

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
                AppendMessage (inOutOutcome, hr, GetLeafName (hostPath) + L" could not be read.");
            }

            continue;
        }

        hr = m_fs.ReadAllText (hostPath, content);

        if (FAILED (hr))
        {
            AppendMessage (inOutOutcome, hr, GetLeafName (hostPath) + L" could not be read.");
            continue;
        }

        bytes.assign (content.begin(), content.end());
        plan = PlanPut (hostPath, bytes, kind);

        if (!plan.refusal.empty())
        {
            AppendMessage (inOutOutcome, E_FAIL, GetLeafName (hostPath) + L": " + plan.refusal);
            continue;
        }

        if (plan.guessedAddress && askAddress && !askAddress (GetLeafName (hostPath), plan.loadAddress, plan.loadAddress))
        {
            inOutOutcome.skipped = true;
            continue;
        }

        plan.catalogName = MakeFreeEntryName (plan.catalogName, taken, kind, false);
        taken.push_back (plan.catalogName);
        path = directory.empty() ? plan.catalogName : directory + "/" + plan.catalogName;

        hr = MakePayload (hostPath, plan, bytes, kind, payload);

        if (FAILED (hr))
        {
            AppendMessage (inOutOutcome, hr, GetLeafName (hostPath) + L" could not be converted.");
            continue;
        }

        SubmitWrite (link, drive, [drive, path, payload] (CassoLinkSession & session) { return session.WriteFile (drive, FilePath::Parse (path), payload); }, inOutOutcome);
        inOutOutcome.created.push_back (path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::DeleteOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::DeleteOnAppleDrive()
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::string             link;
    std::string             directory;
    int                     drive = 0;



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr = E_UNEXPECTED;
        return outcome;
    }

    m_browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        FilePath  path      = FilePath::Parse (m_browser.GetEntryPath (entry));
        bool      directory = entry.isDirectory;

        SubmitWrite (link, drive, [drive, path, directory] (CassoLinkSession & session)
        {
            return directory ? session.RemoveDirectory (drive, path) : session.Delete (drive, path);
        }, outcome);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::RenameOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::RenameOnAppleDrive (const std::string & newName)
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::string             link;
    std::string             directory;
    int                     drive = 0;



    m_browser.GetSelectedEntries (entries);

    if (!TryGetAppleDrive (link, drive, directory) || entries.size() != 1)
    {
        outcome.hr = E_INVALIDARG;
        return outcome;
    }

    {
        FilePath  path = FilePath::Parse (m_browser.GetEntryPath (entries[0]));

        SubmitWrite (link, drive, [drive, path, newName] (CassoLinkSession & session) { return session.Rename (drive, path, newName); }, outcome);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CreateFolderOnAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::CreateFolderOnAppleDrive (const std::string & name)
{
    Outcome      outcome;
    std::string  link;
    std::string  directory;
    std::string  path;
    int          drive = 0;



    if (!TryGetAppleDrive (link, drive, directory) || m_browser.GetVolumeKind() != VolumeKind::ProDos)
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
//  CassoExplorerActions::SubmitWrite
//
//  A write is reported when it is done; the drive's listings are read again
//  then, whether it succeeded or not.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::SubmitWrite (const std::string & link, int drive, CassoLinkConnections::Queuer queue, Outcome & inOutOutcome)
{
    CassoLinkConnections  * links = m_browser.GetLinks();
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
        AppendMessage (inOutOutcome, hr, L"CassoLink could not be started.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::GetAppleSingle
//
//  The file's bytes as the image holds them, wrapped with its name, type,
//  aux type and date.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::GetAppleSingle (const std::string & image, const FileEntry & entry, const std::wstring & hostPath, Outcome & inOutOutcome)
{
    DiskOperations::Result  result = m_browser.GetOperations().Get (image, m_browser.GetEntryPath (entry), Encoding::Verbatim, "", entry.catalogIndex);
    AppleSingleFile         single;
    std::vector<Byte>       bytes;
    HRESULT                 hr     = S_OK;



    if (!result.Succeeded())
    {
        Append (inOutOutcome, result);
        return;
    }

    single      = DragPayload::MakeAppleSingle (entry, m_browser.GetVolumeKind());
    single.data = std::move (result.payload);
    AppleSingleCodec::Encode (single, bytes);

    hr = m_fs.WriteAllText (hostPath, std::string (bytes.begin(), bytes.end()));

    if (FAILED (hr))
    {
        AppendMessage (inOutOutcome, hr, GetLeafName (hostPath) + L" could not be written.");
        return;
    }

    inOutOutcome.written++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::PutFiles
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::PutFiles (const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress)
{
    Outcome  outcome;



    if (m_browser.IsAppleDriveLocation())
    {
        return PutOnAppleDrive (hostPaths, askAddress);
    }

    if (!m_browser.IsImageLocation())
    {
        outcome.hr      = E_UNEXPECTED;
        outcome.message = L"Open a disk image to put files into it.";
        return outcome;
    }

    return PutInto (m_browser.GetLocation().path, m_browser.GetVolumeKind(), m_browser.GetLocation().innerPath, hostPaths, askAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::PutInto
//
//  The image is reported written once, after everything has gone in.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::PutInto (
    const std::wstring               & imagePath,
    VolumeKind                         kind,
    const std::string                & directory,
    const std::vector<std::wstring>  & hostPaths,
    const AddressFn                  & askAddress,
    Conversion                         conversion)
{
    Outcome  outcome;



    PutItems (TextEncoding::WideToNarrow (imagePath), kind, directory, hostPaths, askAddress, conversion, outcome);
    KeepCreatedIn (outcome, directory);

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::AppendMessage
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::AppendMessage (Outcome & inOutOutcome, HRESULT hr, const std::wstring & message)
{
    inOutOutcome.hr       = hr;
    inOutOutcome.message += (inOutOutcome.message.empty() ? L"" : L"\n") + message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::AppendWarning
//
//  A warning about a copy that went ahead: the outcome stays a success,
//  marked damaged.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::AppendWarning (Outcome & inOutOutcome, const std::wstring & warning)
{
    inOutOutcome.damaged  = true;
    inOutOutcome.warning += (inOutOutcome.warning.empty() ? L"" : L"\n") + warning;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::PutItems
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::PutItems (
    const std::string                & image,
    VolumeKind                         kind,
    const std::string                & directory,
    const std::vector<std::wstring>  & hostPaths,
    const AddressFn                  & askAddress,
    Conversion                         conversion,
    Outcome                          & inOutOutcome)
{
    for (const std::wstring & hostPath : hostPaths)
    {
        std::string        content;
        std::vector<Byte>  bytes;
        PutPlan            plan;
        HRESULT            hr = S_OK;

        if (IsHostFolder (hostPath))
        {
            PutFolder (image, kind, directory, hostPath, askAddress, conversion, inOutOutcome);
            continue;
        }

        hr = m_fs.ReadAllText (hostPath, content);

        if (FAILED (hr))
        {
            AppendMessage (inOutOutcome, hr, GetLeafName (hostPath) + L" could not be read.");
            continue;
        }

        bytes.assign (content.begin(), content.end());
        plan = (conversion == Conversion::ByContent) ? PlanPut (hostPath, bytes, kind)
                                                     : PlanForced (hostPath, bytes, kind, conversion);

        if (!plan.refusal.empty())
        {
            AppendMessage (inOutOutcome, E_FAIL, GetLeafName (hostPath) + L": " + plan.refusal);
            continue;
        }

        if (plan.guessedAddress && askAddress && !askAddress (GetLeafName (hostPath), plan.loadAddress, plan.loadAddress))
        {
            inOutOutcome.skipped = true;
            continue;
        }

        //  Into the directory named, which is the volume's own when empty,
        //  under a name no entry there has.
        plan.catalogName = MakeFreeEntryName (plan.catalogName, GetEntryNames (image, directory), kind, false);
        plan.catalogName = directory.empty() ? plan.catalogName : directory + "/" + plan.catalogName;

        if (plan.usePayload)
        {
            Record (inOutOutcome, m_browser.GetOperations().WritePayload (image, plan.catalogName, plan.payload), plan.catalogName);
        }
        else
        {
            Record (inOutOutcome, m_browser.GetOperations().Put (image, TextEncoding::WideToNarrow (hostPath), plan.catalogName,
                                                                 plan.typeName, plan.hasLoadAddress, plan.loadAddress, plan.encoding),
                    plan.catalogName);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::Record
//
//  Appends the write's result and, when it made the entry, the entry.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::Record (Outcome & inOutOutcome, const DiskOperations::Result & result, const std::string & catalogPath)
{
    Append (inOutOutcome, result);

    if (result.Succeeded())
    {
        inOutOutcome.created.push_back (catalogPath);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::KeepCreatedIn
//
//  The entries made directly in the directory, without those made inside a
//  directory that was made too, which go with it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::KeepCreatedIn (Outcome & inOutOutcome, const std::string & directory)
{
    std::erase_if (inOutOutcome.created, [&directory] (const std::string & path)
    {
        size_t  slash = path.find_last_of ('/');

        return ((slash == std::string::npos) ? std::string() : path.substr (0, slash)) != directory;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::TryGetHostEntry
//
//  An item's entry in its folder's listing, which says whether it is a
//  folder and how large a file is.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::TryGetHostEntry (const std::wstring & path, FileSystemEntry & outEntry)
{
    std::vector<FileSystemEntry>  siblings;
    std::filesystem::path         item   (path);
    std::wstring                  leaf   = item.filename().wstring();
    HRESULT                       hr     = m_fs.EnumerateAllEntries (item.parent_path().wstring(), siblings);



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
//  CassoExplorerActions::CopyEntriesInto
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::CopyEntriesInto (
    const std::string                & sourceImage,
    VolumeKind                         sourceKind,
    const std::vector<std::string>   & catalogPaths,
    const std::wstring               & targetImage,
    VolumeKind                         targetKind,
    const std::string                & directory)
{
    Outcome      outcome;
    std::string  target = TextEncoding::WideToNarrow (targetImage);
    bool         same   = _stricmp (sourceImage.c_str(), target.c_str()) == 0;



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
        FinishWrite (targetImage);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::IsInsideFolder
//
//  Whether the catalog path `directory` is below `folder`, without regard
//  to case, the way both file systems compare entries.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::IsInsideFolder (const std::string & directory, const std::string & folder)
{
    return directory.size() > folder.size()
        && directory[folder.size()] == '/'
        && _strnicmp (directory.c_str(), folder.c_str(), folder.size()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CopyEntry
//
//  A path that lists is a directory; anything else is read as a file.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::CopyEntry (
    const std::string    & sourceImage,
    VolumeKind             sourceKind,
    const std::string    & catalogPath,
    const std::string    & targetImage,
    VolumeKind             targetKind,
    const std::string    & directory,
    int                    depth,
    Outcome              & inOutOutcome)
{
    size_t                  slash   = catalogPath.find_last_of ('/');
    std::string             parent  = (slash == std::string::npos) ? std::string() : catalogPath.substr (0, slash);
    //  A copy beside its original is named as Explorer's is; anything else
    //  that collides takes the next free number. Below the top, the entries
    //  go into a directory just made, where nothing collides.
    bool                    beside  = sourceImage == targetImage && _stricmp (parent.c_str(), directory.c_str()) == 0;
    std::string             leaf    = (slash == std::string::npos) ? catalogPath : catalogPath.substr (slash + 1);
    std::string             name    = (depth == 0) ? MakeFreeEntryName (leaf, GetEntryNames (targetImage, directory), targetKind, beside) : leaf;
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

    listedResult = (sourceKind == VolumeKind::ProDos) ? m_browser.GetOperations().List (sourceImage, catalogPath, listing, listed) : DiskOperations::Result { E_NOTIMPL };

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

        Record (inOutOutcome, m_browser.GetOperations().Mkdir (targetImage, target), target);

        for (const FileEntry & entry : listing.entries)
        {
            CopyEntry (sourceImage, sourceKind, catalogPath + "/" + entry.name, targetImage, targetKind, target, depth + 1, inOutOutcome);
        }

        return;
    }

    result = m_browser.GetOperations().Read (sourceImage, catalogPath, payload);

    if (!result.Succeeded())
    {
        Append (inOutOutcome, result);
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

    Record (inOutOutcome, m_browser.GetOperations().WritePayload (targetImage, target, payload), target);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::IsHostFolder
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::IsHostFolder (const std::wstring & path)
{
    FileSystemEntry  entry;
    bool             found = TryGetHostEntry (path, entry);



    return found && entry.isFolder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::PutFolder
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::PutFolder (
    const std::string    & image,
    VolumeKind             kind,
    const std::string    & directory,
    const std::wstring   & hostFolder,
    const AddressFn      & askAddress,
    Conversion             conversion,
    Outcome              & inOutOutcome)
{
    std::vector<FileSystemEntry>  entries;
    std::vector<std::wstring>     children;
    std::string                   name   = MakeFreeEntryName (MakeCatalogName (GetLeafName (hostFolder), kind), GetEntryNames (image, directory), kind, false);
    std::string                   target = directory.empty() ? name : directory + "/" + name;
    HRESULT                       hr     = S_OK;



    if (kind != VolumeKind::ProDos)
    {
        AppendMessage (inOutOutcome, E_FAIL, GetLeafName (hostFolder) + L": DOS 3.3 has no folders, so a folder cannot go on this disk.");
        return;
    }

    hr = m_fs.EnumerateEntries (hostFolder, entries);

    if (FAILED (hr))
    {
        AppendMessage (inOutOutcome, hr, GetLeafName (hostFolder) + L" could not be read.");
        return;
    }

    Record (inOutOutcome, m_browser.GetOperations().Mkdir (image, target), target);

    for (const FileSystemEntry & entry : entries)
    {
        children.push_back (CassoExplorerBrowser::JoinPath (hostFolder, entry.name));
    }

    PutItems (image, kind, target, children, askAddress, conversion, inOutOutcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CountUnitsForFile
//
//  ProDOS counts 512-byte blocks: one for a file up to a block, and an index
//  block for each 256 data blocks beyond that. DOS 3.3 counts 256-byte
//  sectors: the data, plus a track/sector list for each 122 of them.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CassoExplorerActions::CountUnitsForFile (VolumeKind kind, uint64_t sizeBytes)
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
//  CassoExplorerActions::CountUnitsForFolder
//
//  A ProDOS directory's key block. DOS 3.3 holds no folders.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CassoExplorerActions::CountUnitsForFolder (VolumeKind kind)
{
    return (kind == VolumeKind::ProDos) ? 1 : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::GetNewDiskFreeUnits
//
//  A 140K disk after its file system's own structures: ProDOS keeps its boot
//  blocks, volume directory and bitmap; DOS 3.3 keeps its three tracks and
//  the catalog track.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CassoExplorerActions::GetNewDiskFreeUnits (VolumeKind kind)
{
    return (kind == VolumeKind::ProDos) ? kProDosNewDiskFreeBlocks : kDos33NewDiskFreeSectors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CheckFitsNewDisk
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::CheckFitsNewDisk (VolumeKind kind, const std::vector<std::wstring> & hostPaths)
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
                return GetLeafName (path) + L" is a folder, and DOS 3.3 has no folders.";
            }

            needed += CountUnitsForFolder (kind);

            hr = m_fs.EnumerateEntries (path, entries);
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
    bool                       host = !m_browser.IsImageLocation();



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

CassoExplorerActions::Outcome CassoExplorerActions::DeleteSelected()
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::wstring            imagePath = m_browser.GetLocation().path;



    if (m_browser.IsAppleDriveLocation())
    {
        return DeleteOnAppleDrive();
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

CassoExplorerActions::Outcome CassoExplorerActions::DeleteEntries (const std::string & image, const std::vector<std::string> & catalogPaths)
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

CassoExplorerActions::Outcome CassoExplorerActions::CreateFolder (const std::wstring & name)
{
    Outcome       outcome;
    Location      location  = m_browser.GetLocation();
    std::wstring  imagePath = location.path;
    std::string   inner     = location.innerPath;
    std::string   leaf      = TextEncoding::WideToNarrow (name);



    if (!leaf.empty() && m_browser.IsAppleDriveLocation())
    {
        return CreateFolderOnAppleDrive (leaf);
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

CassoExplorerActions::Outcome CassoExplorerActions::BootSelected()
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

CassoExplorerActions::Outcome CassoExplorerActions::RenameSelected (const std::wstring & newName)
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
        return RenameOnAppleDrive (target);
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

CassoExplorerActions::Outcome CassoExplorerActions::CreateImage (const std::wstring & folder, const std::wstring & fileName, const DiskOperations::NewDiskRequest & request)
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

CassoExplorerActions::Outcome CassoExplorerActions::FormatImage (const DiskOperations::NewDiskRequest & request)
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





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::TryParseGoTo
//
//  A hyphen at the very start moves back from the caret, so only a hyphen
//  after the first address separates a range.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::TryParseGoTo (const std::wstring & text, int64_t caretAddress, int64_t & outFirst, int64_t & outLast)
{
    size_t        first  = text.find_first_not_of (L' ');
    size_t        split  = std::wstring::npos;
    size_t        start  = std::wstring::npos;
    std::wstring  rest;
    uint64_t      amount = 0;



    if (first == std::wstring::npos)
    {
        return false;
    }

    split = text.find (L',', first);
    split = (split != std::wstring::npos) ? split : text.find (L'-', first + 1);

    if (split == std::wstring::npos)
    {
        if (!TryParseGoToTarget (text, caretAddress, outFirst))
        {
            return false;
        }

        outLast = outFirst;
        return true;
    }

    if (!TryParseGoToTarget (text.substr (0, split), caretAddress, outFirst))
    {
        return false;
    }

    rest  = text.substr (split + 1);
    start = rest.find_first_not_of (L' ');

    if ((text[split] == L',') && (start != std::wstring::npos) && (rest[start] == L'+'))
    {
        if (!TryParseOffset (rest.substr (start + 1), amount) || amount == 0)
        {
            return false;
        }

        outLast = outFirst + (int64_t) amount - 1;
    }
    else
    {
        if (!TryParseOffset (rest, amount))
        {
            return false;
        }

        outLast = (int64_t) amount;
    }

    return outLast >= outFirst;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::TryParseGoToTarget
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::TryParseGoToTarget (const std::wstring & text, int64_t caretAddress, int64_t & outAddress)
{
    size_t   first  = text.find_first_not_of (L' ');
    wchar_t  sign   = 0;
    uint64_t amount = 0;



    if (first == std::wstring::npos)
    {
        return false;
    }

    if (text[first] == L'+' || text[first] == L'-')
    {
        sign = text[first];
        first++;
    }

    if (!TryParseOffset (text.substr (first), amount))
    {
        return false;
    }

    outAddress = (sign == L'+') ? (caretAddress + (int64_t) amount)
               : (sign == L'-') ? (caretAddress - (int64_t) amount)
                                : (int64_t) amount;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::TryParseSearch
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::TryParseSearch (const std::wstring & text, std::vector<Byte> & outBytes, bool & outIsText)
{
    size_t             first = text.find_first_not_of (L' ');
    size_t             last  = text.find_last_not_of (L' ');
    std::vector<Byte>  bytes;
    std::wstring       digits;



    if (first == std::wstring::npos)
    {
        return false;
    }

    //  Text in quotes: every character must be one an Apple II can store.
    if (text[first] == L'"')
    {
        if (last <= first || text[last] != L'"')
        {
            return false;
        }

        for (size_t i = first + 1; i < last; i++)
        {
            if (text[i] < 0x20 || text[i] > 0x7E)
            {
                return false;
            }

            bytes.push_back ((Byte) text[i]);
        }

        if (bytes.empty())
        {
            return false;
        }

        outBytes  = std::move (bytes);
        outIsText = true;
        return true;
    }

    for (size_t i = first; i <= last; i++)
    {
        if (text[i] != L' ')
        {
            digits.push_back (text[i]);
        }
    }

    //  Anything that is not an even run of hex digits is text, as typed.
    if ((digits.size() % 2) != 0 || digits.find_first_not_of (L"0123456789ABCDEFabcdef") != std::wstring::npos)
    {
        for (size_t i = first; i <= last; i++)
        {
            if (text[i] < 0x20 || text[i] > 0x7E)
            {
                return false;
            }

            bytes.push_back ((Byte) text[i]);
        }

        outBytes  = std::move (bytes);
        outIsText = true;
        return true;
    }

    for (size_t i = 0; i < digits.size(); i += 2)
    {
        bytes.push_back ((Byte) std::stoul (digits.substr (i, 2), nullptr, 16));
    }

    outBytes  = std::move (bytes);
    outIsText = false;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::FindBytes
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoExplorerActions::FindBytes (const std::vector<Byte> & haystack, const std::vector<Byte> & pattern, bool isText, size_t start)
{
    size_t  count = haystack.size();



    if (pattern.empty() || pattern.size() > count)
    {
        return kNotFound;
    }

    start = (start > count - pattern.size()) ? 0 : start;

    for (size_t n = 0; n <= count - pattern.size(); n++)
    {
        size_t  at    = (start + n) % (count - pattern.size() + 1);
        bool    match = true;

        for (size_t i = 0; i < pattern.size() && match; i++)
        {
            Byte  have = haystack[at + i];
            Byte  want = pattern[i];

            if (isText)
            {
                have = (Byte) towupper (have & 0x7F);
                want = (Byte) towupper (want & 0x7F);
            }

            match = (have == want);
        }

        if (match)
        {
            return at;
        }
    }

    return kNotFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::FindInSource
//
//  From `start` to the end, then from the beginning back to `start`.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CassoExplorerActions::FindInSource (const ReadFn & read, uint64_t count, const std::vector<Byte> & pattern, bool isText, uint64_t start)
{
    uint64_t              length = pattern.size();
    uint64_t              last   = 0;
    std::vector<uint8_t>  chunk;



    if (length == 0 || length > count)
    {
        return kNotFoundOffset;
    }

    last  = count - length;
    start = (start > last) ? 0 : start;

    for (int pass = 0; pass < 2; pass++)
    {
        uint64_t  from = (pass == 0) ? start : 0;
        uint64_t  to   = (pass == 0) ? last  : (std::min) (start, last + 1);

        //  `to` is the last offset a match may start at on the first pass,
        //  and one past it on the second.
        for (uint64_t at = from; (pass == 0) ? (at <= to) : (at < to); )
        {
            uint64_t  starts = (std::min) ((uint64_t) kSearchChunkBytes, ((pass == 0) ? (to + 1) : to) - at);
            uint64_t  span   = starts + length - 1;

            chunk.resize ((size_t) span);
            read (at, chunk);

            for (uint64_t i = 0; i < starts; i++)
            {
                bool  match = true;

                for (uint64_t j = 0; j < length && match; j++)
                {
                    Byte  have = chunk[(size_t) (i + j)];
                    Byte  want = pattern[(size_t) j];

                    if (isText)
                    {
                        have = (Byte) towupper (have & 0x7F);
                        want = (Byte) towupper (want & 0x7F);
                    }

                    match = (have == want);
                }

                if (match)
                {
                    return at + i;
                }
            }

            at += starts;
        }
    }

    return kNotFoundOffset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::TryParseAddress
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::TryParseAddress (const std::wstring & text, Word & outAddress)
{
    uint64_t  value = 0;



    if (!TryParseOffset (text, value) || value > 0xFFFF)
    {
        return false;
    }

    outAddress = (Word) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::TryParseOffset
//
//  TryParseAddress's forms, over the whole 64-bit range, for an offset into a
//  file larger than the Apple II's address space.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::TryParseOffset (const std::wstring & text, uint64_t & outValue)
{
    std::wstring        digits = text;
    int                 base   = 16;
    wchar_t           * end    = nullptr;
    unsigned long long  value  = 0;



    while (!digits.empty() && iswspace (digits.front()))
    {
        digits.erase (digits.begin());
    }

    while (!digits.empty() && iswspace (digits.back()))
    {
        digits.pop_back();
    }

    if (!digits.empty() && digits[0] == L'$')
    {
        digits.erase (0, 1);
    }
    else if (digits.size() > 2 && digits[0] == L'0' && (digits[1] == L'x' || digits[1] == L'X'))
    {
        digits.erase (0, 2);
    }
    else if (!digits.empty() && digits[0] == L'#')
    {
        digits.erase (0, 1);
        base = 10;
    }

    if (digits.empty() || !iswxdigit (digits[0]))
    {
        return false;
    }

    errno = 0;
    value = wcstoull (digits.c_str(), &end, base);

    if (end == nullptr || *end != L'\0' || errno == ERANGE)
    {
        return false;
    }

    outValue = (uint64_t) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::TryParseNumbers
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerActions::TryParseNumbers (const std::wstring & text, size_t required, std::vector<int> & outNumbers)
{
    static constexpr size_t  kMaxNumbers = 3;
    std::wstring             token;



    outNumbers.clear();

    for (size_t i = 0; i <= text.size(); i++)
    {
        wchar_t  c = (i < text.size()) ? text[i] : L' ';

        if (iswdigit (c))
        {
            token.push_back (c);
            continue;
        }

        if (c != L' ' && c != L',' && c != L'\t')
        {
            return false;
        }

        if (!token.empty())
        {
            outNumbers.push_back (_wtoi (token.c_str()));
            token.clear();
        }
    }

    return outNumbers.size() >= required && outNumbers.size() <= kMaxNumbers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::ReadSectors
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::ReadSectors (int track, int sector, int count, const std::wstring & hostPath, DiskOperations::Numbering numbering)
{
    Outcome  outcome;



    Append (outcome, m_browser.GetOperations().SectorRead (TextEncoding::WideToNarrow (GetFormatTarget()), numbering,
                                                          track, sector, count, TextEncoding::WideToNarrow (hostPath)));

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::WriteSectors
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::WriteSectors (int track, int sector, const std::wstring & hostPath, DiskOperations::Numbering numbering)
{
    Outcome       outcome;
    std::wstring  target = GetFormatTarget();



    Append (outcome, m_browser.GetOperations().SectorWrite (TextEncoding::WideToNarrow (target), TextEncoding::WideToNarrow (hostPath),
                                                           numbering, track, sector));

    if (outcome.written > 0)
    {
        FinishWrite (target);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::ReadBlocks (int block, int count, const std::wstring & hostPath)
{
    Outcome  outcome;



    Append (outcome, m_browser.GetOperations().BlockRead (TextEncoding::WideToNarrow (GetFormatTarget()), block, count,
                                                         TextEncoding::WideToNarrow (hostPath)));

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::WriteBlocks
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::WriteBlocks (int block, const std::wstring & hostPath)
{
    Outcome       outcome;
    std::wstring  target = GetFormatTarget();



    Append (outcome, m_browser.GetOperations().BlockWrite (TextEncoding::WideToNarrow (target), TextEncoding::WideToNarrow (hostPath), block));

    if (outcome.written > 0)
    {
        FinishWrite (target);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::ReadDiskToImage
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::ReadDiskToImage (const std::wstring & imagePath, bool blankUnreadable, UnreadableFn onUnreadable)
{
    Outcome      outcome;
    std::string  link;
    std::string  directory;
    int          drive = 0;



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
//  CassoExplorerActions::ReadDiskToImage
//
//  The offer to read again is given the link and drive this read went to.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::ReadDiskToImage (const std::string & link, int drive, const std::wstring & imagePath, bool blankUnreadable,
                                                                    UnreadableFn onUnreadable)
{
    Outcome                 outcome;
    CassoLinkConnections  * links = m_browser.GetLinks();
    std::string             path  = TextEncoding::WideToNarrow (imagePath);
    HRESULT                 hr    = S_OK;



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
        AppendMessage (outcome, hr, L"CassoLink could not be started.");
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::WriteImageToDisk
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::WriteImageToDisk (const std::wstring & imagePath)
{
    Outcome      outcome;
    std::string  link;
    std::string  directory;
    int          drive = 0;



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
//  CassoExplorerActions::WriteImageToAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::WriteImageToAppleDrive (const std::string & link, int drive, const std::wstring & imagePath)
{
    Outcome      outcome;
    std::string  path = TextEncoding::WideToNarrow (imagePath);



    SubmitWrite (link, drive, [drive, path] (CassoLinkSession & session) { return session.WriteDisk (drive, path); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CreateImageFromAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::CreateImageFromAppleDrive (const std::string & link, int drive, const std::wstring & hostFolder)
{
    Outcome                 outcome;
    CassoLinkConnections  * links   = m_browser.GetLinks();
    VolumeListing           listing;
    VolumeKind              kind    = VolumeKind::Unknown;
    HRESULT                 result  = S_OK;
    std::string             message;
    std::wstring            name;
    std::wstring            stem;
    std::wstring            extension;
    std::wstring            path;
    std::string             narrow;
    HRESULT                 hr      = S_OK;



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

    for (int copy = 2; m_browser.GetOperations().GetFileIo().Exists (TextEncoding::WideToNarrow (path)); copy++)
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
        AppendMessage (outcome, hr, L"CassoLink could not be started.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::Guard
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkConnections::Completion CassoExplorerActions::Guard (CassoLinkConnections::Completion inner) const
{
    return [alive = std::weak_ptr<int> (m_alive), inner = std::move (inner)] (const LinkOperation & operation) -> std::wstring
    {
        return alive.expired() ? std::wstring() : inner (operation);
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::CheckLinkDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerActions::CheckLinkDisk (const std::string & link, int drive, LinkDiskFn then)
{
    return m_browser.GetLinks()->Submit (link, [drive] (CassoLinkSession & session) { return session.CheckLinkDisk (drive); },
                                         Guard ([then] (const LinkOperation & operation) -> std::wstring
    {
        then (operation.linkDisk, operation.result);
        return std::wstring();
    }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::MakeDriveImageName
//
//  Characters a file name cannot have are left out of a volume's name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::MakeDriveImageName (const VolumeListing & listing, VolumeKind kind, int slot, int drive)
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
//  CassoExplorerActions::SaveLinkToDisk
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::SaveLinkToDisk (const std::wstring & tempFolder)
{
    Outcome                 outcome;
    CassoLinkConnections  * links = m_browser.GetLinks();
    std::string             link;
    std::string             directory;
    int                     drive = 0;
    CassoLinkSettings       chosen;
    std::vector<Byte>       disk;
    std::string             path;
    HRESULT                 hr    = S_OK;



    if (!TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Open a drive of a CassoLink connection first.";
        return outcome;
    }

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
        hr = m_browser.GetOperations().GetFileIo().WriteAllBytes (path, disk);
    }

    if (FAILED (hr))
    {
        AppendMessage (outcome, hr, L"The CassoLink disk could not be made.");
        return outcome;
    }

    SubmitWrite (link, drive, [drive, path] (CassoLinkSession & session) { return session.WriteDisk (drive, path); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::FormatAppleDrive
//
//  The disk is built as a 140 KB image of the format's own sector order and
//  formatted there, with the stock system files when it is to be bootable,
//  then written to the drive track by track. A disk with no file system has
//  nothing for the write to check its tracks against, so it is not offered.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::FormatAppleDrive (const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder)
{
    Outcome      outcome;
    std::string  link;
    std::string  directory;
    int          drive = 0;



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
//  CassoExplorerActions::FormatAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::FormatAppleDrive (const std::string & link, int drive, const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder)
{
    Outcome                         outcome;
    DiskOperations::NewDiskRequest  disk     = request;
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

    hr = m_browser.GetOperations().GetFileIo().WriteAllBytes (path, blank);

    if (FAILED (hr))
    {
        AppendMessage (outcome, hr, L"The new disk could not be made.");
        return outcome;
    }

    Append (outcome, m_browser.GetOperations().Init (path, disk));

    if (!outcome.Succeeded())
    {
        return outcome;
    }

    outcome.written = 0;

    //  The new volume, as the Apple's label and history have it.
    Append (outcome, m_browser.GetOperations().List (path, listing, kind));

    volume = listing.hasVolumeName   ? "/" + listing.volumeName
           : listing.hasVolumeNumber ? std::format ("VOL {}", listing.volumeNumber)
                                     : std::string (prodos ? "PRODOS" : "DOS 3.3");

    SubmitWrite (link, drive, [drive, path, volume] (CassoLinkSession & session) { return session.WriteDisk (drive, path, volume); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::SaveLinkDiskImage
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::SaveLinkDiskImage (const std::wstring & imagePath, const CassoLinkSettings & settings)
{
    Outcome            outcome;
    std::vector<Byte>  disk;
    HRESULT            hr = LinkProgram::BuildUniversalBootDisk (CassoLinkConnections::MakeShimOptions (CassoLinkConnections::GetPcName(), settings), disk);



    if (SUCCEEDED (hr))
    {
        hr = m_browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (imagePath), disk);
    }

    if (FAILED (hr))
    {
        AppendMessage (outcome, hr, L"The CassoLink disk could not be saved.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::SaveCassetteBootstrap
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::Outcome CassoExplorerActions::SaveCassetteBootstrap (const std::wstring & wavPath, const CassoLinkSettings & settings, std::string & outCommand)
{
    Outcome            outcome;
    std::vector<Byte>  wav;
    HRESULT            hr = LinkAudioBootstrap::BuildWav (CassoLinkConnections::MakeShimOptions (CassoLinkConnections::GetPcName(), settings), wav, outCommand);



    if (SUCCEEDED (hr))
    {
        hr = m_browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (wavPath), wav);
    }

    if (FAILED (hr))
    {
        AppendMessage (outcome, hr, L"The cassette bootstrap could not be saved.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions::DescribeAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerActions::DescribeAppleDrive() const
{
    std::wstring            link;
    int                     slot    = 0;
    int                     drive   = 0;
    const VolumeListing   & listing = m_browser.GetListing();
    std::wstring            text;



    if (!TreeModel::TryParseLinkId (m_browser.GetLocation().path, link, slot, drive))
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
//  CassoExplorerActions::CancelLinkOperation
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerActions::CancelLinkOperation()
{
    std::string         link;
    std::string         directory;
    int                 drive   = 0;
    CassoLinkSession  * session = nullptr;



    if (TryGetAppleDrive (link, drive, directory))
    {
        session = m_browser.GetLinks()->GetSession (link);
    }

    if (session != nullptr)
    {
        session->Cancel();
    }
}
