#include "Pch.h"

#include "SectorDecodeReport.h"
#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport::Reset
//
//  Sizes the report for a fresh decode. Every track starts Unformatted with
//  empty coverage, so a track the decoder never reaches reads as "nothing was
//  there" rather than as a silent success.
//
////////////////////////////////////////////////////////////////////////////////

void SectorDecodeReport::Reset (int trackCount)
{
    size_t  count = (trackCount > 0) ? (size_t) trackCount : 0;



    m_outcome.assign    (count, TrackDecodeOutcome::Unformatted);
    m_coverage.assign   (count, (Word) 0);
    m_duplicated.assign (count, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport::SetOutcome
//
////////////////////////////////////////////////////////////////////////////////

void SectorDecodeReport::SetOutcome (
    int                 track,
    TrackDecodeOutcome  outcome,
    Word                coverage,
    bool                duplicated)
{
    bool  inRange = track >= 0 && (size_t) track < m_outcome.size();



    if (inRange)
    {
        m_outcome[(size_t) track]    = outcome;
        m_coverage[(size_t) track]   = coverage;
        m_duplicated[(size_t) track] = duplicated;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport::GetTrackCount
//
////////////////////////////////////////////////////////////////////////////////

int SectorDecodeReport::GetTrackCount() const
{
    return (int) m_outcome.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport::GetOutcome
//
//  Out-of-range reads as Unformatted rather than asserting: a caller asking
//  about a track the image does not have is asking about empty space.
//
////////////////////////////////////////////////////////////////////////////////

TrackDecodeOutcome SectorDecodeReport::GetOutcome (int track) const
{
    bool  inRange = track >= 0 && (size_t) track < m_outcome.size();



    return inRange ? m_outcome[(size_t) track] : TrackDecodeOutcome::Unformatted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport::GetCoverage
//
////////////////////////////////////////////////////////////////////////////////

Word SectorDecodeReport::GetCoverage (int track) const
{
    bool  inRange = track >= 0 && (size_t) track < m_coverage.size();



    return inRange ? m_coverage[(size_t) track] : (Word) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport::IsDuplicated
//
////////////////////////////////////////////////////////////////////////////////

bool SectorDecodeReport::IsDuplicated (int track) const
{
    bool  inRange = track >= 0 && (size_t) track < m_duplicated.size();



    return inRange ? m_duplicated[(size_t) track] : false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport::IsSectorRecovered
//
////////////////////////////////////////////////////////////////////////////////

bool SectorDecodeReport::IsSectorRecovered (int track, int sector) const
{
    Word  mask     = GetCoverage (track);
    bool  inRange  = sector >= 0 && sector < NibblizationLayer::kSectorsPerTrack;



    return inRange && ((mask >> sector) & 1) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport::HasDataLoss
//
//  Partial is the only outcome that means something was lost. Unformatted is
//  benign -- a blank track really is all zeros -- and treating it as damage
//  would make blank and newly formatted regions unwritable.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorDecodeReport::HasDataLoss() const
{
    bool    lost  = false;
    size_t  track = 0;



    for (track = 0; track < m_outcome.size(); track++)
    {
        if (m_outcome[track] == TrackDecodeOutcome::Partial)
        {
            lost = true;
            break;
        }
    }

    return lost;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport::GetUnrecoveredCount
//
////////////////////////////////////////////////////////////////////////////////

int SectorDecodeReport::GetUnrecoveredCount() const
{
    int     count = 0;
    size_t  track = 0;
    int     bit   = 0;



    for (track = 0; track < m_outcome.size(); track++)
    {
        if (m_outcome[track] != TrackDecodeOutcome::Partial)
        {
            continue;
        }

        for (bit = 0; bit < NibblizationLayer::kSectorsPerTrack; bit++)
        {
            if (((m_coverage[track] >> bit) & 1) == 0)
            {
                count++;
            }
        }
    }

    return count;
}
