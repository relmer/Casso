#pragma once

#include "Pch.h"

#include "Debugger/IDiagnosticsProvider.h"

struct MachineRefs;
class TapeDeck;
enum class TapeTransport;





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteDiagnostics
//
//  The Cassette panel: the deck's transport, where the tape is and how long
//  it runs, the port's input level and output flip-flop, how many times the
//  output has toggled, and the cycle of the guest's last access to the
//  tape. The port is read from the machine's references on each call, so a
//  rebuilt machine is followed; a machine with no cassette port has no
//  panel (MachineClockPanel::GetProviders).
//
////////////////////////////////////////////////////////////////////////////////

class CassetteDiagnostics : public IDiagnosticsProvider
{
public:
    CassetteDiagnostics (const TapeDeck & deck, const MachineRefs & refs) : m_deck (deck), m_refs (refs) {}

    std::string  GetDiagnosticsId    () const override { return "cassette"; }
    std::string  GetDiagnosticsTitle () const override { return "Cassette"; }
    void         GetDiagnostics      (DiagnosticsSnapshot & snapshot) const override;

    static const char *  GetTransportText (TapeTransport transport);
    static std::string   FormatSeconds    (double seconds);

private:
    static constexpr int  kTenths = 10;

    const TapeDeck     & m_deck;
    const MachineRefs  & m_refs;
};
