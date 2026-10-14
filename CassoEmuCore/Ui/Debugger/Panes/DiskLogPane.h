#pragma once

#include "Pch.h"

#include "Ui/Debugger/Panes/EventLogPane.h"
#include "Ui/Debugger/TrackSectorPredicate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogPane
//
//  The Disk II event log (FR-213): the controller's events with their cycle,
//  drive, severity, label and detail, and the host's clock as an optional
//  column. A sector's row has its first bytes in the detail and the whole
//  sector in the detail's tip; warning and error rows have fills of their
//  own (FR-231, FR-232).
//
//  The filters are those of the emulator's old Disk II window: a check for
//  each kind of event, a drive, and track and sector lists, and a check for
//  each severity. A row with no
//  track or sector passes those lists, and a read stall has no kind of its
//  own, so only the drive and track filters apply to it. Head steps and
//  bumps match the track list by quarter-track: a whole track matches every
//  quarter-track on it, and 17.25 matches that one.
//
////////////////////////////////////////////////////////////////////////////////

class DiskLogPane : public EventLogPane<Disk2Event>
{
public:
    enum Column : size_t
    {
        kCycle,
        kDrive,
        kSeverity,
        kEvent,
        kDetail,
        kWall,
        kColumnCount,
    };

    enum Kind : size_t
    {
        kMotor,
        kHeadStep,
        kHeadBump,
        kAddrMark,
        kRead,
        kWrite,
        kDoor,
        kDriveSelect,
        kKindCount,
        kNoKind = kKindCount,
    };

    enum Severity : size_t
    {
        kInfo,
        kWarning,
        kError,
        kSeverityCount,
    };

    //  The count of a sector's bytes in its detail, and on each line of its
    //  hex dump.
    static constexpr size_t  kDetailBytes = 8;
    static constexpr size_t  kDumpBytes   = 16;

    DiskLogPane (DxuiListView * list, RequestFn request) : EventLogPane (list, std::move (request)) {}

    void  SetKindShown     (Kind kind, bool isShown);
    bool  IsKindShown      (Kind kind) const { return m_kinds[kind]; }
    void  SetSeverityShown (Severity severity, bool isShown);
    bool  IsSeverityShown  (Severity severity) const { return m_severities[severity]; }

    //  The fills of warning and error rows, from the theme.
    void  SetSeverityFills (uint32_t warningArgb, uint32_t errorArgb);
    void  SetDrive         (int drive);
    int   GetDrive         () const { return m_drive; }
    void  SetTrackText     (const std::wstring & text);
    void  SetSectorText    (const std::wstring & text);

    const std::wstring &  GetTrackText  () const { return m_trackText; }
    const std::wstring &  GetSectorText () const { return m_sectorText; }

    //  "Invalid track: abc, 99" and "Invalid sector: 16", each when the list
    //  has tokens it did not take; empty when neither does.
    std::wstring  GetInvalidText () const;

    //  The filters and columns as text for the preferences, and back.
    std::string   GetOptionsText () const;
    void          SetOptionsText (const std::string & text);

    static const wchar_t *  GetKindLabel      (Kind kind);
    static const wchar_t *  GetKindTip        (Kind kind);
    static Kind             GetKindOf         (Disk2EventType type);
    static Severity         GetSeverityOf     (Disk2EventType type);
    static const wchar_t *  GetSeverityLabel  (Severity severity);
    static const wchar_t *  GetEventLabel     (Disk2EventType type);
    static std::wstring     GetDetail         (const Disk2Event & event, const std::vector<Byte> * bytes = nullptr);
    static std::wstring     GetHexDump        (const std::vector<Byte> & bytes);

protected:
    ColumnSpecs  GetColumnSpecs () const override;
    void         NoteFitTexts   () override;
    bool         Matches        (const Row & row, size_t index) const override;
    void         MakeCells      (const Row & row, Cells & out) const override;
    size_t       GetLabelColumn () const override { return kEvent; }
    size_t       GetTextColumn  () const override { return kDetail; }

private:
    static std::wstring  ListRejected (const std::wstring & text, const TrackSectorPredicate & predicate);
    static std::wstring  CleanForOptions (const std::wstring & text);

    void  ParseTrack ();

    std::array<bool, kKindCount>      m_kinds       = { true, true, true, true, true, true, true, true };
    std::array<bool, kSeverityCount>  m_severities  = { true, true, true };
    int                               m_drive       = 0;
    uint32_t                          m_warningArgb = 0;
    uint32_t                          m_errorArgb   = 0;
    std::wstring                      m_trackText;
    std::wstring                      m_sectorText;
    TrackSectorPredicate              m_track;
    TrackSectorPredicate              m_sector;
};
