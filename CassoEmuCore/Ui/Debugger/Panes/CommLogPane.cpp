#include "Pch.h"

#include "Ui/Debugger/Panes/CommLogPane.h"
#include "Core/TextEncoding.h"
#include "Debugger/IoEventLog.h"
#include "Ui/Debugger/Panes/CommGraph.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::CommLogPane
//
////////////////////////////////////////////////////////////////////////////////

CommLogPane::CommLogPane (
    DxuiListView  * list,
    RequestFn       request,
    bool            isNetwork) :
    EventLogPane (list, std::move (request)),
    m_isNetwork  (isNetwork),
    m_lanes      (isNetwork ? CommGraph::kSocketCount : CommGraph::kSerialLaneCount, true)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::SetLaneShown
//
////////////////////////////////////////////////////////////////////////////////

void CommLogPane::SetLaneShown (
    size_t  lane,
    bool    isShown)
{
    if (lane < m_lanes.size())
    {
        m_lanes[lane] = isShown;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::GetOptionsText
//
////////////////////////////////////////////////////////////////////////////////

std::string CommLogPane::GetOptionsText() const
{
    std::string  lanes;



    for (bool isShown : m_lanes)
    {
        lanes += isShown ? '1' : '0';
    }

    return std::format ("lanes={};columnLayout={}", lanes, GetColumnsText());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::SetOptionsText
//
////////////////////////////////////////////////////////////////////////////////

void CommLogPane::SetOptionsText (const std::string & text)
{
    size_t       start = 0;
    size_t       end   = 0;
    size_t       equal = 0;
    size_t       i     = 0;
    std::string  field;
    std::string  key;
    std::string  value;



    while (start < text.size())
    {
        end   = text.find (';', start);
        end   = (end == std::string::npos) ? text.size() : end;
        field = text.substr (start, end - start);
        equal = field.find ('=');
        key   = field.substr (0, equal);
        value = (equal == std::string::npos) ? std::string() : field.substr (equal + 1);

        if (key == "lanes")
        {
            for (i = 0; i < value.size() && i < m_lanes.size(); i++)
            {
                m_lanes[i] = value[i] == '1';
            }
        }
        else if (key == "columnLayout")
        {
            SetColumnsText (value);
        }

        start = end + 1;
    }

    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::IsDataRow
//
////////////////////////////////////////////////////////////////////////////////

bool CommLogPane::IsDataRow (const IoEvent & event)
{
    switch (event.kind)
    {
    case IoEventKind::SerialSent:
    case IoEventKind::SerialReceived:
    case IoEventKind::SocketSent:
    case IoEventKind::SocketReceived:
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::FormatHex
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommLogPane::FormatHex (std::span<const Byte> bytes)
{
    std::wstring  text;
    size_t        count = std::min (bytes.size(), kMaxHexBytes);



    for (size_t i = 0; i < count; i++)
    {
        text += std::format (L"{}{:02X}", (i > 0) ? L" " : L"", bytes[i]);
    }

    if (bytes.size() > count)
    {
        text += std::format (L" (+{} more)", bytes.size() - count);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::FormatText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommLogPane::FormatText (std::span<const Byte> bytes)
{
    constexpr Byte  kFirstPrintable = 0x20;
    constexpr Byte  kLastPrintable  = 0x7E;
    std::wstring    text;
    size_t          count           = std::min (bytes.size(), kMaxHexBytes);



    for (size_t i = 0; i < count; i++)
    {
        text += (bytes[i] >= kFirstPrintable && bytes[i] <= kLastPrintable) ? (wchar_t) bytes[i] : L'.';
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::GetColumnSpecs
//
//  The length only for the network log; a serial row is a byte.
//
////////////////////////////////////////////////////////////////////////////////

CommLogPane::ColumnSpecs CommLogPane::GetColumnSpecs() const
{
    return { { L"Time",      DxuiTextHAlign::Left,  kFit, true        },
             { L"Cycle",     DxuiTextHAlign::Right, 110,  true        },
             { m_isNetwork ? L"Socket" : L"Port", DxuiTextHAlign::Left, 70, true },
             { L"Direction", DxuiTextHAlign::Left,   80,  true        },
             { L"Length",    DxuiTextHAlign::Right,  60,  m_isNetwork },
             { L"Text",      DxuiTextHAlign::Left,  160,  true        },
             { L"Hex",       DxuiTextHAlign::Left,    0,  true        } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool CommLogPane::Matches (
    const Row  & row,
    size_t       index) const
{
    (void) index;

    return IsDataRow (row.event);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogPane::MakeCells
//
////////////////////////////////////////////////////////////////////////////////

void CommLogPane::MakeCells (
    const Row  & row,
    Cells      & out) const
{
    const IoEvent          & event      = row.event;
    bool                     isReceived = event.kind == IoEventKind::SerialReceived || event.kind == IoEventKind::SocketReceived;
    std::span<const Byte>    bytes;



    if (m_isNetwork)
    {
        bytes = (row.data != nullptr) ? std::span<const Byte> (*row.data) : std::span<const Byte>();

        out[kPort].text   = std::format (L"Socket {}", event.chip);
        out[kLength].text = std::format (L"{}", event.length);
    }
    else
    {
        bytes = std::span<const Byte> (&event.value, 1);

        out[kPort].text = TextEncoding::NarrowToWide (IoEventLog::GetSerialPortName (event.chip));
    }

    out[kDirection].text = isReceived ? L"Received" : L"Sent";
    out[kText].text      = FormatText (bytes);
    out[kHex].text       = FormatHex (bytes);
}
