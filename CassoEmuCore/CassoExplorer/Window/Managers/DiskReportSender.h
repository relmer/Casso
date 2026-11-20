#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/DiskProblemReport.h"

class CassoLinkConnections;
class IDiskPictures;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportSender
//
//  A problem report for a disk image: the facts about it are gathered, the
//  user types a note, and the zip is saved in a Reports folder beside the
//  image. Then a new GitHub issue opens in the browser, with the zip selected
//  in Casso Explorer to drag onto it, or the mail program drafts an email
//  with the zip attached.
//
////////////////////////////////////////////////////////////////////////////////

class DiskReportSender
{
public:
    struct Sources
    {
        CassoLinkConnections   * links    = nullptr;
        IDiskPictures          * pictures = nullptr;
        std::filesystem::path    logFile;

        //  Selects a file in the window's list, in its folder.
        std::function<void (const std::filesystem::path & file)>  select;
    };

    static constexpr const wchar_t *  kReportsFolder = L"Reports";
    static constexpr int              kLogLeadMinutes = 30;     // of log before a disk's first row, when it has no earlier row
    static constexpr int              kLogTailMinutes = 3;      // after its row, for the boot check

    static void  Report (HWND owner, const IDxuiTheme * theme, const std::filesystem::path & image, const Sources & sources);

private:
    static void         Gather            (const std::filesystem::path & image, const Sources & sources, DiskProblemFacts & outFacts);
    static void         GatherRow         (const std::filesystem::path & image, DiskProblemFacts & ioFacts, std::string & outFrom, std::string & outTo);
    static void         GatherLink        (const Sources & sources, DiskProblemFacts & ioFacts);
    static void         GatherScreen      (const std::filesystem::path & image, const Sources & sources, DiskProblemFacts & ioFacts);
    static std::string  GetWindowsVersion ();
    static std::string  ToLocalStamp      (const std::string & isoUtc, int offsetMinutes);
    static bool         ReadAll           (const std::filesystem::path & path, std::vector<Byte> & outBytes);
    static HRESULT      WriteAll          (const std::filesystem::path & path, const std::vector<Byte> & bytes);
    static HRESULT      SendEmail         (HWND owner, const std::filesystem::path & zip, const std::string & subject, const std::string & body);
};
