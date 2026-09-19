#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTable
//
//  The counters behind PROFILE: per opcode the count and the base cycles, the
//  three avoidable penalties by kind, the cycles each address consumed, and
//  the total. Each penalty is one cycle the CPU already counted in the
//  instruction's cost, so the base is the cost less one cycle per penalty bit.
//
//  Record counts only while the table is on; a table that is off ignores
//  every instruction it is handed.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileTable
{
public:
    struct OpcodeCounts
    {
        uint64_t  count  = 0;
        uint64_t  cycles = 0;
    };

    struct PenaltyCycles
    {
        uint64_t  pageCross   = 0;
        uint64_t  branchTaken = 0;
        uint64_t  branchCross = 0;
    };

    static constexpr size_t  kOpcodeCount = 256;

    void  SetOn  (bool isOn)  { m_isOn = isOn; }
    bool  IsOn   () const     { return m_isOn; }

    void  Record (Word pc, Byte opcode, Byte cycles, Byte penalties);
    void  Reset  ();

    const OpcodeCounts                       & GetOpcode           (Byte opcode) const { return m_byOpcode[opcode]; }
    const PenaltyCycles                      & GetPenalties        () const            { return m_penalties; }
    const std::unordered_map<Word, uint64_t> & GetByAddress        () const            { return m_byAddress; }
    uint64_t                                   GetTotalCycles      () const            { return m_totalCycles; }
    uint64_t                                   GetInstructionCount () const            { return m_instructions; }

private:
    std::array<OpcodeCounts, kOpcodeCount>  m_byOpcode     = {};
    PenaltyCycles                           m_penalties;
    std::unordered_map<Word, uint64_t>      m_byAddress;
    uint64_t                                m_totalCycles  = 0;
    uint64_t                                m_instructions = 0;
    bool                                    m_isOn         = false;
};
