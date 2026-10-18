#pragma once

#include "Cpu6502.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayCpu
//
//  A 6502 for running a routine of the guest's ROM without touching the
//  machine: a read returns the byte the run wrote there, or else the peek's
//  byte, and a write lands in the overlay alone. Applesoft's own FOUT runs on
//  it, so a variable's value is written exactly as PRINT writes it.
//
////////////////////////////////////////////////////////////////////////////////

class OverlayCpu : public Cpu6502
{
public:
    using Peek = std::function<Byte (Word)>;

    explicit OverlayCpu (Peek peek) : m_peek (std::move (peek)) {}

    //  Sets a byte the run starts with, over the machine's.
    void  Poke (Word address, Byte value) { m_overlay[address] = value; }

    //  Calls the routine at address with an empty stack and runs until it
    //  returns, false when it takes more than maxSteps instructions.
    bool  TryCall (Word address, uint32_t maxSteps);

    Byte  ReadByteSlow (Word address) override;
    Word  ReadWord     (Word address) override;
    void  WriteByte    (Word address, Byte value) override;
    void  WriteWord    (Word address, Word value) override;

private:
    static constexpr Word  kSentinel = 0x0000;     // the return address the call is pushed with, plus one

    Peek                            m_peek;
    std::unordered_map<Word, Byte>  m_overlay;
};
