#pragma once

#include "Pch.h"

#include "Ui/Debugger/Panes/EventLogPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane
//
//  The serial log's or the network log's data view: a row for each byte a
//  serial port sent or received, or each send and receive of a socket, with
//  the port or socket, the direction, the length, and the bytes as text and
//  in hex. The settings, line and state rows are in the graph alone. Which
//  of the graph's lanes are shown is kept here, with the columns, in the
//  pane's options.
//
////////////////////////////////////////////////////////////////////////////////

class CommLogPane : public EventLogPane<IoEvent>
{
public:
    enum Column : size_t
    {
        kTime,
        kCycle,
        kPort,
        kDirection,
        kLength,
        kText,
        kHex,
        kColumnCount,
    };

    static constexpr size_t  kMaxHexBytes = 64;

    CommLogPane (DxuiListView * list, RequestFn request, bool isNetwork);

    bool  IsNetwork () const { return m_isNetwork; }

    void  SetLaneShown (size_t lane, bool isShown);
    bool  IsLaneShown  (size_t lane) const { return lane < m_lanes.size() && m_lanes[lane]; }
    const std::vector<bool> &  GetLanesShown () const { return m_lanes; }

    std::string   GetOptionsText () const;
    void          SetOptionsText (const std::string & text);

    //  A row's bytes as hex, at most kMaxHexBytes with a count of the rest,
    //  and as text, a dot for each byte that is not printable ASCII.
    static std::wstring  FormatHex  (std::span<const Byte> bytes);
    static std::wstring  FormatText (std::span<const Byte> bytes);

    static bool  IsDataRow (const IoEvent & event);

protected:
    ColumnSpecs  GetColumnSpecs () const override;
    bool         Matches        (const Row & row, size_t index) const override;
    void         MakeCells      (const Row & row, Cells & out) const override;
    size_t       GetLabelColumn () const override { return kPort; }
    size_t       GetTextColumn  () const override { return kHex; }

private:
    bool               m_isNetwork = false;
    std::vector<bool>  m_lanes;
};
