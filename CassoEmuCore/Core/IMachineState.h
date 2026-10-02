#pragma once

#include "Pch.h"

class StateReader;
class StateWriter;





////////////////////////////////////////////////////////////////////////////////
//
//  IMachineState
//
//  Save and restore of one stateful part of the machine: the CPU, the bus, a
//  soft-switch bank, a card. A whole-machine snapshot is the concatenation of
//  every part's blob in a fixed order; restoring reads them back in the same
//  order.
//
//  Every implementation follows the same pattern:
//
//      static constexpr uint32_t kStateTag     = IMachineState::MakeTag ('V', 'I', 'A', ' ');
//      static constexpr uint16_t kStateVersion = 1;
//
//      HRESULT Via6522::SaveState (StateWriter & writer) const
//      {
//          writer.BeginSection (kStateTag, kStateVersion);
//          writer.WriteByte    (m_ifr);
//          ...
//          return writer.EndSection();
//      }
//
//      HRESULT Via6522::LoadState (StateReader & reader)
//      {
//          HRESULT  hr      = S_OK;
//          uint16_t version = 0;
//
//          hr = reader.BeginSection (kStateTag, kStateVersion, version);
//          CHR (hr);
//
//          reader.ReadByte (m_ifr);
//          ...
//
//          hr = reader.EndSection();
//          CHR (hr);
//
//          ReDeriveCaches();       // only what depends on the fields read
//
//      Error:
//          return hr;
//      }
//
//  Rules:
//
//  - Reads do not return errors. The reader's error is sticky: the first
//    failure zeroes that read and every later one, and EndSection reports it.
//    Check BeginSection and EndSection. A value read that indexes or sizes
//    anything (a bank number, a track count) is range-checked before use.
//  - Write and read the same fields in the same order. The round-trip test
//    for a part sets every field to a distinct nonzero value, saves, loads
//    into a fresh object, and compares every field; add each new field to it.
//  - One section per object, with a tag of its own and a version. A reader
//    meeting another tag, a newer version, or a section whose size does not
//    match what it consumed fails with an error rather than loading garbage.
//  - Raise kStateVersion whenever the field list changes. LoadState may accept
//    older versions by branching on the version it read; a section newer than
//    the reader supports is refused.
//  - Save the state the machine computes with, not caches derived from it.
//    Page tables, the active video mode object, and the like are re-derived at
//    the end of LoadState from the flags just loaded.
//  - Never save a pointer. Save which one as an index or a flag and resolve it
//    on load.
//  - Wiring is not state: the device set, slot assignments, IRQ source tokens
//    and host sinks are rebuilt by the machine builder before a load. Anything
//    that has to agree with that wiring (a source count, a buffer size) is
//    saved and checked on load, failing with ERROR_INVALID_DATA on mismatch.
//  - Debugger configuration (watchpoints, the trace ring, opcode watches) is
//    not machine state and is never saved.
//  - Values are little-endian and fixed width regardless of the in-memory type;
//    the stream classes do the conversion. A bool is one byte, 0 or 1.
//  - A failed LoadState may leave the object partly loaded. The caller treats
//    the machine as unusable and reloads a known-good snapshot.
//  - A part that holds another stateful part (a card with two VIAs) calls the
//    inner part's SaveState / LoadState inside its own section; sections nest.
//
////////////////////////////////////////////////////////////////////////////////

class IMachineState
{
public:
    virtual          ~IMachineState() = default;

    virtual HRESULT  SaveState (StateWriter & writer) const = 0;
    virtual HRESULT  LoadState (StateReader & reader)       = 0;

    // Four characters packed into a section tag, first character in the low
    // byte, so the tag reads as text in a hex dump of the stream.
    static constexpr uint32_t MakeTag (char a, char b, char c, char d)
    {
        return  static_cast<uint32_t> (static_cast<Byte> (a))
             | (static_cast<uint32_t> (static_cast<Byte> (b)) << (CHAR_BIT * 1))
             | (static_cast<uint32_t> (static_cast<Byte> (c)) << (CHAR_BIT * 2))
             | (static_cast<uint32_t> (static_cast<Byte> (d)) << (CHAR_BIT * 3));
    }
};
