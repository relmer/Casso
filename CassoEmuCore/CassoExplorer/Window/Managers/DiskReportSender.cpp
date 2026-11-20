#include "Pch.h"

#include "CassoExplorer/Window/Managers/DiskReportSender.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/CassoLinkSettings.h"
#include "CassoLink/FullSend/FullSendFolder.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Seams/IDiskPictures.h"
#include "Version.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::Report
//
////////////////////////////////////////////////////////////////////////////////

void DiskReportSender::Report (HWND owner, const IDxuiTheme * theme, const std::filesystem::path & image, const Sources & sources)
{
    static constexpr size_t  kMaxNote = 2000;
    DiskProblemFacts         facts;
    std::wstring             note;
    std::vector<Byte>        zip;
    std::filesystem::path    folder   = image.parent_path() / kReportsFolder;
    std::filesystem::path    zipPath;
    std::string              zipName;
    SYSTEMTIME               now      = {};
    HRESULT                  hr       = S_OK;
    std::error_code          ec;
    int                      choice   = IDCANCEL;



    if (!CassoExplorerPromptDialog::Ask (owner, theme, L"Report a problem", L"What went wrong with this disk?", L"", kMaxNote, note))
    {
        return;
    }

    Gather (image, sources, facts);
    facts.note = TextEncoding::WideToNarrow (note);

    GetLocalTime (&now);
    zipName = DiskProblemReport::MakeZipName (facts, std::format ("{:04}-{:02}-{:02} {:02}{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute));
    zipPath = folder / TextEncoding::NarrowToWide (zipName);

    hr = DiskProblemReport::MakeZip (facts, zip);

    if (SUCCEEDED (hr))
    {
        std::filesystem::create_directories (folder, ec);
        hr = WriteAll (zipPath, zip);
    }

    if (FAILED (hr))
    {
        DxuiMessageBox (owner, theme, std::format (L"The report couldn't be saved in {}.", folder.wstring()).c_str(), CassoExplorerShell::kAppName, MB_OK | MB_ICONERROR);
        return;
    }

    choice = DxuiMessageBox (owner, theme,
                             std::format (L"The report is saved as {}.\n\nFile it as a GitHub issue, and attach the zip there, or email it?", zipPath.filename().wstring()).c_str(),
                             CassoExplorerShell::kAppName, MB_YESNOCANCEL | MB_ICONINFORMATION,
                             { L"File on GitHub", L"Email", L"Close" });

    if (choice == IDYES)
    {
        ShellExecuteW (owner, L"open", DiskProblemReport::MakeIssueUrl (facts, zipName).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        ShowInFolder (zipPath);
    }
    else if (choice == IDNO)
    {
        hr = SendEmail (owner, zipPath, "Casso disk report: " + zipName, DiskProblemReport::MakeEmailBody (facts, zipName));

        if (FAILED (hr))
        {
            DxuiMessageBox (owner, theme, L"No mail program could draft the email. Attach the zip from the Reports folder to an email yourself.", CassoExplorerShell::kAppName, MB_OK | MB_ICONINFORMATION);
            ShowInFolder (zipPath);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::Gather
//
////////////////////////////////////////////////////////////////////////////////

void DiskReportSender::Gather (const std::filesystem::path & image, const Sources & sources, DiskProblemFacts & outFacts)
{
    std::vector<Byte>  log;
    std::string        from;
    std::string        to;



    outFacts              = DiskProblemFacts();
    outFacts.image        = TextEncoding::WideToNarrow (image.filename().wstring());
    outFacts.cassoVersion = TextEncoding::WideToNarrow (L"" VERSION_STRING L" (built " VERSION_BUILD_TIMESTAMP L")");
    outFacts.windowsVersion = GetWindowsVersion();

    ReadAll (image, outFacts.imageBytes);
    ReadAll (image.wstring() + TextEncoding::NarrowToWide (FullSendFolder::kReadsSuffix), outFacts.reads);

    GatherRow    (image, outFacts, from, to);
    GatherLink   (sources, outFacts);
    GatherScreen (image, sources, outFacts);

    if (!from.empty() && ReadAll (sources.logFile, log))
    {
        outFacts.captureLog = DiskProblemReport::PickLogRows (std::string (log.begin(), log.end()), from, to);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::GatherRow
//
//  The image's row in the manifest beside it, and the log's span for the
//  disk: from the row before it, which ended as this disk's read began, to
//  just after it, for its boot check.
//
////////////////////////////////////////////////////////////////////////////////

void DiskReportSender::GatherRow (const std::filesystem::path & image, DiskProblemFacts & ioFacts, std::string & outFrom, std::string & outTo)
{
    std::vector<Byte>    bytes;
    FullSendManifest     manifest;
    const FullSendRow  * previous = nullptr;
    HRESULT              hr       = S_OK;



    if (!ReadAll (image.parent_path() / FullSendManifest::kFileName, bytes))
    {
        return;
    }

    hr = manifest.Parse (bytes);

    if (FAILED (hr))
    {
        return;
    }

    for (const FullSendRow & row : manifest.GetRows())
    {
        if (_stricmp (row.file.c_str(), ioFacts.image.c_str()) == 0)
        {
            ioFacts.hasRow = true;
            ioFacts.row    = row;
            ioFacts.drive  = std::format ("Drive {}", row.drive);
            outFrom        = (previous != nullptr) ? ToLocalStamp (previous->time, 0) : ToLocalStamp (row.time, -kLogLeadMinutes);
            outTo          = ToLocalStamp (row.time, kLogTailMinutes);
            break;
        }

        previous = &row;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::GatherLink
//
//  The first connection with a session: its transport, the Apple it
//  reported and the program version Casso sends it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskReportSender::GatherLink (const Sources & sources, DiskProblemFacts & ioFacts)
{
    ioFacts.linkProgram = std::format ("version {}", LinkProgram::kProgramVersion);

    if (sources.links == nullptr)
    {
        return;
    }

    for (const CassoLinkSettings & settings : sources.links->GetLinks())
    {
        std::string          name    = CassoLinkSettings::MakeName (settings);
        CassoLinkSession   * session = sources.links->GetSession (name);
        std::wstring         model   = (session != nullptr) ? session->GetModelName() : std::wstring();

        if (model.empty())
        {
            continue;
        }

        ioFacts.appleModel = TextEncoding::WideToNarrow (model);
        ioFacts.transport  = std::format ("{}, {} on {} at {} baud", name, CassoLinkSettings::GetTransportName (settings.transport),
                                          TextEncoding::WideToNarrow (session->GetPort()), settings.baud);
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::GatherScreen
//
//  The image's thumbnail, the screen its boot check drew, and the line with
//  its boot result.
//
////////////////////////////////////////////////////////////////////////////////

void DiskReportSender::GatherScreen (const std::filesystem::path & image, const Sources & sources, DiskProblemFacts & ioFacts)
{
    std::shared_ptr<const DxuiIconImage>  picture;
    std::wstring                          result;
    HRESULT                               hr = S_OK;



    if (sources.pictures == nullptr || !sources.pictures->Find (image.wstring(), picture, result) || picture == nullptr)
    {
        return;
    }

    ioFacts.bootDetail = TextEncoding::WideToNarrow (result);
    hr = PicturePreview::EncodePng (picture->bgraPremul, picture->width, picture->height, ioFacts.bootScreen);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::GetWindowsVersion
//
//  The product, its release and its build, as "Windows 11 Pro 24H2 (26100)".
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskReportSender::GetWindowsVersion()
{
    static constexpr const wchar_t *  kKey       = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    wchar_t                           value[128] = {};
    std::wstring                      text;
    DWORD                             size       = sizeof (value);



    for (const wchar_t * name : { L"ProductName", L"DisplayVersion", L"CurrentBuild" })
    {
        size     = sizeof (value);
        value[0] = L'\0';

        if (RegGetValueW (HKEY_LOCAL_MACHINE, kKey, name, RRF_RT_REG_SZ, nullptr, value, &size) == ERROR_SUCCESS)
        {
            text += text.empty() ? value : (wcscmp (name, L"CurrentBuild") == 0 ? std::format (L" ({})", value) : std::format (L" {}", value));
        }
    }

    return TextEncoding::WideToNarrow (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::ToLocalStamp
//
//  A manifest's UTC time, as 2026-11-20T16:24:52Z, moved by offsetMinutes,
//  in local time as the CassoLink log has it: 2026-11-20 08:24:52. Empty
//  for a time it can't read.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskReportSender::ToLocalStamp (const std::string & isoUtc, int offsetMinutes)
{
    static constexpr LONGLONG  kTicksPerMinute = 600000000LL;
    SYSTEMTIME                 utc             = {};
    SYSTEMTIME                 local           = {};
    FILETIME                   file            = {};
    ULARGE_INTEGER             ticks           = {};
    int                        read            = 0;



    read = sscanf_s (isoUtc.c_str(), "%hu-%hu-%huT%hu:%hu:%hu", &utc.wYear, &utc.wMonth, &utc.wDay, &utc.wHour, &utc.wMinute, &utc.wSecond);

    if (read != 6 || !SystemTimeToFileTime (&utc, &file))
    {
        return std::string();
    }

    ticks.LowPart   = file.dwLowDateTime;
    ticks.HighPart  = file.dwHighDateTime;
    ticks.QuadPart += (ULONGLONG) ((LONGLONG) offsetMinutes * kTicksPerMinute);
    file.dwLowDateTime  = ticks.LowPart;
    file.dwHighDateTime = ticks.HighPart;

    if (!FileTimeToSystemTime (&file, &utc) || !SystemTimeToTzSpecificLocalTime (nullptr, &utc, &local))
    {
        return std::string();
    }

    return std::format ("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::ReadAll
//
////////////////////////////////////////////////////////////////////////////////

bool DiskReportSender::ReadAll (const std::filesystem::path & path, std::vector<Byte> & outBytes)
{
    std::ifstream  in (path, std::ios::binary);



    outBytes.clear();

    if (!in.good())
    {
        return false;
    }

    outBytes.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::WriteAll
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskReportSender::WriteAll (const std::filesystem::path & path, const std::vector<Byte> & bytes)
{
    std::ofstream  out (path, std::ios::binary | std::ios::trunc);



    out.write ((const char *) bytes.data(), (std::streamsize) bytes.size());

    return out.good() ? S_OK : HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::SendEmail
//
//  A draft in the default mail program, through Simple MAPI, with the zip
//  attached and no recipient, for the user to address.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskReportSender::SendEmail (HWND owner, const std::filesystem::path & zip, const std::string & subject, const std::string & body)
{
    HRESULT           hr       = S_OK;
    HMODULE           mapi     = LoadLibraryW (L"MAPI32.DLL");
    LPMAPISENDMAILW   send     = nullptr;
    std::wstring      path     = zip.wstring();
    std::wstring      name     = zip.filename().wstring();
    std::wstring      wSubject = TextEncoding::NarrowToWide (subject);
    std::wstring      wBody    = TextEncoding::NarrowToWide (body);
    MapiFileDescW     file     = {};
    MapiMessageW      message  = {};
    ULONG             result   = 0;



    CBREx (mapi != nullptr, HRESULT_FROM_WIN32 (ERROR_MOD_NOT_FOUND));

    send = (LPMAPISENDMAILW) GetProcAddress (mapi, "MAPISendMailW");
    CBREx (send != nullptr, HRESULT_FROM_WIN32 (ERROR_PROC_NOT_FOUND));

    file.nPosition    = (ULONG) -1;
    file.lpszPathName = path.data();
    file.lpszFileName = name.data();

    message.lpszSubject  = wSubject.data();
    message.lpszNoteText = wBody.data();
    message.nFileCount   = 1;
    message.lpFiles      = &file;

    result = send (0, (ULONG_PTR) owner, &message, MAPI_DIALOG | MAPI_LOGON_UI, 0);
    CBR (result == SUCCESS_SUCCESS || result == MAPI_USER_ABORT);

Error:
    if (mapi != nullptr)
    {
        FreeLibrary (mapi);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender::ShowInFolder
//
//  A File Explorer window on the zip's folder with the zip selected, to drag
//  onto the issue.
//
////////////////////////////////////////////////////////////////////////////////

void DiskReportSender::ShowInFolder (const std::filesystem::path & path)
{
    PIDLIST_ABSOLUTE  item = ILCreateFromPathW (path.wstring().c_str());
    HRESULT           hr   = S_OK;



    if (item != nullptr)
    {
        hr = SHOpenFolderAndSelectItems (item, 0, nullptr, 0);
        IGNORE_RETURN_VALUE (hr, S_OK);
        ILFree (item);
    }
}
