#include "Pch.h"

#include "CassoLink/Session/CassoLinkDiskStatus.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskStatus::GetTrackMark
//
////////////////////////////////////////////////////////////////////////////////

FullSendTrackMark CassoLinkDiskStatus::GetTrackMark (size_t newUnreadable, size_t newMarks, bool verifying)
{
    return newUnreadable > 0 ? FullSendTrackMark::Unreadable
         : newMarks > 0      ? FullSendTrackMark::Unreliable
         : verifying         ? FullSendTrackMark::Verified
                             : FullSendTrackMark::Read;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskStatus::NoteCapture
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDiskStatus::NoteCapture (FullSendStatus & status, int drive, const CassoLinkCapture::Progress & where)
{
    FullSendSnapshot  snapshot;



    status.SetCapture (drive, where.pass, where.position, where.read, where.reads, where.percent, where.secondsLeft);

    if (where.pass != 2 || where.read != 1)
    {
        return;
    }

    snapshot = status.GetSnapshot();

    for (int position = 0; position < where.position; position++)
    {
        if (snapshot.drives[(size_t) drive - 1].positions[(size_t) position] == FullSendPositionMark::Unread)
        {
            status.MarkPosition (drive, position, FullSendPositionMark::Skipped);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskStatus::NoteCaptureResult
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDiskStatus::NoteCaptureResult (FullSendStatus & status, int drive, const CassoLinkCapture::Result & result)
{
    FullSendSnapshot  snapshot = status.GetSnapshot();



    //  The second pass ends at the last position it read, so the positions
    //  after that one are marked here.
    for (int position = 0; position < FullSendDriveStatus::kPositions; position++)
    {
        if (snapshot.drives[(size_t) drive - 1].positions[(size_t) position] == FullSendPositionMark::Unread)
        {
            status.MarkPosition (drive, position, FullSendPositionMark::Skipped);
        }
    }

    for (int position : result.weak)
    {
        status.MarkPosition (drive, position, FullSendPositionMark::Weak);
    }

    for (const auto & [position, sector] : result.replaced)
    {
        status.AddLine (std::format ("Track {} sector {} was taken from another read", CassoLinkDiskTransfer::FormatPosition (position), sector), drive);
    }

    for (int position : result.unstable)
    {
        status.MarkPosition (drive, position, FullSendPositionMark::Unstable);
    }

    status.SetCaptureTime (drive, result.diskMs / 1000.0, result.lineMs / 1000.0);
}
