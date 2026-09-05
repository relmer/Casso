#include "Pch.h"

#include "Capture/CaptureOutcome.h"




static constexpr const wchar_t *  s_kpszNothingRendered = L"Nothing to capture -- the window is minimized";
static constexpr const wchar_t *  s_kpszCopiedOnly      = L"Screenshot copied to the clipboard";
static constexpr const wchar_t *  s_kpszNothingWorked   = L"Screenshot failed: could not copy or save";
static constexpr const wchar_t *  s_kpszSaveFailed      = L"Screenshot copied, but the file could not be saved";
static constexpr const wchar_t *  s_kpszCopyFailed      = L"Saved {}, but the clipboard was unavailable";
static constexpr const wchar_t *  s_kpszSaved           = L"Screenshot saved as {}";





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeResult
//
//  Six sentences over the reachable states, in the order that reads best when
//  only one of them is ever shown.
//
//  A REFUSAL IS REPORTED FIRST and on its own. Nothing was attempted, so
//  mentioning the clipboard or the file would only invite the reader to wonder
//  which of them broke.
//
//  The file is named because the name is the useful part -- the folder is
//  configured once and remembered, while the name is what the user types into
//  a file dialog five seconds later. The bare filename rather than the full
//  path: a notice across the picture is not the place for a directory tree,
//  and the path would carry the user's account name into any screenshot OF
//  the notice.
//
////////////////////////////////////////////////////////////////////////////////

wstring CaptureOutcome::DescribeResult (const CaptureOutcome & outcome)
{
    wstring   text;
    wstring   name;



    name = outcome.path.filename().wstring();

    if (outcome.refusal == CaptureRefusal::NothingRendered)
    {
        text = s_kpszNothingRendered;
    }
    else if (outcome.fileWritten && outcome.clipboardOk)
    {
        text = std::vformat (s_kpszSaved, std::make_wformat_args (name));
    }
    else if (outcome.fileWritten)
    {
        text = std::vformat (s_kpszCopyFailed, std::make_wformat_args (name));
    }
    else if (outcome.clipboardOk && outcome.writeAttempted)
    {
        text = s_kpszSaveFailed;
    }
    else if (outcome.clipboardOk)
    {
        text = s_kpszCopiedOnly;
    }
    else
    {
        text = s_kpszNothingWorked;
    }

    return text;
}
