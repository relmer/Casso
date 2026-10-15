#include "Pch.h"

#include "Debugger/DiagnosticsSnapshot.h"
#include "Devices/Tape/TapeDeck.h"
#include "Machines/Apple2/Common/CassetteDiagnostics.h"
#include "Machines/Apple2/Common/CassettePort.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteDiagnostics::GetDiagnostics
//
//  A tape's place and length are in seconds; with no tape in the deck,
//  the panel has neither row.
//
////////////////////////////////////////////////////////////////////////////////

void CassetteDiagnostics::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    const TapeDeck      & tapeDeck = m_machine.GetTapeDeck();
    const CassettePort  * jacks    = m_machine.GetRefs().cassettePort;
    TapeDeck::Snapshot    deck     = tapeDeck.GetSnapshot();
    DiagnosticsGroup      tape     { "Tape", {} };
    DiagnosticsGroup      port     { "Port", {} };
    double                rate     = (deck.sampleRate != 0) ? (double) deck.sampleRate : 1.0;



    if (jacks == nullptr)
    {
        return;
    }

    tape.rows.push_back (MakeTextRow ("Transport", GetTransportText (deck.transport)));

    if (deck.transport != TapeTransport::Empty)
    {
        tape.rows.push_back (MakeTextRow ("Position", FormatSeconds (deck.positionSamples / rate)));
        tape.rows.push_back (MakeTextRow ("Length",   FormatSeconds ((double) deck.lengthSamples / rate)));
    }

    tape.rows.push_back (MakeTextRow ("Last access", tapeDeck.HasBeenAccessed() ? std::format ("cycle {}", tapeDeck.GetLastAccessCycle()) : std::string ("none")));

    port.rows.push_back (MakeFlagRow ("Input level",    jacks->PeekInputLevel()));
    port.rows.push_back (MakeFlagRow ("Output level",   jacks->GetOutputLevel()));
    port.rows.push_back (MakeTextRow ("Output toggles", std::format ("{}", jacks->GetToggleCount())));

    snapshot.groups.push_back (std::move (tape));
    snapshot.groups.push_back (std::move (port));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteDiagnostics::GetTransportText
//
////////////////////////////////////////////////////////////////////////////////

const char * CassetteDiagnostics::GetTransportText (TapeTransport transport)
{
    switch (transport)
    {
    case TapeTransport::Stopped:        return "Stopped";
    case TapeTransport::Playing:        return "Playing";
    case TapeTransport::Recording:      return "Recording";
    case TapeTransport::FastForwarding: return "Fast forward";
    case TapeTransport::Rewinding:      return "Rewind";
    default:                            return "No tape";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteDiagnostics::FormatSeconds
//
//  Minutes and seconds to a tenth, "2:05.3".
//
////////////////////////////////////////////////////////////////////////////////

std::string CassetteDiagnostics::FormatSeconds (double seconds)
{
    constexpr int  kSecondsPerMinute = 60;
    int            tenths            = (int) std::lround (std::max (0.0, seconds) * kTenths);
    int            minutes           = tenths / (kSecondsPerMinute * kTenths);



    return std::format ("{}:{:02}.{}", minutes, (tenths / kTenths) % kSecondsPerMinute, tenths % kTenths);
}
