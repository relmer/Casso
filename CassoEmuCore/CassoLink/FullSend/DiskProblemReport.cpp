#include "Pch.h"

#include "CassoLink/FullSend/DiskProblemReport.h"
#include "CassoLink/FullSend/FullSendFolder.h"
#include "Core/TextEncoding.h"
#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::FormatReport
//
//  Report.txt: the note, then the disk, its capture and boot, then Casso
//  and the link, a line for each known fact.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskProblemReport::FormatReport (const DiskProblemFacts & facts)
{
    std::string  text;



    text += "Casso disk problem report\r\n\r\n";

    text += "What went wrong:\r\n";
    text += facts.note.empty() ? std::string ("(no note)") : facts.note;
    text += "\r\n\r\nDisk\r\n";
    AddLine (text, "Image",          facts.image);
    AddLine (text, "Image bytes",    facts.imageBytes.empty() ? std::string ("none captured") : std::to_string (facts.imageBytes.size()));
    AddLine (text, "Drive",          facts.drive);

    if (facts.hasRow)
    {
        AddLine (text, "Disk number",    std::to_string (facts.row.sequence));
        AddLine (text, "Captured",       facts.row.time);
        AddLine (text, "Status",         facts.row.status);
        AddLine (text, "Verification",   facts.row.verification);
        AddLine (text, "Unreadable",     std::to_string (facts.row.unreadable));
        AddLine (text, "Recovered",      std::to_string (facts.row.recovered));
        AddLine (text, "Unreliable",     std::to_string (facts.row.unreliable));
        AddLine (text, "Raw marks",      facts.row.marks);
        AddLine (text, "Write-protected", facts.row.writeProtected ? std::string ("yes") : std::string ("no"));
        AddLine (text, "File system",    facts.row.fsCheck);
        AddLine (text, "Boot",           facts.row.boot);
        AddLine (text, "Duplicate of",   facts.row.duplicateOf);
        AddLine (text, "Fingerprint",    facts.row.fingerprint);
        AddLine (text, "SHA-256",        facts.row.hash);
    }

    AddLine (text, "Boot check",     facts.bootDetail);

    text += "\r\nCasso\r\n";
    AddLine (text, "Version",        facts.cassoVersion);
    AddLine (text, "Windows",        facts.windowsVersion);

    text += "\r\nCassoLink\r\n";
    AddLine (text, "Transport",      facts.transport);
    AddLine (text, "Apple",          facts.appleModel);
    AddLine (text, "Apple ROM",      facts.appleRom);
    AddLine (text, "Apple memory",   facts.appleMemory);
    AddLine (text, "Apple drives",   facts.appleDrives);
    AddLine (text, "Apple program",  facts.linkProgram);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::MakeZip
//
//  Each file is added to an archive with no entries; one that is empty is
//  left out.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskProblemReport::MakeZip (const DiskProblemFacts & facts, std::vector<Byte> & outZip)
{
    HRESULT                                                    hr      = S_OK;
    std::string                                                report  = FormatReport (facts);
    std::vector<Byte>                                          next;
    std::vector<std::pair<std::string, std::span<const Byte>>> files;



    outZip.assign (22, 0);
    outZip[0] = 0x50;
    outZip[1] = 0x4B;
    outZip[2] = 0x05;
    outZip[3] = 0x06;

    files.push_back ({ kReportFile, std::span<const Byte> ((const Byte *) report.data(), report.size()) });
    files.push_back ({ facts.image, facts.imageBytes });
    files.push_back ({ facts.image + FullSendFolder::kReadsSuffix, facts.reads });
    files.push_back ({ kLogFile, std::span<const Byte> ((const Byte *) facts.captureLog.data(), facts.captureLog.size()) });
    files.push_back ({ kScreenFile, facts.bootScreen });

    for (const auto & [path, data] : files)
    {
        if (data.empty() || path.empty() || !ZipArchive::IsSafeEntryPath (path))
        {
            continue;
        }

        hr = ZipArchive::Replace (outZip, path, data, next);
        CHR (hr);

        outZip.swap (next);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::MakeZipName
//
//  The image's name without its extension, or "Disk" with no image, and the
//  stamp, as "Karate Champ report 2026-11-20 1405.zip".
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskProblemReport::MakeZipName (const DiskProblemFacts & facts, const std::string & stamp)
{
    std::string  stem = facts.image.empty() ? std::string ("Disk") : std::filesystem::path (facts.image).stem().string();



    return std::format ("{} report {}.zip", stem, stamp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::MakeIssueUrl
//
//  A new issue with the title and summary filled in. A summary too long for
//  a URL is cut, since the zip has all of it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskProblemReport::MakeIssueUrl (const DiskProblemFacts & facts, const std::string & zipName)
{
    std::string  body = MakeSummary (facts, zipName);
    std::string  url;



    if (body.size() > kMaxUrlBody)
    {
        body.resize (kMaxUrlBody);
        body += "\n...";
    }

    url = std::format ("?labels=disk&title={}&body={}", EncodeUrl (MakeTitle (facts)), EncodeUrl (body));

    return kNewIssueUrl + TextEncoding::NarrowToWide (url);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::MakeEmailBody
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskProblemReport::MakeEmailBody (const DiskProblemFacts & facts, const std::string & zipName)
{
    return MakeSummary (facts, zipName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::EncodeUrl
//
//  Percent-encoding of every byte but the unreserved characters of RFC 3986.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskProblemReport::EncodeUrl (const std::string & text)
{
    std::string  encoded;



    for (unsigned char ch : text)
    {
        if (isalnum (ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~')
        {
            encoded += (char) ch;
        }
        else
        {
            encoded += std::format ("%{:02X}", ch);
        }
    }

    return encoded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::AddLine
//
//  A line for a known fact, as "Status: raw-capture".
//
////////////////////////////////////////////////////////////////////////////////

void DiskProblemReport::AddLine (std::string & ioText, const char * label, const std::string & value)
{
    if (!value.empty())
    {
        ioText += std::format ("{}: {}\r\n", label, value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::MakeTitle
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskProblemReport::MakeTitle (const DiskProblemFacts & facts)
{
    std::string  what = facts.image.empty() ? std::string ("a disk that didn't capture") : std::filesystem::path (facts.image).stem().string();



    return kIssueTitle + what;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::MakeSummary
//
//  Report.txt and a line requesting the zip, for an issue or an email,
//  whose text can't include a file.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskProblemReport::MakeSummary (const DiskProblemFacts & facts, const std::string & zipName)
{
    std::string  text = FormatReport (facts);



    text += std::format ("\r\nPlease attach {}: it has the image, the capture log and the boot screen.\r\n", zipName);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport::PickLogRows
//
//  A row's time has milliseconds after the seconds; the seconds are
//  compared. A row that continues a quoted message onto another line goes
//  with the row it continues.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskProblemReport::PickLogRows (const std::string & log, const std::string & from, const std::string & to)
{
    static constexpr size_t  kStampChars = 19;
    std::string              picked;
    size_t                   at          = 0;
    bool                     isFirst     = true;
    bool                     isKept      = false;



    while (at < log.size())
    {
        size_t       end   = log.find ('\n', at);
        std::string  line  = log.substr (at, (end == std::string::npos) ? std::string::npos : end + 1 - at);
        bool         isRow = line.size() >= kStampChars && isdigit ((unsigned char) line[0]) && line[4] == '-';

        if (isFirst)
        {
            isKept = true;
        }
        else if (isRow)
        {
            std::string  stamp = line.substr (0, kStampChars);

            isKept = stamp >= from && stamp <= to;
        }

        if (isKept)
        {
            picked += line;
        }

        isFirst = false;
        at      = (end == std::string::npos) ? log.size() : end + 1;
    }

    return picked;
}
