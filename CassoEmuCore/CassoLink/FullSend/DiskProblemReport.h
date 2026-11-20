#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemFacts
//
//  Everything known about a disk that didn't capture or boot as it should,
//  for a problem report. A field that isn't known is empty.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskProblemFacts
{
    std::string        image;              // the image's file name
    std::vector<Byte>  imageBytes;
    std::vector<Byte>  reads;              // a raw capture's .reads file
    bool               hasRow = false;
    FullSendRow        row;                // the image's row in the manifest
    std::string        captureLog;         // the CassoLink log's rows for the disk
    std::string        bootDetail;         // the boot check's verdict and machines
    std::vector<Byte>  bootScreen;         // the boot check's screen, as a PNG
    std::string        cassoVersion;
    std::string        windowsVersion;
    std::string        transport;          // the port and its speed
    std::string        appleModel;
    std::string        linkProgram;        // the Apple program's version
    std::string        drive;              // the drive the disk was read in
    std::string        note;               // what the user typed
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReport
//
//  A problem report for a disk: a zip with Report.txt, the image, its .reads,
//  the capture log and the boot screen, and a GitHub issue and an email that
//  have Report.txt's summary and a line requesting the zip.
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class DiskProblemReport
{
public:
    static constexpr const char *     kReportFile   = "Report.txt";
    static constexpr const char *     kLogFile      = "Capture log.csv";
    static constexpr const char *     kScreenFile   = "Boot screen.png";
    static constexpr const char *     kIssueTitle   = "[Disk]: ";
    static constexpr const wchar_t *  kNewIssueUrl  = L"https://github.com/relmer/Casso/issues/new";
    static constexpr size_t           kMaxUrlBody   = 6000;      // what a browser passes on in a URL, with room to spare

    static std::string   FormatReport  (const DiskProblemFacts & facts);
    static HRESULT       MakeZip       (const DiskProblemFacts & facts, std::vector<Byte> & outZip);
    static std::string   MakeZipName   (const DiskProblemFacts & facts, const std::string & stamp);
    static std::wstring  MakeIssueUrl  (const DiskProblemFacts & facts, const std::string & zipName);
    static std::string   MakeEmailBody (const DiskProblemFacts & facts, const std::string & zipName);
    static std::string   EncodeUrl     (const std::string & text);

    //  The log's header and its rows from `from` to `to`, each as
    //  "2026-11-20 08:24:52", by the time each row begins with.
    static std::string   PickLogRows   (const std::string & log, const std::string & from, const std::string & to);

private:
    static void         AddLine     (std::string & ioText, const char * label, const std::string & value);
    static std::string  MakeTitle   (const DiskProblemFacts & facts);
    static std::string  MakeSummary (const DiskProblemFacts & facts, const std::string & zipName);
};
