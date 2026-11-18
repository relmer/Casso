#pragma once

#include "Pch.h"

#include "Disassembler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler65816
//
//  A linear walk over 65816 code, from the opcode map of WDC's W65C816S
//  datasheet. An immediate operand of an accumulator instruction is two
//  bytes while the M flag is clear and one while it is set, and of an index
//  register instruction likewise by the X flag. The walk starts with both
//  flags clear when `longRegisters` is set and both set otherwise; with
//  `followFlags`, a REP clears and a SEP sets the flags its operand has.
//
////////////////////////////////////////////////////////////////////////////////

class Disassembler65816
{
public:
    static void  Disassemble (std::span<const Byte>           bytes,
                              Word                            origin,
                              bool                            longRegisters,
                              bool                            followFlags,
                              std::vector<DisassembledLine> & outLines);

private:
    //  Each addressing mode is a letter in s_kModes.
    static constexpr char  kImplied          = 'i';
    static constexpr char  kAccumulator      = 'A';
    static constexpr char  kImmediateM       = 'M';
    static constexpr char  kImmediateX       = 'I';
    static constexpr char  kImmediate8       = '8';
    static constexpr char  kDirect           = 'd';
    static constexpr char  kDirectX          = 'x';
    static constexpr char  kDirectY          = 'y';
    static constexpr char  kDirectIndirect   = 'n';
    static constexpr char  kDirectXIndirect  = 'N';
    static constexpr char  kDirectIndirectY  = 'o';
    static constexpr char  kDirectLong       = 'l';
    static constexpr char  kDirectLongY      = 'L';
    static constexpr char  kAbsolute         = 'a';
    static constexpr char  kAbsoluteX        = 'X';
    static constexpr char  kAbsoluteY        = 'Y';
    static constexpr char  kLong             = 'f';
    static constexpr char  kLongX            = 'F';
    static constexpr char  kIndirect         = 'j';
    static constexpr char  kXIndirect        = 'J';
    static constexpr char  kIndirectLong     = 'k';
    static constexpr char  kStack            = 's';
    static constexpr char  kStackIndirectY   = 'S';
    static constexpr char  kRelative         = 'r';
    static constexpr char  kRelativeLong     = 'R';
    static constexpr char  kBlockMove        = 'm';

    static constexpr Byte  kRep   = 0xC2;
    static constexpr Byte  kSep   = 0xE2;
    static constexpr Byte  kFlagM = 0x20;
    static constexpr Byte  kFlagX = 0x10;

    //  Three letters an opcode, in opcode order, and each opcode's mode.
    static constexpr const char *  s_kMnemonics =
        "BRKORACOPORATSBORAASLORAPHPORAASLPHDTSBORAASLORA"
        "BPLORAORAORATRBORAASLORACLCORAINCTCSTRBORAASLORA"
        "JSRANDJSLANDBITANDROLANDPLPANDROLPLDBITANDROLAND"
        "BMIANDANDANDBITANDROLANDSECANDDECTSCBITANDROLAND"
        "RTIEORWDMEORMVPEORLSREORPHAEORLSRPHKJMPEORLSREOR"
        "BVCEOREOREORMVNEORLSREORCLIEORPHYTCDJMLEORLSREOR"
        "RTSADCPERADCSTZADCRORADCPLAADCRORRTLJMPADCRORADC"
        "BVSADCADCADCSTZADCRORADCSEIADCPLYTDCJMPADCRORADC"
        "BRASTABRLSTASTYSTASTXSTADEYBITTXAPHBSTYSTASTXSTA"
        "BCCSTASTASTASTYSTASTXSTATYASTATXSTXYSTZSTASTZSTA"
        "LDYLDALDXLDALDYLDALDXLDATAYLDATAXPLBLDYLDALDXLDA"
        "BCSLDALDALDALDYLDALDXLDACLVLDATSXTYXLDYLDALDXLDA"
        "CPYCMPREPCMPCPYCMPDECCMPINYCMPDEXWAICPYCMPDECCMP"
        "BNECMPCMPCMPPEICMPDECCMPCLDCMPPHXSTPJMLCMPDECCMP"
        "CPXSBCSEPSBCCPXSBCINCSBCINXSBCNOPXBACPXSBCINCSBC"
        "BEQSBCSBCSBCPEASBCINCSBCSEDSBCPLXXCEJSRSBCINCSBC";

    static constexpr const char *  s_kModes =
        "8N8sdddliMAiaaaf"
        "ronSdxxLiYAiaXXF"
        "aNfsdddliMAiaaaf"
        "ronSxxxLiYAiXXXF"
        "iN8smddliMAiaaaf"
        "ronSmxxLiYiifXXF"
        "iNRsdddliMAijaaf"
        "ronSxxxLiYiiJXXF"
        "rNRsdddliMiiaaaf"
        "ronSxxyLiYiiaXXF"
        "INIsdddliMiiaaaf"
        "ronSxxyLiYiiXXYF"
        "IN8sdddliMiiaaaf"
        "ronSnxxLiYiikXXF"
        "IN8sdddliMiiaaaf"
        "ronSaxxLiYiiJXXF";

    static size_t       GetOperandSize (char mode, bool wideM, bool wideX);
    static std::string  FormatOperand  (char mode, std::span<const Byte> operand, Word next);
};
