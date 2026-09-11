#include "Pch.h"

#include "Cassque/CassqueActions.h"
#include "Cassque/Model/ContentSniffer.h"
#include "Cassque/Model/DragPayload.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::CassqueActions
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::CassqueActions (CassqueBrowser & browser, IFileSystem & fs)
    : m_browser (browser),
      m_fs      (fs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::GetListVerbs
//
//  Files in an image can be copied out, deleted, renamed and booted; the
//  image itself always takes more files. Disk images in a host folder can be
//  opened and handed to Casso.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassqueActions::Verb> CassqueActions::GetListVerbs() const
{
    std::vector<Verb>       verbs;
    std::vector<FileEntry>  entries;
    size_t                  selected = m_browser.GetSelectedRows().size();



    if (m_browser.IsImageLocation())
    {
        m_browser.GetSelectedEntries (entries);

        if (!entries.empty())
        {
            verbs.push_back (Verb::Get);
            verbs.push_back (Verb::Delete);
        }

        if (entries.size() == 1)
        {
            verbs.push_back (Verb::Rename);

            if (!entries[0].isDirectory)
            {
                verbs.push_back (Verb::Boot);
            }
        }

        verbs.push_back (Verb::Put);
        verbs.push_back (Verb::Format);
    }
    else if (m_browser.AreSelectedRowsImages() && selected == 1)
    {
        verbs.push_back (Verb::Open);
        verbs.push_back (Verb::InsertDrive1);
        verbs.push_back (Verb::InsertDrive2);
        verbs.push_back (Verb::OpenInNewCasso);
        verbs.push_back (Verb::Format);
    }

    if (!m_browser.IsImageLocation() && m_browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        verbs.push_back (Verb::NewDisk);
    }

    verbs.push_back (Verb::Refresh);

    return verbs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::GetEncoding
//
//  BASIC to a listing and text to text; everything else verbatim.
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Encoding CassqueActions::GetEncoding (const FileEntry & entry, VolumeKind kind)
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
//  CassqueActions::GetLeafName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueActions::GetLeafName (const std::wstring & path)
{
    size_t  slash = path.find_last_of (L"\\/");



    return (slash == std::wstring::npos) ? path : path.substr (slash + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::MakeCatalogName
//
////////////////////////////////////////////////////////////////////////////////

std::string CassqueActions::MakeCatalogName (const std::wstring & hostName, VolumeKind kind)
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
//  CassqueActions::SanitizeCatalogName
//
////////////////////////////////////////////////////////////////////////////////

std::string CassqueActions::SanitizeCatalogName (const std::wstring & stem, VolumeKind kind)
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
//  CassqueActions::PlanPut
//
//  The suffix decides first, since it says what the file was; the content
//  decides only for a file that says nothing. A binary with no suffix has no
//  recorded address, so it takes the sniffer's suggestion and says so.
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::PutPlan CassqueActions::PlanPut (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind)
{
    PutPlan                  plan;
    ParsedHostName           parsed;
    bool                     isDos     = kind == VolumeKind::Dos33;
    ContentSniffer::Verdict  verdict   = ContentSniffer::Verdict::Binary;
    Word                     suggested = 0;
    Byte                     dosType   = 0;



    if (HostFileNaming::Parse (GetLeafName (hostName), parsed))
    {
        plan.catalogName = SanitizeCatalogName (TextEncoding::NarrowToWide (parsed.catalogName), kind);

        if (parsed.converted == ParsedHostName::ConvertedKind::ApplesoftListing)
        {
            plan.encoding = Encoding::Basic;
            return plan;
        }

        if (parsed.converted != ParsedHostName::ConvertedKind::None)
        {
            plan.encoding = Encoding::Text;
            return plan;
        }

        if (parsed.hasType)
        {
            plan.usePayload    = true;
            plan.payload.bytes = bytes;
            plan.payload.type  = parsed.type;

            if (isDos && !HostFileNaming::TryMapProDosToDos33 (parsed.type, dosType))
            {
                plan.refusal = L"DOS 3.3 has no file type matching this file's ProDOS type.";
                return plan;
            }

            if (isDos)
            {
                plan.payload.type = dosType;
            }

            if (parsed.hasAux)
            {
                plan.payload.hasAuxType     = !isDos;
                plan.payload.auxType        = parsed.aux;
                plan.payload.hasLoadAddress = plan.payload.type == (isDos ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary);
                plan.payload.loadAddress    = parsed.aux;
            }

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
//  CassqueActions::Append
//
////////////////////////////////////////////////////////////////////////////////

void CassqueActions::Append (Outcome & inOutOutcome, const DiskOperations::Result & result)
{
    if (!result.message.empty())
    {
        if (!inOutOutcome.message.empty())
        {
            inOutOutcome.message += L"\n";
        }

        inOutOutcome.message += TextEncoding::NarrowToWide (result.message);
    }

    if (!result.Succeeded())
    {
        inOutOutcome.hr = result.hr;
    }
    else
    {
        inOutOutcome.written++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::FinishWrite
//
////////////////////////////////////////////////////////////////////////////////

void CassqueActions::FinishWrite (const std::wstring & imagePath)
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
//  CassqueActions::GetSelected
//
//  Directories are skipped: a directory copies out through a drag, which
//  walks it.
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::GetSelected (const std::wstring & hostFolder, HostFileNaming::Style style)
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::string             image     = TextEncoding::WideToNarrow (m_browser.GetLocation().path);
    bool                    converted = false;



    m_browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        std::wstring  hostName = DragPayload::GetHostName (entry, m_browser.GetVolumeKind(), style, converted);
        std::wstring  hostPath = CassqueBrowser::JoinPath (hostFolder, hostName);

        if (entry.isDirectory)
        {
            continue;
        }

        Append (outcome, m_browser.GetOperations().Get (image, entry.name, GetEncoding (entry, m_browser.GetVolumeKind()),
                                                        TextEncoding::WideToNarrow (hostPath)));
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::PutFiles
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::PutFiles (const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress)
{
    Outcome        outcome;
    std::wstring   imagePath = m_browser.GetLocation().path;
    std::string    image     = TextEncoding::WideToNarrow (imagePath);
    VolumeKind     kind      = m_browser.GetVolumeKind();



    if (!m_browser.IsImageLocation())
    {
        outcome.hr      = E_UNEXPECTED;
        outcome.message = L"Open a disk image to put files into it.";
        return outcome;
    }

    for (const std::wstring & hostPath : hostPaths)
    {
        std::string        content;
        std::vector<Byte>  bytes;
        PutPlan            plan;
        HRESULT            hr = m_fs.ReadAllText (hostPath, content);

        if (FAILED (hr))
        {
            outcome.hr       = hr;
            outcome.message += (outcome.message.empty() ? L"" : L"\n") + GetLeafName (hostPath) + L" could not be read.";
            continue;
        }

        bytes.assign (content.begin(), content.end());
        plan = PlanPut (hostPath, bytes, kind);

        if (!plan.refusal.empty())
        {
            outcome.hr       = E_FAIL;
            outcome.message += (outcome.message.empty() ? L"" : L"\n") + GetLeafName (hostPath) + L": " + plan.refusal;
            continue;
        }

        if (plan.guessedAddress && askAddress && !askAddress (GetLeafName (hostPath), plan.loadAddress, plan.loadAddress))
        {
            continue;
        }

        if (plan.usePayload)
        {
            Append (outcome, m_browser.GetOperations().WritePayload (image, plan.catalogName, plan.payload));
        }
        else
        {
            Append (outcome, m_browser.GetOperations().Put (image, TextEncoding::WideToNarrow (hostPath), plan.catalogName,
                                                            plan.typeName, plan.hasLoadAddress, plan.loadAddress, plan.encoding));
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
//  CassqueActions::DeleteSelected
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::DeleteSelected()
{
    Outcome                 outcome;
    std::vector<FileEntry>  entries;
    std::wstring            imagePath = m_browser.GetLocation().path;



    m_browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        Append (outcome, m_browser.GetOperations().Delete (TextEncoding::WideToNarrow (imagePath), entry.name));
    }

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::BootSelected
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::BootSelected()
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

    Append (outcome, m_browser.GetOperations().Boot (TextEncoding::WideToNarrow (imagePath), entries[0].name));

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::RenameSelected
//
//  The name is taken as typed, uppercased, and the file system's rules are
//  left to the volume, which refuses what it cannot hold.
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::RenameSelected (const std::wstring & newName)
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

    Append (outcome, m_browser.GetOperations().Rename (TextEncoding::WideToNarrow (imagePath), entries[0].name, target));

    if (outcome.written > 0)
    {
        FinishWrite (imagePath);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::CreateImage
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::CreateImage (const std::wstring & folder, const std::wstring & fileName, const DiskOperations::NewDiskRequest & request)
{
    Outcome       outcome;
    std::wstring  path = CassqueBrowser::JoinPath (folder, fileName);
    HRESULT       hr   = S_OK;



    if (fileName.empty() || m_fs.Exists (path))
    {
        outcome.hr      = HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS);
        outcome.message = fileName.empty() ? L"The new disk needs a file name." : fileName + L" already exists.";
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
//  CassqueActions::GetFormatTarget
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueActions::GetFormatTarget() const
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
//  CassqueActions::FormatImage
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::FormatImage (const DiskOperations::NewDiskRequest & request)
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
//  CassqueActions::TryParseAddress
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueActions::TryParseAddress (const std::wstring & text, Word & outAddress)
{
    std::wstring  digits = text;
    int           base   = 16;
    wchar_t     * end    = nullptr;
    unsigned long value  = 0;



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

    value = wcstoul (digits.c_str(), &end, base);

    if (end == nullptr || *end != L'\0' || value > 0xFFFF)
    {
        return false;
    }

    outAddress = (Word) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::TryParseNumbers
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueActions::TryParseNumbers (const std::wstring & text, size_t required, std::vector<int> & outNumbers)
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
//  CassqueActions::ReadSectors
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::ReadSectors (int track, int sector, int count, const std::wstring & hostPath)
{
    Outcome  outcome;



    Append (outcome, m_browser.GetOperations().SectorRead (TextEncoding::WideToNarrow (GetFormatTarget()), DiskOperations::Numbering::Logical,
                                                          track, sector, count, TextEncoding::WideToNarrow (hostPath)));

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::WriteSectors
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::WriteSectors (int track, int sector, const std::wstring & hostPath)
{
    Outcome       outcome;
    std::wstring  target = GetFormatTarget();



    Append (outcome, m_browser.GetOperations().SectorWrite (TextEncoding::WideToNarrow (target), TextEncoding::WideToNarrow (hostPath),
                                                           DiskOperations::Numbering::Logical, track, sector));

    if (outcome.written > 0)
    {
        FinishWrite (target);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::ReadBlocks (int block, int count, const std::wstring & hostPath)
{
    Outcome  outcome;



    Append (outcome, m_browser.GetOperations().BlockRead (TextEncoding::WideToNarrow (GetFormatTarget()), block, count,
                                                         TextEncoding::WideToNarrow (hostPath)));

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions::WriteBlocks
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::Outcome CassqueActions::WriteBlocks (int block, const std::wstring & hostPath)
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
