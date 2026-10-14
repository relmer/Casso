#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/Panes/DiskLogPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetKindShown
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogPane::SetKindShown (Kind kind, bool isShown)
{
    m_kinds[kind] = isShown;
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSeverityShown
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogPane::SetSeverityShown (Severity severity, bool isShown)
{
    m_severities[severity] = isShown;
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSeverityFills
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogPane::SetSeverityFills (uint32_t warningArgb, uint32_t errorArgb)
{
    if (warningArgb != m_warningArgb || errorArgb != m_errorArgb)
    {
        m_warningArgb = warningArgb;
        m_errorArgb   = errorArgb;
        MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDrive
//
//  0 for every drive, else drive 1 or 2.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogPane::SetDrive (int drive)
{
    m_drive = std::clamp (drive, 0, 2);
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTrackText
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogPane::SetTrackText (const std::wstring & text)
{
    m_trackText = text;
    ParseTrack();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSectorText
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogPane::SetSectorText (const std::wstring & text)
{
    m_sectorText = text;
    m_sector     = TrackSectorPredicate::Parse (text, TrackSectorPredicate::Mode::Sector);
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseTrack
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogPane::ParseTrack()
{
    m_track = TrackSectorPredicate::Parse (m_trackText, TrackSectorPredicate::Mode::Track);
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetInvalidText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskLogPane::GetInvalidText() const
{
    std::wstring  track  = ListRejected (m_trackText,  m_track);
    std::wstring  sector = ListRejected (m_sectorText, m_sector);
    std::wstring  text;



    if (!track.empty())
    {
        text = L"Invalid track: " + track;
    }

    if (!sector.empty())
    {
        text += std::format (L"{}Invalid sector: {}", text.empty() ? L"" : L"    ", sector);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListRejected
//
//  The tokens the list did not take, as typed, a comma between each.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskLogPane::ListRejected (
    const std::wstring          & text,
    const TrackSectorPredicate  & predicate)
{
    std::wstring  list;



    for (const TrackSectorPredicate::RejectedSpan & span : predicate.GetRejectedSpans())
    {
        list += std::format (L"{}{}", list.empty() ? L"" : L", ", text.substr ((size_t) span.beginUtf16, (size_t) (span.endUtf16 - span.beginUtf16)));
    }

    return list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetOptionsText
//
//  kinds=11111111;severity=111;drive=0;track=...;sector=...;columns=111110 0,0,0,0,0,0
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskLogPane::GetOptionsText() const
{
    std::string  kinds;
    std::string  severities;



    for (bool isShown : m_kinds)
    {
        kinds += isShown ? '1' : '0';
    }

    for (bool isShown : m_severities)
    {
        severities += isShown ? '1' : '0';
    }

    return std::format ("kinds={};severity={};drive={};track={};sector={};columns={}", kinds, severities, m_drive,
                        TextEncoding::WideToUtf8 (CleanForOptions (m_trackText)), TextEncoding::WideToUtf8 (CleanForOptions (m_sectorText)),
                        GetColumnsText());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOptionsText
//
//  Fields missing or not understood are left as they were.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogPane::SetOptionsText (const std::string & text)
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

        if (key == "kinds")
        {
            for (i = 0; i < value.size() && i < kKindCount; i++)
            {
                m_kinds[i] = value[i] == '1';
            }
        }
        else if (key == "severity")
        {
            for (i = 0; i < value.size() && i < kSeverityCount; i++)
            {
                m_severities[i] = value[i] == '1';
            }
        }
        else if (key == "drive" && value.size() == 1 && value[0] >= '0' && value[0] <= '2')
        {
            m_drive = value[0] - '0';
        }
        else if (key == "track")
        {
            m_trackText = TextEncoding::Utf8ToWide (value);
        }
        else if (key == "sector")
        {
            m_sectorText = TextEncoding::Utf8ToWide (value);
        }
        else if (key == "columns")
        {
            SetColumnsText (value);
        }

        start = end + 1;
    }

    m_sector = TrackSectorPredicate::Parse (m_sectorText, TrackSectorPredicate::Mode::Sector);
    ParseTrack();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CleanForOptions
//
//  The options text separates its fields with ';' and '='; a list has no use
//  for either.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskLogPane::CleanForOptions (const std::wstring & text)
{
    std::wstring  clean = text;



    std::erase_if (clean, [] (wchar_t ch) { return ch == L';' || ch == L'='; });

    return clean;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetKindLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DiskLogPane::GetKindLabel (Kind kind)
{
    static constexpr const wchar_t * kLabels[kKindCount] =
    {
        L"Motor", L"Head step", L"Head bump", L"Address mark", L"Read", L"Write", L"Disk in and out", L"Drive select",
    };



    return (kind < kKindCount) ? kLabels[kind] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetKindTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DiskLogPane::GetKindTip (Kind kind)
{
    static constexpr const wchar_t * kTips[kKindCount] =
    {
        L"Motor on and off commands, and spin-up and spin-down",
        L"Stepper head moves between tracks",
        L"Head bumps against a track stop",
        L"Address-field reads (track, sector, volume)",
        L"Data-field sector reads",
        L"Data-field sector writes",
        L"Disks inserted and ejected",
        L"Soft-switch drive selection (drive 1 or drive 2)",
    };



    return (kind < kKindCount) ? kTips[kind] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetKindOf
//
//  The kind whose check lists an event; a read stall has none.
//
////////////////////////////////////////////////////////////////////////////////

DiskLogPane::Kind DiskLogPane::GetKindOf (Disk2EventType type)
{
    Kind  kind = kNoKind;



    switch (type)
    {
        case Disk2EventType::MotorCommandOn:
        case Disk2EventType::MotorEngaged:
        case Disk2EventType::MotorCommandOff:
        case Disk2EventType::MotorDisengaged:  kind = kMotor;       break;
        case Disk2EventType::HeadStep:         kind = kHeadStep;    break;
        case Disk2EventType::HeadBump:         kind = kHeadBump;    break;
        case Disk2EventType::AddrMark:
        case Disk2EventType::AddrChecksumError: kind = kAddrMark;   break;
        case Disk2EventType::DataRead:
        case Disk2EventType::DataChecksumError:
        case Disk2EventType::DataCutOff:
        case Disk2EventType::DataNoAddress:    kind = kRead;        break;
        case Disk2EventType::DataWrite:
        case Disk2EventType::WriteProtected:   kind = kWrite;       break;
        case Disk2EventType::DiskInserted:
        case Disk2EventType::DiskEjected:      kind = kDoor;        break;
        case Disk2EventType::DriveSelect:      kind = kDriveSelect; break;
        default:                                                    break;
    }

    return kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEventLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DiskLogPane::GetEventLabel (Disk2EventType type)
{
    const wchar_t  * label = L"?";



    switch (type)
    {
        case Disk2EventType::MotorCommandOn:   label = L"Motor command on";  break;
        case Disk2EventType::MotorEngaged:     label = L"Motor engaged";     break;
        case Disk2EventType::MotorCommandOff:  label = L"Motor command off"; break;
        case Disk2EventType::MotorDisengaged:  label = L"Motor disengaged";  break;
        case Disk2EventType::HeadStep:         label = L"Head step";         break;
        case Disk2EventType::HeadBump:         label = L"Head bump";         break;
        case Disk2EventType::AddrMark:         label = L"Address mark";      break;
        case Disk2EventType::DataRead:         label = L"Sector read";       break;
        case Disk2EventType::DataWrite:        label = L"Sector write";      break;
        case Disk2EventType::AddrChecksumError: label = L"Address checksum"; break;
        case Disk2EventType::DataChecksumError: label = L"Data checksum";    break;
        case Disk2EventType::DataCutOff:       label = L"Data cut off";      break;
        case Disk2EventType::DataNoAddress:    label = L"No address field";  break;
        case Disk2EventType::WriteProtected:   label = L"Write protected";   break;
        case Disk2EventType::DriveSelect:      label = L"Drive select";      break;
        case Disk2EventType::DiskInserted:     label = L"Disk inserted";     break;
        case Disk2EventType::DiskEjected:      label = L"Disk ejected";      break;
        case Disk2EventType::ReadStall:        label = L"Read stall";        break;
        default:                                                             break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDetail
//
//  A track, sector or volume not known yet is "?": a data field read before
//  any address field. A sector's detail ends with its first bytes.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskLogPane::GetDetail (const Disk2Event & event, const std::vector<Byte> * bytes)
{
    auto          field  = [] (wchar_t prefix, int value) { return (value < 0) ? std::format (L"{}?", prefix) : std::format (L"{}{}", prefix, value); };
    std::wstring  detail;
    std::wstring  first;
    size_t        i      = 0;



    for (i = 0; bytes != nullptr && i < bytes->size() && i < kDetailBytes; i++)
    {
        first += std::format (L" {:02X}", (*bytes)[i]);
    }

    if (bytes != nullptr && bytes->size() > kDetailBytes)
    {
        first += L" ...";
    }



    switch (event.type)
    {
        case Disk2EventType::HeadStep:
            detail = std::format (L"quarter-track {} -> {}", event.payload.step.prevQt, event.payload.step.newQt);
            break;

        case Disk2EventType::HeadBump:
            detail = std::format (L"at quarter-track {}", event.payload.bump.atQt);
            break;

        case Disk2EventType::AddrMark:
            detail = std::format (L"{} {} {}", field (L'T', event.payload.addrMark.track), field (L'S', event.payload.addrMark.sector),
                                  field (L'V', event.payload.addrMark.volume));
            break;

        case Disk2EventType::DataRead:
        case Disk2EventType::DataWrite:
        case Disk2EventType::DataNoAddress:
            detail = std::format (L"{} {} {} ({} bytes){}", field (L'T', event.payload.dataMark.track), field (L'S', event.payload.dataMark.sector),
                                  field (L'V', event.payload.dataMark.volume), event.payload.dataMark.byteCount, first);
            break;

        case Disk2EventType::DataChecksumError:
            detail = std::format (L"{} {} {}: the checksum does not match ({} bytes){}", field (L'T', event.payload.dataMark.track),
                                  field (L'S', event.payload.dataMark.sector), field (L'V', event.payload.dataMark.volume),
                                  event.payload.dataMark.byteCount, first);
            break;

        case Disk2EventType::AddrChecksumError:
            detail = std::format (L"{} {} {} as read: the checksum does not match", field (L'T', event.payload.addrMark.track),
                                  field (L'S', event.payload.addrMark.sector), field (L'V', event.payload.addrMark.volume));
            break;

        case Disk2EventType::DataCutOff:
            detail = std::format (L"after {} {} {}: no epilogue at either sector length", field (L'T', event.payload.addrMark.track),
                                  field (L'S', event.payload.addrMark.sector), field (L'V', event.payload.addrMark.volume));
            break;

        case Disk2EventType::WriteProtected:
            detail = L"writing turned on with a write-protected disk in the drive";
            break;

        case Disk2EventType::ReadStall:
            detail = std::format (L"no address field in two revolutions at quarter-track {}", event.payload.bump.atQt);
            break;

        default:
            break;
    }

    return detail;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetColumnSpecs
//
////////////////////////////////////////////////////////////////////////////////

DiskLogPane::ColumnSpecs DiskLogPane::GetColumnSpecs() const
{
    return { { L"Cycle",     DxuiTextHAlign::Right,  110, true  },
             { L"Drive",     DxuiTextHAlign::Right,   48, true  },
             { L"Severity",  DxuiTextHAlign::Left,  kFit, true  },
             { L"Event",     DxuiTextHAlign::Left,  kFit, true  },
             { L"Detail",    DxuiTextHAlign::Left,     0, true  },
             { L"Wall time", DxuiTextHAlign::Left,   100, false } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Matches
//
////////////////////////////////////////////////////////////////////////////////

bool DiskLogPane::Matches (
    const Row  & row,
    size_t       index) const
{
    const Disk2Event  & event   = row.event;
    Kind                kind    = GetKindOf (event.type);
    bool                isShown = ((kind == kNoKind) || m_kinds[kind]) && m_severities[GetSeverityOf (event.type)];
    int                 track   = -1;
    int                 sector  = -1;



    UNREFERENCED_PARAMETER (index);

    if (isShown && m_drive != 0)
    {
        isShown = event.drive == m_drive - 1;
    }

    switch (event.type)
    {
        case Disk2EventType::AddrMark:
        case Disk2EventType::AddrChecksumError:
        case Disk2EventType::DataCutOff:
            track  = event.payload.addrMark.track;
            sector = event.payload.addrMark.sector;
            break;

        case Disk2EventType::DataRead:
        case Disk2EventType::DataWrite:
        case Disk2EventType::DataChecksumError:
        case Disk2EventType::DataNoAddress:
            track  = event.payload.dataMark.track;
            sector = event.payload.dataMark.sector;
            break;

        case Disk2EventType::HeadStep:
            isShown = isShown && m_track.MatchesQuarterTrack (event.payload.step.newQt);
            break;

        case Disk2EventType::HeadBump:
        case Disk2EventType::ReadStall:
            isShown = isShown && m_track.MatchesQuarterTrack (event.payload.bump.atQt);
            break;

        default:
            break;
    }

    if (isShown && track >= 0)
    {
        isShown = m_track.Matches (track);
    }

    if (isShown && sector >= 0)
    {
        isShown = m_sector.Matches (sector);
    }

    return isShown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeCells
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogPane::MakeCells (
    const Row  & row,
    Cells      & out) const
{
    Severity  severity = GetSeverityOf (row.event.type);
    uint32_t  fill     = (severity == kWarning) ? m_warningArgb : (severity == kError) ? m_errorArgb : 0u;



    out[kDrive].text    = (row.event.drive >= 0) ? std::format (L"{}", row.event.drive + 1) : std::wstring();
    out[kSeverity].text = GetSeverityLabel (severity);
    out[kEvent].text    = GetEventLabel (row.event.type);
    out[kDetail].text   = GetDetail (row.event, row.data.get());
    out[kDetail].tip    = (row.data != nullptr) ? GetHexDump (*row.data) : std::wstring();

    for (DxuiListView::Cell & cell : out)
    {
        cell.background = fill;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSeverityOf
//
////////////////////////////////////////////////////////////////////////////////

DiskLogPane::Severity DiskLogPane::GetSeverityOf (Disk2EventType type)
{
    Severity  severity = kInfo;



    switch (type)
    {
        case Disk2EventType::AddrChecksumError:
        case Disk2EventType::DataChecksumError:  severity = kError;   break;
        case Disk2EventType::DataCutOff:
        case Disk2EventType::DataNoAddress:
        case Disk2EventType::WriteProtected:
        case Disk2EventType::ReadStall:          severity = kWarning; break;
        default:                                                      break;
    }

    return severity;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSeverityLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DiskLogPane::GetSeverityLabel (Severity severity)
{
    static constexpr const wchar_t * kLabels[kSeverityCount] = { L"Info", L"Warning", L"Error" };



    return (severity < kSeverityCount) ? kLabels[severity] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHexDump
//
//  kDumpBytes to a line: the offset, the bytes, then the bytes as Apple II
//  text with the high bit dropped, "." for a control character.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskLogPane::GetHexDump (const std::vector<Byte> & bytes)
{
    constexpr Byte  kHighBit    = 0x80;
    constexpr Byte  kFirstPrint = 0x20;
    constexpr Byte  kDelete     = 0x7F;
    std::wstring    dump;
    std::wstring    text;
    size_t          line        = 0;
    size_t          i           = 0;
    Byte            ch          = 0;



    for (line = 0; line < bytes.size(); line += kDumpBytes)
    {
        dump += std::format (L"{}{:02X}: ", dump.empty() ? L"" : L"\n", line);
        text.clear();

        for (i = line; i < line + kDumpBytes; i++)
        {
            if (i >= bytes.size())
            {
                dump += L"   ";
                continue;
            }

            ch    = (Byte) (bytes[i] & ~kHighBit);
            dump += std::format (L"{:02X} ", bytes[i]);
            text += (ch >= kFirstPrint && ch < kDelete) ? (wchar_t) ch : L'.';
        }

        dump += text;
    }

    return dump;
}
