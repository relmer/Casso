#pragma once

#include "Pch.h"

#include "Ui/Debugger/Panes/EventLogPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane
//
//  The video log or the sound log (FR-198), as VIDEOLOG and SOUNDLOG list
//  them, a row an entry: the cycle; for a video switch the frame it fell in,
//  where the beam was, and the modes it turned on and off; for a speaker
//  toggle or a Mockingboard register write the cycles since the last of the
//  same, the speaker's or the board's, and what was written. Every column
//  either log has is in both, the ones that log does not use hidden to
//  start with.
//
//  The sound log's filter is by source: the speaker, the boards' 6522s, and
//  their AY-3-8910s.
//
////////////////////////////////////////////////////////////////////////////////

class IoLogPane : public EventLogPane<IoEvent>
{
public:
    enum Column : size_t
    {
        kTime,
        kCycle,
        kFrame,
        kLine,
        kHorizontal,
        kSince,
        kAddress,
        kSource,
        kEvent,
        kColumnCount,
    };

    enum Lane : size_t
    {
        kSpeaker,
        kVia,
        kAy,
        kLaneCount,
    };

    IoLogPane (DxuiListView * list, RequestFn request, bool isVideo) :
        EventLogPane (list, std::move (request)),
        m_isVideo    (isVideo)
    {
    }

    bool  IsVideo      () const { return m_isVideo; }

    void  SetLaneShown (Lane lane, bool isShown);
    bool  IsLaneShown  (Lane lane) const { return m_lanes[lane]; }

    //  The filters and columns as text for the preferences, and back.
    std::string   GetOptionsText () const;
    void          SetOptionsText (const std::string & text);

    static const wchar_t *  GetLaneLabel (Lane lane);
    static Lane             GetLane      (const IoEvent & event);

    //  The cycles since the last entry of the same, for the row at index;
    //  none for the first after a gap, and for a video switch.
    std::optional<uint64_t>  GetSince (size_t index) const;

protected:
    ColumnSpecs  GetColumnSpecs () const override;
    void         PrepareView    (const std::vector<Row> & rows, size_t from) override;
    bool         Matches        (const Row & row, size_t index) const override;
    void         MakeCells      (const Row & row, Cells & out) const override;
    size_t       GetLabelColumn () const override { return kSource; }
    size_t       GetTextColumn  () const override { return kEvent; }
    std::optional<uint64_t>  GetSortNumber (const Row & row, size_t index, size_t column) const override;

private:
    bool                          m_isVideo = false;
    std::array<bool, kLaneCount>  m_lanes   = { true, true, true };

    //  For each row, the cycles since the last entry of the same.
    std::vector<std::optional<uint64_t>>  m_since;
};
