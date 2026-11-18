#include "Pch.h"

#include "CassoLink/LinkFault.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFault::Parse
//
//  Empty entries are skipped, so a list may end with a semicolon.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkFault::Parse (const std::string & text, std::vector<LinkFault> & outFaults)
{
    HRESULT      hr    = S_OK;
    size_t       start = 0;
    size_t       end   = 0;
    std::string  entry;
    LinkFault    fault;



    outFaults.clear();

    while (start <= text.size())
    {
        end   = text.find (';', start);
        end   = (end == std::string::npos) ? text.size() : end;
        entry = text.substr (start, end - start);
        start = end + 1;

        std::erase_if (entry, [] (char ch) { return ch == ' ' || ch == '\t'; });

        if (entry.empty())
        {
            continue;
        }

        hr = ParseOne (entry, fault);
        CHR (hr);

        outFaults.push_back (fault);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFault::ParseOne
//
//  source:kind@where, with :quarters after a stall's offset and #request
//  after an Apple byte's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkFault::ParseOne (const std::string & text, LinkFault & outFault)
{
    HRESULT      hr       = S_OK;
    std::string  lower    = text;
    size_t       colon    = 0;
    size_t       at       = 0;
    std::string  source;
    std::string  kind;
    std::string  where;
    bool         allowed  = false;



    outFault = LinkFault();

    std::transform (lower.begin(), lower.end(), lower.begin(), [] (char ch) { return (char) tolower ((unsigned char) ch); });

    colon = lower.find (':');
    at    = lower.find ('@');
    CBREx (colon != std::string::npos && at != std::string::npos && colon < at, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    source = lower.substr (0, colon);
    kind   = lower.substr (colon + 1, at - colon - 1);
    where  = lower.substr (at + 1);

    if      (source == "pc")        { outFault.source = Source::Pc;        }
    else if (source == "apple")     { outFault.source = Source::Apple;     }
    else if (source == "bootstrap") { outFault.source = Source::Bootstrap; }
    else if (source == "cassette")  { outFault.source = Source::Cassette;  }
    else                            { CBREx (false, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA)); }

    if      (kind == "flip")       { outFault.kind = Kind::Flip;       }
    else if (kind == "drop")       { outFault.kind = Kind::Drop;       }
    else if (kind == "stall")      { outFault.kind = Kind::Stall;      }
    else if (kind == "unreadable") { outFault.kind = Kind::Unreadable; }
    else if (kind == "baddata")    { outFault.kind = Kind::BadData;    }
    else if (kind == "badcrc")     { outFault.kind = Kind::BadCrc;     }
    else                           { CBREx (false, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA)); }

    //  A sector is the Apple's alone, and the cassette's bytes are flipped or
    //  left out, since a stall in its audio is only a longer gap.
    allowed = outFault.IsSectorFault() ? outFault.source == Source::Apple
            : outFault.kind == Kind::Stall ? outFault.source != Source::Cassette
            : true;
    CBREx (allowed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = outFault.IsSectorFault() ? ParseSector (where, outFault) : ParseByte (where, outFault);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFault::ParseSector
//
//  T, the track, S and the sector, as T17S3.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkFault::ParseSector (const std::string & where, LinkFault & inOutFault)
{
    HRESULT   hr     = S_OK;
    size_t    mark   = where.find ('s');
    bool      formed = where.size() > 1 && where[0] == 't' && mark != std::string::npos;
    uint32_t  value  = 0;
    bool      parsed = false;



    CBREx (formed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    parsed = TryParseNumber (where.substr (1, mark - 1), kLastTrack, value);
    CBREx (parsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    inOutFault.track = (Byte) value;

    parsed = TryParseNumber (where.substr (mark + 1), kLastSector, value);
    CBREx (parsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    inOutFault.sector = (Byte) value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFault::ParseByte
//
//  The offset, then :quarters for a stall and #request for the Apple's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkFault::ParseByte (const std::string & text, LinkFault & inOutFault)
{
    HRESULT      hr     = S_OK;
    std::string  where  = text;
    std::string  quarters;
    std::string  request;
    size_t       mark   = where.find ('#');
    uint32_t     value  = 0;
    bool         parsed = false;



    if (mark != std::string::npos)
    {
        CBREx (inOutFault.source == Source::Apple, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        request = where.substr (mark + 1);
        where   = where.substr (0, mark);

        parsed = TryParseNumber (request, 255, value);
        CBREx (parsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        inOutFault.request = (int) value;
    }

    mark = where.find (':');

    if (mark != std::string::npos)
    {
        CBREx (inOutFault.kind == Kind::Stall, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        quarters = where.substr (mark + 1);
        where    = where.substr (0, mark);

        parsed = TryParseNumber (quarters, 255, value);
        CBREx (parsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        inOutFault.quarters = (Byte) value;
    }
    else if (inOutFault.kind == Kind::Stall)
    {
        inOutFault.quarters = kDefaultQuarters;
    }

    //  The Apple counts its bytes in 24 bits.
    parsed = TryParseNumber (where, inOutFault.source == Source::Apple ? 0xFFFFFF : UINT32_MAX, value);
    CBREx (parsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    inOutFault.offset = value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFault::TryParseNumber
//
//  Decimal, or hexadecimal after $ or 0x.
//
////////////////////////////////////////////////////////////////////////////////

bool LinkFault::TryParseNumber (const std::string & text, uint32_t limit, uint32_t & outValue)
{
    std::string         digits = text;
    int                 base   = 10;
    unsigned long long  value  = 0;
    size_t              used   = 0;



    outValue = 0;

    if (digits.starts_with ("$"))
    {
        digits = digits.substr (1);
        base   = 16;
    }
    else if (digits.starts_with ("0x"))
    {
        digits = digits.substr (2);
        base   = 16;
    }

    if (digits.empty() || !std::all_of (digits.begin(), digits.end(), [base] (char ch) { return base == 16 ? isxdigit ((unsigned char) ch) != 0 : isdigit ((unsigned char) ch) != 0; }))
    {
        return false;
    }

    try
    {
        value = std::stoull (digits, &used, base);
    }
    catch (...)
    {
        return false;
    }

    if (used != digits.size() || value > limit)
    {
        return false;
    }

    outValue = (uint32_t) value;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFault::Describe
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkFault::Describe (const LinkFault & fault)
{
    std::string  what;
    std::string  change;



    switch (fault.kind)
    {
        case Kind::Flip:       change = "its low bit flipped";                                          break;
        case Kind::Drop:       change = "left out";                                                     break;
        case Kind::Stall:      change = std::format ("sent after {} ms", fault.quarters * kQuarterMs);   break;
        case Kind::Unreadable: change = "reported as not read";                                         break;
        case Kind::BadData:    change = "sent with its data changed and a CRC that matches";             break;
        case Kind::BadCrc:     change = "given a wrong CRC in Read CRCs";                               break;
    }

    if (fault.IsSectorFault())
    {
        return std::format ("track {}, sector {}, {}", fault.track, fault.sector, change);
    }

    switch (fault.source)
    {
        case Source::Pc:        what = std::format ("the PC's byte {}", fault.offset);                                              break;
        case Source::Apple:     what = std::format ("byte {} of the Apple's reply to request {}", fault.offset, fault.request + 1);  break;
        case Source::Bootstrap: what = std::format ("the serial bootstrap's byte {}", fault.offset);                                break;
        case Source::Cassette:  what = std::format ("the cassette data's byte {}", fault.offset);                                   break;
    }

    return what + ", " + change;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFault::MakePayload
//
//  The Fault command's payload: the kind, then the byte's offset in three
//  bytes, least significant first, or the track and the sector, then the
//  stall in quarter seconds.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> LinkFault::MakePayload (const LinkFault & fault)
{
    if (fault.IsSectorFault())
    {
        return { (Byte) fault.kind, fault.track, fault.sector, 0, 0 };
    }

    return { (Byte) fault.kind, (Byte) (fault.offset & 0xFF), (Byte) ((fault.offset >> 8) & 0xFF), (Byte) ((fault.offset >> 16) & 0xFF), fault.quarters };
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults::GetState
//
////////////////////////////////////////////////////////////////////////////////

LinkFaults::State & LinkFaults::GetState()
{
    static State  s_state;



    return s_state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults::Enable
//
////////////////////////////////////////////////////////////////////////////////

void LinkFaults::Enable()
{
    State &                      state = GetState();
    std::lock_guard<std::mutex>  lock (state.lock);



    state.enabled = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults::Disable
//
////////////////////////////////////////////////////////////////////////////////

void LinkFaults::Disable()
{
    State &                      state = GetState();
    std::lock_guard<std::mutex>  lock (state.lock);



    state.enabled = false;
    state.armed.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults::IsEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool LinkFaults::IsEnabled()
{
    State &                      state = GetState();
    std::lock_guard<std::mutex>  lock (state.lock);



    return state.enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults::Arm
//
////////////////////////////////////////////////////////////////////////////////

void LinkFaults::Arm (const std::vector<LinkFault> & faults)
{
    State &                      state = GetState();
    std::lock_guard<std::mutex>  lock (state.lock);



    state.armed.insert (state.armed.end(), faults.begin(), faults.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults::TryTake
//
//  The first armed fault from one of the sources, removed from the list.
//  Nothing is taken until Enable is called.
//
////////////////////////////////////////////////////////////////////////////////

bool LinkFaults::TryTake (std::initializer_list<LinkFault::Source> sources, LinkFault & outFault)
{
    State &                      state = GetState();
    std::lock_guard<std::mutex>  lock (state.lock);
    auto                         found = state.armed.end();



    if (!state.enabled)
    {
        return false;
    }

    found = std::find_if (state.armed.begin(), state.armed.end(), [&sources] (const LinkFault & fault)
    {
        return std::find (sources.begin(), sources.end(), fault.source) != sources.end();
    });

    if (found == state.armed.end())
    {
        return false;
    }

    outFault = *found;
    state.armed.erase (found);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults::Clear
//
////////////////////////////////////////////////////////////////////////////////

void LinkFaults::Clear()
{
    State &                      state = GetState();
    std::lock_guard<std::mutex>  lock (state.lock);



    state.armed.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults::SetReporter
//
////////////////////////////////////////////////////////////////////////////////

void LinkFaults::SetReporter (ReportFn report)
{
    State &                      state = GetState();
    std::lock_guard<std::mutex>  lock (state.lock);



    state.report = std::move (report);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults::Report
//
////////////////////////////////////////////////////////////////////////////////

void LinkFaults::Report (const std::string & text)
{
    State &   state  = GetState();
    ReportFn  report;



    {
        std::lock_guard<std::mutex>  lock (state.lock);

        report = state.report;
    }

    if (report)
    {
        report (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkByteFault::Arm
//
////////////////////////////////////////////////////////////////////////////////

void LinkByteFault::Arm (const LinkFault & fault)
{
    m_fault = fault;
    m_armed = true;
    m_sent  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkByteFault::Apply
//
////////////////////////////////////////////////////////////////////////////////

void LinkByteFault::Apply (std::span<const Byte> data, std::vector<Byte> & outBytes, size_t & outStallAt, DWORD & outStallMs, std::string & outReport)
{
    size_t  index = 0;



    outBytes.assign (data.begin(), data.end());
    outStallAt = kNoStall;
    outStallMs = 0;
    outReport.clear();

    if (!m_armed || m_fault.offset >= m_sent + data.size())
    {
        m_sent += data.size();
        return;
    }

    index    = (size_t) (m_fault.offset - m_sent);
    m_sent  += data.size();
    m_armed  = false;

    switch (m_fault.kind)
    {
        case LinkFault::Kind::Flip:
            outBytes[index] ^= 0x01;
            break;

        case LinkFault::Kind::Drop:
            outBytes.erase (outBytes.begin() + (ptrdiff_t) index);
            break;

        case LinkFault::Kind::Stall:
            outStallAt = index;
            outStallMs = m_fault.quarters * LinkFault::kQuarterMs;
            break;

        default:
            break;
    }

    outReport = "Injected a fault: " + LinkFault::Describe (m_fault);
}
