#pragma once

#include "GlobalAddressingModes.h"

class OpcodeTable;





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssemblyStatus
//
////////////////////////////////////////////////////////////////////////////////

enum class LineAssemblyStatus
{
    Ok,
    UnknownMnemonic,
    InvalidOperand,
    ModeNotAvailable,
    BranchOutOfRange,
};





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssembler
//
////////////////////////////////////////////////////////////////////////////////

class LineAssembler
{
public:
    explicit LineAssembler (const OpcodeTable & opcodes);

    LineAssemblyStatus  TryAssemble (Word                 address,
                                     const std::string  & line,
                                     std::vector<Byte>  & outBytes,
                                     std::string        & outError) const;

private:
    enum class OperandShape
    {
        None,
        Accumulator,
        Immediate,
        Plain,
        IndexedX,
        IndexedY,
        Indirect,
        IndirectX,
        IndirectY,
        BitBranch,
    };

    struct Operand
    {
        OperandShape  shape   = OperandShape::None;
        uint32_t      value   = 0;
        uint32_t      target  = 0;
        bool          isShort = false;
    };

    using ModeList = std::vector<GlobalAddressingMode::AddressingMode>;

    static void  SplitLine         (const std::string & line, std::string & mnemonic, std::string & operandText);
    static bool  TryParseHex       (const std::string & text, uint32_t & value, bool & isShort);
    static bool  TryParseOperand   (const std::string & text, Operand & operand);
    static bool  TryParseEnclosed  (const std::string & text, Operand & operand);
    static void  GetCandidateModes (const Operand & operand, ModeList & modes);
    static bool  TryEncodeOperand  (Word                                   address,
                                    GlobalAddressingMode::AddressingMode   mode,
                                    Byte                                   operandSize,
                                    const Operand                        & operand,
                                    std::vector<Byte>                    & outBytes);
    static void  SetFailure        (LineAssemblyStatus  & status,
                                    LineAssemblyStatus    value,
                                    std::string         & outError,
                                    const std::string   & message);

    const OpcodeTable & m_opcodes;
};
