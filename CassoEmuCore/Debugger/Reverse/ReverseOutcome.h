#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOutcome
//
//  How a reverse command ended. Moved: the machine is at the target.
//  AtHistoryStart: no earlier position satisfied the command, so the machine
//  is at the oldest position history holds. AtHistoryGap: the target lay in
//  a stretch recording skipped (Maximum speed chosen by the user), so the
//  machine stopped at the edge of it. HistoryCut: a replay diverged from a
//  keyframe's checksum, history after the last good keyframe was dropped,
//  and the machine is live at that keyframe.
//
////////////////////////////////////////////////////////////////////////////////

enum class ReverseOutcome
{
    Moved,
    AtHistoryStart,
    AtHistoryGap,
    HistoryCut,
};
