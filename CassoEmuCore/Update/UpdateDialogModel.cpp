#include "Pch.h"

#include "Update/UpdateDialogModel.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::SelectButtons
//
//  Only an official copy with a download for it is offered the update. A
//  developer build is told to pull and rebuild. Anything else, including a
//  copy whose install type is not known yet, gets the release page.
//
////////////////////////////////////////////////////////////////////////////////

UpdateButtonSet UpdateDialogModel::SelectButtons (InstallType installType, bool hasAsset)
{
    UpdateButtonSet  buttons    = UpdateButtonSet::ReleasePage;
    bool             isOfficial = installType == InstallType::Zip || installType == InstallType::Msix;



    if (installType == InstallType::Developer)
    {
        buttons = UpdateButtonSet::Developer;
    }
    else if (isOfficial && hasAsset)
    {
        buttons = UpdateButtonSet::UpdateNow;
    }

    return buttons;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::SelectAfterFailure
//
//  A failed update that a retry cannot fix -- a folder Casso cannot write
//  to, or no download for this copy -- swaps the update button for the release
//  page. Any other failure leaves the update button in place to try again.
//
////////////////////////////////////////////////////////////////////////////////

UpdateButtonSet UpdateDialogModel::SelectAfterFailure (UpdateButtonSet current, UpdateFailure failure)
{
    UpdateButtonSet  buttons = current;



    if (failure == UpdateFailure::FolderNotWritable ||
        failure == UpdateFailure::NoAsset           ||
        failure == UpdateFailure::NotOfficial       ||
        failure == UpdateFailure::RestoreFailed)
    {
        buttons = UpdateButtonSet::ReleasePage;
    }

    return buttons;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::HasAsset
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateDialogModel::HasAsset (const ReleaseInfo & release, InstallType installType, ReleaseArch arch)
{
    const ReleaseAsset  * asset = nullptr;



    if (installType == InstallType::Zip)
    {
        asset = release.FindZipAsset (arch);
    }
    else if (installType == InstallType::Msix)
    {
        asset = release.FindBundleAsset();
    }

    return asset != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_kJudgements
//
//  The closing remark on the update dialog's header, one picked per dialog.
//  Dry and good-natured: the joke is on the old version, never the user.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr LPCWSTR  s_kJudgements[] =
{
    L"how gauche",
    L"how quaint",
    L"positively vintage",
    L"the Apple II approves, at least",
    L"very retro of you",
    L"a bold fashion choice",
    L"practically an heirloom",
    L"how delightfully last season",
    L"Woz would understand",
    L"charming, in a museum sort of way",
    L"the floppy drives are blushing",
    L"a classic, if not a current one",
};





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeHeader
//
//  "Casso 1.30.0 (released 2026-10-03) is available. Sadly, you're still
//  using 1.29.0--how gauche." with an em dash abutting both sides. A release
//  with no date leaves the parenthesis out.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeHeader (
    const ReleaseVersion  & newer,
    const std::string     & publishedDate,
    const ReleaseVersion  & running,
    std::wstring_view       judgement)
{
    std::string   newerText   = newer.ToString();
    std::string   runningText = running.ToString();
    std::wstring  released;



    if (!publishedDate.empty())
    {
        released = L" (released " + std::wstring (publishedDate.begin(), publishedDate.end()) + L")";
    }

    return std::format (L"Casso {}{} is available. Sadly, you're still using {}{}{}.",
                        std::wstring (newerText.begin(),   newerText.end()),
                        released,
                        std::wstring (runningText.begin(), runningText.end()),
                        s_kchEmDash,
                        judgement);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeUpdateLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeUpdateLabel (const ReleaseVersion & newer)
{
    std::string  newerText = newer.ToString();



    return L"Update to " + std::wstring (newerText.begin(), newerText.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::GetJudgements
//
////////////////////////////////////////////////////////////////////////////////

UpdateDialogModel::JudgementList UpdateDialogModel::GetJudgements()
{
    return JudgementList (s_kJudgements);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::PickJudgement
//
//  The random source returns an index below the count it is given; one out
//  of range is clamped rather than read past the end.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::PickJudgement (const RandomIndexFn & randomIndex)
{
    JudgementList  list  = GetJudgements();
    size_t         index = randomIndex ? randomIndex (list.size()) : 0;



    return list[std::min (index, list.size() - 1)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeUpToDateText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeUpToDateText (const ReleaseVersion & running)
{
    std::string  runningText = running.ToString();



    return std::format (L"{}\n\nYou have version {}, the latest release.",
                        kpszUpToDate,
                        std::wstring (runningText.begin(), runningText.end()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::FormatMegabytes
//
//  One decimal place, which is as fine as a progress line needs.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::FormatMegabytes (std::uint64_t bytes)
{
    constexpr double  kBytesPerMegabyte = 1024.0 * 1024.0;



    return std::format (L"{:.1f}", (double) bytes / kBytesPerMegabyte);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeProgressText
//
//  A total of zero means the size is unknown, so only the count shows.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeProgressText (std::uint64_t bytesDone, std::uint64_t bytesTotal)
{
    std::wstring  text;



    if (bytesTotal > 0)
    {
        text = std::format (L"Downloading: {} of {} MB", FormatMegabytes (bytesDone), FormatMegabytes (bytesTotal));
    }
    else
    {
        text = std::format (L"Downloading: {} MB", FormatMegabytes (bytesDone));
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::DescribeFailure
//
//  One plain sentence of cause per failure, for the line under the label.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::DescribeFailure (UpdateFailure failure)
{
    std::wstring  text;



    switch (failure)
    {
        case UpdateFailure::None:
            break;

        case UpdateFailure::Network:
            text = L"Casso could not reach GitHub. Check the network connection and try again.";
            break;

        case UpdateFailure::RateLimited:
            text = L"GitHub is not accepting more requests from this network right now. Try again in an hour.";
            break;

        case UpdateFailure::BadData:
            text = L"The release information from GitHub could not be read.";
            break;

        case UpdateFailure::NoAsset:
            text = L"This release has no download for this copy of Casso.";
            break;

        case UpdateFailure::DigestMismatch:
            text = L"The download did not match the release's checksum, so nothing was changed.";
            break;

        case UpdateFailure::NotOfficial:
            text = L"This copy of Casso is not an official release, so it cannot update itself.";
            break;

        case UpdateFailure::FolderNotWritable:
            text = L"Casso cannot write to the folder it runs from. Download the release from the release page instead.";
            break;

        case UpdateFailure::OtherInstanceRunning:
            text = L"Another copy of Casso is open. Close it, then try the update again.";
            break;

        case UpdateFailure::InstallFailed:
            text = L"The update could not be installed, and the previous version was kept.";
            break;

        case UpdateFailure::RestoreFailed:
            text = L"The update failed, and the previous version could not be fully put back. "
                   L"Download the release from the release page to repair this copy.";
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeCheckFailedText
//
//  A label line, then the cause as a sentence.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeCheckFailedText (UpdateFailure failure)
{
    return L"Error: update check failed\n\n" + DescribeFailure (failure);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeUpdateFailedText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeUpdateFailedText (UpdateFailure failure)
{
    return L"Error: update failed\n" + DescribeFailure (failure);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::StripVersionBrackets
//
//  "[1.30.0] - 2026-10-03: Title" reads "1.30.0 - 2026-10-03: Title". The
//  brackets are markdown link syntax in the CHANGELOG, not part of the text.
//  A heading that does not open with a bracketed span is returned as is.
//
////////////////////////////////////////////////////////////////////////////////

std::string UpdateDialogModel::StripVersionBrackets (const std::string & heading)
{
    size_t       close   = heading.find (']');
    std::string  result  = heading;



    if (heading.starts_with ('[') && close != std::string::npos)
    {
        result = heading.substr (1, close - 1) + heading.substr (close + 1);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::FormatNotes
//
//  The README highlights first, each under its title, then every CHANGELOG
//  section under its own heading line, newest first as the extractor
//  ordered them. A blank line separates one section from the next.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogModel::FormatNotes (const ReleaseNotes & notes, std::vector<FormattedLine> & outLines)
{
    constexpr int  kHighlightLevel = 2;
    constexpr int  kChangesLevel   = 3;



    FormattedLine               heading;
    FormattedLine               blank;
    FormattedRun                title;
    std::vector<FormattedLine>  body;



    outLines.clear();
    blank.kind   = FormattedLineKind::Blank;
    heading.kind = FormattedLineKind::Heading;

    for (const std::vector<NotesSection> * sections : { &notes.highlights, &notes.changes })
    {
        for (const NotesSection & section : *sections)
        {
            if (!outLines.empty())
            {
                outLines.push_back (blank);
            }

            title.text           = (sections == &notes.changes) ? StripVersionBrackets (section.heading) : section.heading;
            heading.headingLevel = (sections == &notes.highlights) ? kHighlightLevel : kChangesLevel;
            heading.runs         = { title };
            outLines.push_back (heading);

            body.clear();
            ReleaseNotesFormatter::Format (section.body, body);
            outLines.insert (outLines.end(), body.begin(), body.end());
        }
    }
}
