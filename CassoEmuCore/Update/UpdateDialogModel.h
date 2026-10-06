#pragma once

#include "Pch.h"

#include "Update/InstallTypeDetector.h"
#include "Update/ReleaseInfo.h"
#include "Update/ReleaseNotesExtractor.h"
#include "Update/ReleaseNotesFormatter.h"
#include "Update/UpdateFailure.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateButtonSet
//
//  The actions the update dialog offers. Skip this version is always there,
//  bottom-left; the set decides the primary button, bottom-right:
//
//    UpdateNow    Update to <version>: an official copy with a download
//                 for it
//    ReleasePage  Open release page: no download for this copy, or a
//                 folder Casso cannot write to
//    Developer    Update to <version>, disabled, with the pull-and-rebuild
//                 text and a release page link
//
////////////////////////////////////////////////////////////////////////////////

enum class UpdateButtonSet
{
    UpdateNow,
    ReleasePage,
    Developer,
};





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel
//
//  The update dialog's decisions and text, as data in and data out so the
//  tests reach every branch without building a window.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateDialogModel
{
public:
    static constexpr LPCWSTR  kpszTitle          = L"Casso update";
    static constexpr LPCWSTR  kpszSkip           = L"Skip this version";
    static constexpr LPCWSTR  kpszOpenPage       = L"Open release page";
    static constexpr LPCWSTR  kpszCancel         = L"Cancel";
    static constexpr LPCWSTR  kpszPageLink       = L"View this release on GitHub";
    static constexpr LPCWSTR  kpszDeveloperText  = L"This is a developer build. Pull and rebuild to update.";
    static constexpr LPCWSTR  kpszNotesLoading   = L"Loading the release notes...";
    static constexpr LPCWSTR  kpszNotesMissing   = L"The release notes could not be loaded. They are on the release page.";
    static constexpr LPCWSTR  kpszInstalling     = L"Installing the update...";
    static constexpr LPCWSTR  kpszRestarting     = L"Restarting Casso...";
    static constexpr LPCWSTR  kpszUpToDate       = L"Casso is up to date.";

    static UpdateButtonSet  SelectButtons        (InstallType installType, bool hasAsset);
    static UpdateButtonSet  SelectAfterFailure   (UpdateButtonSet current, UpdateFailure failure);
    static bool             HasAsset             (const ReleaseInfo & release, InstallType installType, ReleaseArch arch);

    using RandomIndexFn = std::function<size_t (size_t count)>;
    using JudgementList = std::span<const LPCWSTR>;

    static std::wstring     MakeHeader           (const ReleaseVersion  & newer,
                                                  const std::string     & publishedDate,
                                                  const ReleaseVersion  & running,
                                                  std::wstring_view       judgement);
    static std::wstring     MakeUpdateLabel      (const ReleaseVersion & newer);
    static JudgementList    GetJudgements        ();
    static std::wstring     PickJudgement        (const RandomIndexFn & randomIndex);
    static std::wstring     MakeUpToDateText     (const ReleaseVersion & running);
    static std::wstring     MakeProgressText     (std::uint64_t bytesDone, std::uint64_t bytesTotal);
    static std::wstring     DescribeFailure      (UpdateFailure failure);
    static std::wstring     MakeCheckFailedText  (UpdateFailure failure);
    static std::wstring     MakeUpdateFailedText (UpdateFailure failure);
    static void             FormatNotes          (const ReleaseNotes & notes, std::vector<FormattedLine> & outLines);
    static std::string      StripVersionBrackets (const std::string & heading);

private:
    static std::wstring     FormatMegabytes      (std::uint64_t bytes);
};
