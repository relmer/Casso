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
//  s_kAgeRemarks
//
//  Full-sentence remarks on how long the running build has gone without an
//  update. {0} is the age ("1 day", "47 days"), {1} the running version, {2}
//  an em dash. Same voice as the short remarks: the old version is the joke.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr LPCWSTR  s_kAgeRemarks[] =
{
    L"Wait, this can't be right{2}you haven't updated Casso in {0}? Srsly?",
    L"{1} shipped {0} ago. It's practically retro-computing itself.",
    L"That's {0} without an update. Even the Disk II has moved on.",
    L"{0} on {1}. The phosphors are starting to burn in.",
    L"Your copy is {0} old. In emulator years, that's a collector's item.",
    L"{1} came out {0} ago{2}the Apple II waited longer for less, but still.",
    L"It has been {0} since {1}. The floppies have stopped asking where you've been.",
    L"Mal would've updated {0} ago.",
};





////////////////////////////////////////////////////////////////////////////////
//
//  s_kOpeners
//
//  The excited line above the update dialog's header, one per dialog. The
//  first six and the Firefly lines are the owner's own words, kept verbatim.
//  {0} is an em dash, which a constant array cannot splice in from
//  UnicodeSymbols.h, so GetOpeners fills it.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr LPCWSTR  s_kOpeners[] =
{
    L"Ooh ooh, new toys, new toys!!",
    L"ZOMG! Fresh Casso available!!",
    L"I love it when a plan comes together.",
    L"I love the smell of fresh Casso in the morning!",
    L"This just in...",
    L"Huzzah!",
    L"Hot off the assembler!",
    L"New bits, fresh from the oven!",
    L"Stop the presses: there's a new Casso!",
    L"Somebody's been busy!",
    L"New Casso just dropped{0}shiny!",
    L"Curse your sudden but inevitable update!",
    L"Gorram it, there's a new Casso.",
    L"Can't stop the signal{0}or the updates.",
    L"Everything's shiny, Cap'n. A new Casso's in the black.",
};





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::GetOpeners
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> UpdateDialogModel::GetOpeners()
{
    std::vector<std::wstring>  openers;
    wchar_t                    dash = s_kchEmDash;



    for (LPCWSTR pattern : s_kOpeners)
    {
        openers.push_back (std::vformat (pattern, std::make_wformat_args (dash)));
    }

    return openers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::PickOpener
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::PickOpener (const RandomIndexFn & randomIndex)
{
    std::vector<std::wstring>  list  = GetOpeners();
    size_t                     index = randomIndex ? randomIndex (list.size()) : 0;



    return list[std::min (index, list.size() - 1)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_kDeveloperNudges
//
//  What a developer build shows where the update buttons would be: a copy
//  built from source updates by pulling and rebuilding, never in place.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr LPCWSTR  s_kDeveloperNudges[] =
{
    L"Psst... you should probably pull and rebuild.",
    L"Psst... a git pull and a rebuild would fix this right up.",
    L"Built from source? Then pull and rebuild, you know the drill.",
    L"There are fresh commits upstream. Pull and rebuild.",
    L"Developer build spotted. Pull, rebuild, carry on.",
};





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::GetDeveloperNudges
//
////////////////////////////////////////////////////////////////////////////////

UpdateDialogModel::JudgementList UpdateDialogModel::GetDeveloperNudges()
{
    return JudgementList (s_kDeveloperNudges);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::PickDeveloperNudge
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::PickDeveloperNudge (const RandomIndexFn & randomIndex)
{
    JudgementList  list  = GetDeveloperNudges();
    size_t         index = randomIndex ? randomIndex (list.size()) : 0;



    return list[std::min (index, list.size() - 1)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::GetAgeRemarkCount
//
////////////////////////////////////////////////////////////////////////////////

size_t UpdateDialogModel::GetAgeRemarkCount()
{
    return std::size (s_kAgeRemarks);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeAgeRemark
//
//  One age remark with the age filled in, singular for a single day. An
//  index past the end is clamped.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeAgeRemark (size_t index, int days, const ReleaseVersion & running)
{
    std::string   runningText = running.ToString();
    std::wstring  version (runningText.begin(), runningText.end());
    std::wstring  age         = (days == 1) ? std::wstring (L"1 day") : std::format (L"{} days", days);
    wchar_t       dash        = s_kchEmDash;
    LPCWSTR       pattern     = s_kAgeRemarks[std::min (index, std::size (s_kAgeRemarks) - 1)];



    return std::vformat (pattern, std::make_wformat_args (age, version, dash));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::TryGetDaysSince
//
//  Whole days from a "YYYY-MM-DD" date to `nowUtc` (Unix seconds), counted
//  in UTC calendar days. False for text that is not a real date, and for a
//  date after today, which has no age to remark on.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateDialogModel::TryGetDaysSince (std::string_view date, std::int64_t nowUtc, int & outDays)
{
    constexpr std::int64_t  kSecondsPerDay = 86400;
    constexpr size_t        kMonthAt       = 5;
    constexpr size_t        kDayAt         = 8;
    constexpr size_t        kYearDigits    = 4;
    constexpr size_t        kPartDigits    = 2;



    int                            year    = 0;
    unsigned                       month   = 0;
    unsigned                       day     = 0;
    std::chrono::year_month_day    ymd;
    std::int64_t                   then    = 0;
    std::int64_t                   today   = 0;
    bool                           isKnown = ReleaseNotesExtractor::IsDateText (date);



    outDays = 0;

    if (isKnown)
    {
        std::from_chars (date.data(),            date.data() + kYearDigits,           year);
        std::from_chars (date.data() + kMonthAt, date.data() + kMonthAt + kPartDigits, month);
        std::from_chars (date.data() + kDayAt,   date.data() + kDayAt + kPartDigits,   day);

        ymd     = std::chrono::year_month_day { std::chrono::year (year), std::chrono::month (month), std::chrono::day (day) };
        isKnown = ymd.ok();
    }

    if (isKnown)
    {
        then    = std::chrono::sys_days (ymd).time_since_epoch().count();
        today   = (nowUtc >= 0 ? nowUtc : nowUtc - kSecondsPerDay + 1) / kSecondsPerDay;
        isKnown = today >= then;
        outDays = isKnown ? (int) (today - then) : 0;
    }

    return isKnown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeAgeHeader
//
//  The header that ends plainly, "...Sadly, you're still using 1.29.0.",
//  with an age remark as its own sentence on the next line. An empty
//  remark leaves just the first line, which is what the dialog shows until
//  the notes (and with them the running build's date) arrive.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeAgeHeader (
    const ReleaseVersion  & newer,
    const std::string     & publishedDate,
    const ReleaseVersion  & running,
    std::wstring_view       ageRemark)
{
    std::wstring  header = MakeHeader (newer, publishedDate, running, L"");



    // MakeHeader ends with a dash and an empty remark; a plain sentence ends
    // with the version instead.
    header.resize (header.size() - 2);
    header += L".";

    if (!ageRemark.empty())
    {
        header += L"\n";
        header += ageRemark;
    }

    return header;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeFinalHeader
//
//  One remark per dialog. With the running build's age known, the pick
//  ranges over the short judgements and the age remarks together; without
//  it, over the short judgements only.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeFinalHeader (
    const ReleaseVersion  & newer,
    const std::string     & publishedDate,
    const ReleaseVersion  & running,
    std::optional<int>      ageDays,
    const RandomIndexFn   & randomIndex)
{
    size_t        shortCount = GetJudgements().size();
    size_t        ageCount   = ageDays.has_value() ? GetAgeRemarkCount() : 0;
    size_t        total      = shortCount + ageCount;
    size_t        index      = randomIndex ? std::min (randomIndex (total), total - 1) : 0;
    std::wstring  header;



    if (index < shortCount)
    {
        header = MakeHeader (newer, publishedDate, running, GetJudgements()[index]);
    }
    else
    {
        header = MakeAgeHeader (newer, publishedDate, running, MakeAgeRemark (index - shortCount, ageDays.value_or (0), running));
    }

    return header;
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





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::FormatNotesTab
//
//  One tab's notes: What's new is the README highlights, each under its
//  title; Changelog is the CHANGELOG sections, each under its heading line.
//  A blank line separates one section from the next.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogModel::FormatNotesTab (const ReleaseNotes & notes, NotesTab tab, std::vector<FormattedLine> & outLines)
{
    ReleaseNotes  only;



    if (tab == NotesTab::WhatsNew)
    {
        only.highlights = notes.highlights;
    }
    else
    {
        only.changes = notes.changes;
    }

    FormatNotes (only, outLines);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::GetNotesTabs
//
//  The tabs the notes get, the default first: What's new when the README
//  has highlights for the range, then Changelog. Without highlights there
//  is only Changelog.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<NotesTab> UpdateDialogModel::GetNotesTabs (const ReleaseNotes & notes)
{
    std::vector<NotesTab>  tabs;



    if (!notes.highlights.empty())
    {
        tabs.push_back (NotesTab::WhatsNew);
    }

    tabs.push_back (NotesTab::Changelog);

    return tabs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::ShowsTabStrip
//
//  A strip only when there is a choice to make: a lone tab would be a
//  control that does nothing, so the changelog alone shows without one.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateDialogModel::ShowsTabStrip (const std::vector<NotesTab> & tabs)
{
    return tabs.size() > 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::GetTabLabel
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR UpdateDialogModel::GetTabLabel (NotesTab tab)
{
    return (tab == NotesTab::WhatsNew) ? kpszWhatsNewTab : kpszChangelogTab;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::CountWrappedLines
//
//  How many lines `text` takes wrapped word by word into `widthPx`, as a
//  wrapping draw lays it out: each newline starts a line, and a word that
//  does not fit on the current line starts the next. A word wider than the
//  whole line still takes one line. Empty text takes none.
//
////////////////////////////////////////////////////////////////////////////////

int UpdateDialogModel::CountWrappedLines (std::wstring_view text, float widthPx, const MeasureWidthFn & measure)
{
    int           lines     = 0;
    size_t        start     = 0;
    size_t        end       = 0;
    size_t        wordEnd   = 0;
    std::wstring  line;
    std::wstring  candidate;
    std::wstring  word;



    while (start <= text.size() && !text.empty())
    {
        end = text.find (L'\n', start);
        end = (end == std::wstring_view::npos) ? text.size() : end;

        lines++;
        line.clear();

        for (size_t at = start; at < end; at = wordEnd + 1)
        {
            wordEnd = text.find (L' ', at);
            wordEnd = (wordEnd == std::wstring_view::npos || wordEnd > end) ? end : wordEnd;
            word    = std::wstring (text.substr (at, wordEnd - at));

            candidate = line.empty() ? word : line + L" " + word;

            if (!line.empty() && measure (candidate) > widthPx)
            {
                lines++;
                candidate = word;
            }

            line = candidate;
        }

        start = end + 1;
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogModel::MakeUpdatedNotice
//
//  What the first launch after an update says.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateDialogModel::MakeUpdatedNotice (const std::string & version)
{
    return L"Casso was updated to version " + std::wstring (version.begin(), version.end()) + L".";
}
