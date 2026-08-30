#include "Pch.h"

#include "AssemblySession.h"
#include "DialectProfile.h"
#include "DialectRegistry.h"
#include "ExpressionEvaluator.h"
#include "Parser.h"
#include "StringEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ToUpperCase
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::ToUpperCase (const std::string & text)
{
    std::string  upper = text;



    for (char & c : upper)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return upper;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::StripCommentAndTrim
//
//  The code part of a line: indentation removed, anything from the first ';'
//  dropped, trailing blanks removed. Deliberately naive about ';' inside a
//  string literal, which is what the two callers already assumed.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::StripCommentAndTrim (const std::string & text)
{
    std::string  code    = text;
    size_t       comment = 0;
    size_t       end     = 0;
    size_t       start   = code.find_first_not_of (" \t");



    if (start != std::string::npos)
    {
        code = code.substr (start);
    }

    comment = code.find (';');

    if (comment != std::string::npos)
    {
        code = code.substr (0, comment);
    }

    end = code.find_last_not_of (" \t");

    if (end != std::string::npos)
    {
        code = code.substr (0, end + 1);
    }

    return code;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::GetLeadingWord
//
//  The first whitespace-delimited word, ignoring any indentation before it.
//  Returns an empty string for text that is blank or whitespace only.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::GetLeadingWord (const std::string & text)
{
    std::string  word;
    size_t       end   = 0;
    size_t       start = text.find_first_not_of (" \t");



    if (start != std::string::npos)
    {
        end  = text.find_first_of (" \t", start);
        word = (end == std::string::npos) ? text.substr (start) : text.substr (start, end - start);
    }

    return word;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLowerExtension
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::GetLowerExtension (const std::string & filename)
{
    size_t       dot = filename.rfind ('.');
    std::string  ext;



    // No dot at all yields the empty string, same as a name ending in one.
    if (dot != std::string::npos)
    {
        ext = filename.substr (dot);

        for (auto & c : ext)
        {
            c = (char) std::tolower ((unsigned char) c);
        }
    }

    return ext;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HexCharToNibble
//
////////////////////////////////////////////////////////////////////////////////

int AssemblySession::HexCharToNibble (char c)
{
    int  nibble = -1;      // -1 == not a hex digit



    if      (c >= '0' && c <= '9') { nibble = c - '0';      }
    else if (c >= 'A' && c <= 'F') { nibble = c - 'A' + 10; }
    else if (c >= 'a' && c <= 'f') { nibble = c - 'a' + 10; }

    return nibble;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HexByte
//
////////////////////////////////////////////////////////////////////////////////

int AssemblySession::HexByte (const std::string & s, size_t offset)
{
    bool  hasPair = (offset + 1 < s.size());
    int   hi      = hasPair ? HexCharToNibble (s[offset])     : -1;
    int   lo      = hasPair ? HexCharToNibble (s[offset + 1]) : -1;
    int   value   = -1;      // -1 == not two hex digits at `offset`



    if (hi >= 0 && lo >= 0)
    {
        value = (hi << 4) | lo;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseSRecord
//
//  Motorola S-record text -> the data bytes it carries, address records
//  and all other record types discarded. Only S1/S2/S3 hold data; they
//  differ solely in address width (2/3/4 bytes), which is why the type
//  digit maps to a byte count rather than to separate parsers.
//
//  Deliberately lenient: every malformed line is skipped rather than
//  failing the file. This feeds .incbin-style payload loading, where a
//  stray banner or a truncated final line should not lose the megabyte
//  that parsed cleanly -- and a record's own byte count is the authority
//  on its length, so a bad line cannot desynchronize the ones after it.
//  The checksum byte is counted out of the data length but not verified.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> AssemblySession::ParseSRecord (const std::string & content)
{
    std::vector<Byte>  data;
    std::string        line;
    std::istringstream stream (content);



    while (std::getline (stream, line))
    {
        char    recType    = 0;
        int     addrBytes  = 0;
        int     byteCount  = 0;
        int     dataBytes  = 0;
        size_t  dataOffset = 0;

        // Trim trailing CR
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.size() < 2 || line[0] != 'S')
        {
            continue;
        }

        recType = line[1];

        // S1: 2-byte address, S2: 3-byte address, S3: 4-byte address

        if (recType == '1')      addrBytes = 2;
        else if (recType == '2') addrBytes = 3;
        else if (recType == '3') addrBytes = 4;
        else                     continue;

        if (line.size() < 4)
        {
            continue;
        }

        byteCount = HexByte (line, 2);

        if (byteCount < 0)
        {
            continue;
        }

        // Data bytes = byteCount - address bytes - 1 checksum byte
        dataBytes = byteCount - addrBytes - 1;

        if (dataBytes <= 0)
        {
            continue;
        }

        // Data starts after "Sn" + 2-char count + address hex chars
        dataOffset = 4 + (size_t) addrBytes * 2;

        for (int i = 0; i < dataBytes; i++)
        {
            int b = HexByte (line, dataOffset + (size_t) i * 2);

            if (b >= 0)
            {
                data.push_back ((Byte) b);
            }
        }
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseIntelHex
//
//  Intel HEX text -> the data bytes it carries. Only type-00 records hold
//  data; end-of-file (01) and the segment / linear address records (02-05)
//  are skipped, so the result is a flat byte stream with the addressing
//  discarded -- the same contract as ParseSRecord above.
//
//  The 11-character floor is the shortest possible well-formed record:
//  ':' + 2 count + 4 address + 2 type + 2 checksum, i.e. a zero-length one.
//  Anything shorter cannot be read without running off the end.
//
//  Lenient for the same reason as ParseSRecord: a malformed line is skipped,
//  not fatal, and each record's own count bounds it so one bad line cannot
//  desynchronize the rest. The checksum is not verified.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> AssemblySession::ParseIntelHex (const std::string & content)
{
    std::vector<Byte>  data;
    std::string        line;
    std::istringstream stream (content);



    while (std::getline (stream, line))
    {
        int     byteCount  = 0;
        int     recordType = 0;
        size_t  dataOffset = 0;

        // Trim trailing CR
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.empty() || line[0] != ':')
        {
            continue;
        }

        if (line.size() < 11)
        {
            continue;
        }

        byteCount = HexByte (line, 1);
        recordType = HexByte (line, 7);

        if (byteCount < 0 || recordType < 0)
        {
            continue;
        }

        // Only process data records (type 00)
        if (recordType != 0x00)
        {
            continue;
        }

        dataOffset = 9;

        for (int i = 0; i < byteCount; i++)
        {
            int b = HexByte (line, dataOffset + (size_t) i * 2);

            if (b >= 0)
            {
                data.push_back ((Byte) b);
            }
        }
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GenerateByteDirectives
//
//  Turns a binary payload into `.byte $xx,$xx,...` source lines, which is how
//  an included .bin / S-record / Intel-HEX file re-enters assembly: as
//  ordinary source, so nothing downstream needs a binary path at all.
//
//  16 per line keeps the listing readable and bounds line length; the split
//  point is arbitrary to the assembler, since the lines are re-parsed and
//  concatenated into one byte stream regardless.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> AssemblySession::GenerateByteDirectives (const std::vector<Byte> & data)
{
    std::vector<std::string> lines;



    static const int kBytesPerLine = 16;



    for (size_t i = 0; i < data.size(); i += kBytesPerLine)
    {
        size_t  end = 0;

        std::string line = "    .byte ";

        end = std::min (i + kBytesPerLine, data.size());

        for (size_t j = i; j < end; j++)
        {
            if (j > i)
            {
                line += ",";
            }

            line += std::format ("${:02X}", data[j]);
        }

        lines.push_back (line);
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBranchMnemonic
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsBranchMnemonic (const std::string & mnemonic) const
{
    return m_opcodeTable->HasMode (mnemonic, GlobalAddressingMode::Relative);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBitOpMnemonic — bare Rockwell bit-op mnemonic (as65 operand form)
//
//  RMB/SMB/BBR/BBS in their bare, bit-as-operand spelling. The opcode table keys
//  these per bit (RMB0..RMB7), so the bare names are not IsMnemonic()-recognized;
//  callers that decide "mnemonic vs. label" must special-case them, and
//  NormalizeBitOp folds the bit operand back into the suffixed mnemonic.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsBitOpMnemonic (const std::string & mnemonic)
{
    bool  found = false;



    // A dialect fact, not a CPU one, which is why it cannot be answered from
    // the opcode table the way IsBranchMnemonic now is: the table holds
    // RMB0..RMB7, and these bare names exist only because as65 spells the bit
    // as an operand. A second dialect supplies a different list here.
    static constexpr std::string_view  s_kBareBitOps[] = { "RMB", "SMB", "BBR", "BBS" };




    for (std::string_view name : s_kBareBitOps)
    {
        if (mnemonic == name)
        {
            found = true;
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::GetAddressingRules
//
//  The syntax -> mode policy, whole, in one place. Every case of the switch
//  this replaced had the same shape once the noise came off: try a short list
//  of candidate modes in priority order, take the first the mnemonic actually
//  carries, and otherwise fall back to what the syntax means on its own.
//
//  Two things the old form hid. `IsBranchMnemonic` was a separate concept only
//  because it predated the opcode table answering it -- it is now exactly the
//  ungated Relative candidate, so Bare's three-way decision is a plain list.
//  And each case built an OpcodeEntry it never read, because TryLookup was being
//  used as an existence test; that is HasMode.
//
//  Indexed by OperandSyntax, so the rows must stay in enum order. The `syntax`
//  field is what catches it if they do not.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const AssemblySession::AddressingRule> AssemblySession::GetAddressingRules()
{
    using AM = GlobalAddressingMode::AddressingMode;



    // `expr` alone: a branch target, a jump target, then zero page if it fits.
    static constexpr ModeCandidate  s_kBare[] =
    {
        { AM::Relative,          false },
        { AM::JumpAbsolute,      false },
        { AM::ZeroPage,          true  },
    };

    static constexpr ModeCandidate  s_kIndexedX[] = { { AM::ZeroPageX, true } };
    static constexpr ModeCandidate  s_kIndexedY[] = { { AM::ZeroPageY, true } };

    // (expr,X): the common (zp,X), or the 65C02 (abs,X) that only JMP carries.
    static constexpr ModeCandidate  s_kIndirectX[] =
    {
        { AM::ZeroPageXIndirect, true  },
        { AM::AbsoluteXIndirect, false },
    };

    // (expr): the 65C02 (zp) indirect when it fits and the mnemonic has it,
    // else the (abs) JMP indirect -- NMOS, or the page-fixed CMOS variant that
    // carries its own mode.
    static constexpr ModeCandidate  s_kIndirect[] =
    {
        { AM::ZeroPageIndirect,  true  },
        { AM::JumpIndirect,      false },
        { AM::JumpIndirectCmos,  false },
    };

    static constexpr AddressingRule  s_kRules[] =
    {
        { OperandSyntax::None,             {},             AM::SingleByteNoOperand },
        { OperandSyntax::Immediate,        {},             AM::Immediate           },
        { OperandSyntax::Bare,             s_kBare,        AM::Absolute            },
        { OperandSyntax::IndexedX,         s_kIndexedX,    AM::AbsoluteX           },
        { OperandSyntax::IndexedY,         s_kIndexedY,    AM::AbsoluteY           },
        { OperandSyntax::IndirectX,        s_kIndirectX,   AM::ZeroPageXIndirect   },
        { OperandSyntax::IndirectY,        {},             AM::ZeroPageIndirectY   },
        { OperandSyntax::Indirect,         s_kIndirect,    AM::JumpIndirect        },
        { OperandSyntax::Accumulator,      {},             AM::Accumulator         },
        // BBRn/BBSn only; a mnemonic lacking the mode fails the caller's lookup.
        { OperandSyntax::ZeroPageRelative, {},             AM::ZeroPageRelative    },
    };

    static_assert (std::size (s_kRules) == (size_t) OperandSyntax::Count,
                   "every OperandSyntax needs an addressing rule");

    return std::span<const AddressingRule> (s_kRules, std::size (s_kRules));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveAddressingMode — derive final 6502 addressing mode from syntax,
//  mnemonic, and resolved value
//
////////////////////////////////////////////////////////////////////////////////

GlobalAddressingMode::AddressingMode AssemblySession::ResolveAddressingMode (
    OperandSyntax       syntax,
    const std::string & mnemonic,
    int32_t             value,
    bool                resolved) const
{
    const AddressingRule &                rule       = GetAddressingRules()[(size_t) syntax];
    bool                                  fitsInPage = resolved && value >= 0 && value <= 0xFF;
    GlobalAddressingMode::AddressingMode  mode       = rule.fallback;



    ASSERT (rule.syntax == syntax);



    for (const ModeCandidate & candidate : rule.candidates)
    {
        if (candidate.needsZeroPage && !fitsInPage)
        {
            continue;
        }

        if (m_opcodeTable->HasMode (mnemonic, candidate.mode))
        {
            mode = candidate.mode;
            break;
        }
    }

    // A dialect that lets the accumulator go unnamed resolves there when the
    // mnemonic has no implied form at all. Which mnemonics those are is the
    // opcode table's answer, not the profile's -- a second list would be a
    // second list to get wrong -- and a dialect that requires `LSR A` reaches
    // none of this, so a bare LSR stays the missing operand it has always been.
    //
    // The implied-mode test is uncovered and cannot be covered: no mnemonic in
    // either table carries an implied AND an accumulator encoding, so removing
    // it changes nothing anything can observe. Recorded here rather than
    // dropped, because it is what keeps the precedence right by construction
    // instead of by the tables happening not to overlap.
    if ((syntax == OperandSyntax::None) &&
        (m_dialect.GetOperandlessForm() == OperandlessForm::ImpliedOrAccumulator) &&
        !m_opcodeTable->HasMode (mnemonic, GlobalAddressingMode::SingleByteNoOperand) &&
        m_opcodeTable->HasMode (mnemonic, GlobalAddressingMode::Accumulator))
    {
        mode = GlobalAddressingMode::Accumulator;
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CanReplaceJumpWithBranch
//
//  Whether `JMP addr` may be emitted as `BRA addr`, which is the only
//  optimization as65 performs on the code it writes.
//
//  Gated on the DIALECT first. Merlin's assembler does not do this and must not
//  acquire it by sharing an engine with as65, so the profile answers before any
//  of the four conditions is asked.
//
//  Four conditions, all of them as65's:
//
//    * The extended instruction set is active. Asked of the OPCODE TABLE rather
//      than of a flag, because the question is whether BRA can be encoded at
//      all, and the table is what answers that.
//    * Optimization is on. It is on by default; NOOPT turns it off, OPT turns
//      it back on, and the command line's -n outranks both.
//    * The target is ALREADY DEFINED. This is the load-bearing one. Because the
//      address is known while pass 1 is sizing the line, the two-versus-three
//      byte choice is made with complete information and no relaxation pass is
//      needed. A forward reference is never optimized -- not because it could
//      not be, but because as65 does not, and matching its bytes is the point.
//    * The displacement reaches. A branch carries one signed byte, measured
//      from the address after the two-byte instruction.
//
//  JSR is excluded by name rather than by mode: it carries JumpAbsolute too,
//  and there is no branch that pushes a return address.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::CanReplaceJumpWithBranch (const LineInfo & info,
                                                GlobalAddressingMode::AddressingMode mode,
                                                int32_t value, bool resolved) const
{
    constexpr int  kBranchSize          = 2;
    constexpr int  kDisplacementMinimum = -128;
    constexpr int  kDisplacementMaximum = 127;
    bool           dialectDoesIt        = m_dialect.HasJumpToBranchOptimization();
    bool           isJump               = (ToUpperCase (info.parsed.mnemonic) == kJumpMnemonic);
    bool           isAbsolute           = (mode == GlobalAddressingMode::JumpAbsolute);
    bool           hasBranch            = m_opcodeTable->HasMode (kBranchAlwaysMnemonic, GlobalAddressingMode::Relative);
    int            displacement         = value - (int) (info.pc + kBranchSize);
    bool           reaches              = (displacement >= kDisplacementMinimum) && (displacement <= kDisplacementMaximum);



    return dialectDoesIt && m_optimizeEnabled && isJump && isAbsolute && resolved && hasBranch && reaches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EncodedMnemonic
//
//  The instruction a line is ENCODED as, which is what it was written as unless
//  pass 1 replaced the jump with a branch.
//
//  Both the byte emitter and the listing's cycle count ask through here. That
//  is the point: a listing reporting the timing of an instruction the image
//  does not contain is worse than no listing, and two independent lookups is
//  how that happens.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::EncodedMnemonic (const LineInfo & info)
{
    std::string  mnemonic = info.parsed.mnemonic;



    if (info.jumpOptimizedToBranch)
    {
        mnemonic = kBranchAlwaysMnemonic;
    }

    return mnemonic;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EstimateErrorRecoverySize — how far to advance the PC past an instruction
//  that could not be encoded
//
//  Error recovery only. The single caller reaches this after RecordError, once
//  ResolveAddressingMode has named a mode the opcode table does not carry, so
//  the assembly has already failed. The goal is only to keep the labels on the
//  following lines close enough to their true addresses that the remaining
//  diagnostics stay useful instead of cascading.
//
//  It is NOT the forward-reference sizing path, despite what this function was
//  called and commented for. A forward reference the table *can* encode is
//  sized from the OpcodeEntry the caller already looked up; nothing routes here.
//
//  The best guess is the width the mnemonic actually has. For the (…) syntaxes
//  that means asking whether this is a jump: JMP and JSR are the only mnemonics
//  carrying JumpAbsolute, and both are 3 bytes in every form they have, while
//  every other indirect form on either instruction set is 2.
//
//  Deliberately NOT "the size of the mode ResolveAddressingMode returned". That
//  looks more principled and is worse: when nothing matches, the resolver
//  returns a default -- ZeroPageXIndirect for `JMP (foo,X)` on NMOS -- that the
//  mnemonic does not possess, and sizing it would advance 2 for an instruction
//  with no 2-byte encoding anywhere.
//
////////////////////////////////////////////////////////////////////////////////

Byte AssemblySession::EstimateErrorRecoverySize (OperandSyntax syntax, const std::string & mnemonic) const
{
    Byte  size = 1;      // opcode only, and the fallback for an unknown syntax



    switch (syntax)
    {
        case OperandSyntax::None:
        case OperandSyntax::Accumulator:
            break;

        case OperandSyntax::Immediate:
        case OperandSyntax::IndirectY:
            size = 2;
            break;

        case OperandSyntax::IndirectX:
        case OperandSyntax::Indirect:
            // A jump is 3 bytes -- JMP (abs), JMP (abs,X), JSR abs -- and every
            // other parenthesized form is 2, on both instruction sets.
            size = m_opcodeTable->HasMode (mnemonic, GlobalAddressingMode::JumpAbsolute) ? 3 : 2;
            break;

        case OperandSyntax::IndexedX:
        case OperandSyntax::IndexedY:
        case OperandSyntax::Bare:
            // A branch takes a one-byte signed displacement; everything else
            // with a bare or indexed operand takes a 16-bit address.
            size = IsBranchMnemonic (mnemonic) ? 2 : 3;
            break;

        case OperandSyntax::ZeroPageRelative:
            // 65C02 BBRn/BBSn: opcode + zero-page byte + relative offset.
            size = 3;
            break;
    }

    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProcessEscapeSequences
//
//  Resolves backslash escapes inside a string literal. The banner here named
//  TryEvaluateDirectiveArgs, the function BELOW it -- a copy-paste that made
//  the file appear to define it twice.
//
//  An UNRECOGNIZED escape is left intact, backslash and all, rather than
//  dropping the backslash or erroring. A 6502 source is full of paths and
//  data where a backslash is literal, and silently eating it would corrupt
//  bytes the author never meant as an escape.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::ProcessEscapeSequences (const std::string & str)
{
    std::string result;



    result.reserve (str.size());

    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] == '\\' && i + 1 < str.size())
        {
            char next = str[i + 1];

            switch (next)
            {
                case 'a':  result += '\a'; i++; break;
                case 'b':  result += '\b'; i++; break;
                case 'n':  result += '\n'; i++; break;
                case 'r':  result += '\r'; i++; break;
                case 't':  result += '\t'; i++; break;
                case '\\': result += '\\'; i++; break;
                case '"':  result += '"';  i++; break;
                default:   result += str[i]; break;
            }
        }
        else
        {
            result += str[i];
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryEvaluateDirectiveArgs
//
//  Evaluates a comma-separated expression list into values, shared by every
//  directive that takes one (.WORD, .DD, and the pass-1 sizing path).
//
//  A quoted string contributes ONE VALUE PER CHARACTER rather than a single
//  result, which is what lets `.word "AB"` and `.word 'A','B'` mean the same
//  thing. Note these characters do NOT go through the .CMAP table -- only
//  .BYTE translates, since a word-sized value is a number rather than text.
//
//  Errors are appended to the CALLER'S list rather than recorded directly, so
//  pass 1 can size a line using a throwaway list while pass 2 reports into the
//  real one. Evaluation continues past a failure so every bad argument in a
//  long table is reported in one run.
//
//  The bool says whether all of them evaluated; `values` alone cannot, since
//  an empty list is also what a legitimately empty argument produces.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::TryEvaluateDirectiveArgs (
    const std::string &                      argText,
    const ExprContext &                       ctx,
    std::vector<int32_t> &                   values,
    int                                      lineNumber,
    std::vector<AssemblyError> &             errors)
{
    auto args = Parser::SplitArgList (argText);
    bool ok   = true;



    for (const auto & arg : args)
    {
        ExprResult  er;

        // A quoted run is text, UNLESS this dialect spells a character constant
        // with the quotation mark -- in which case the evaluator owns it, and
        // sending it here instead would give the two passes different readings
        // of the same argument. The delimiter comes off the context the
        // evaluator is about to be handed, so the two cannot disagree.
        if (arg.size() >= 2 && arg.front() == '"' && arg.back() == '"' &&
            (arg.front() != ctx.highAsciiCharDelimiter))
        {
            std::string raw       = arg.substr (1, arg.size() - 2);
            std::string processed = ProcessEscapeSequences (raw);

            for (char c : processed)
            {
                values.push_back ((int32_t) (unsigned char) c);
            }

            continue;
        }

        er = ExpressionEvaluator::Evaluate (arg, ctx);

        if (!er.success)
        {
            AssemblyError error = {};
            error.lineNumber = lineNumber;
            error.message    = "Cannot evaluate expression: " + arg + " (" + er.error + ")";
            errors.push_back (error);
            ok = false;
        }
        else
        {
            values.push_back (er.value);
        }
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope types for AssemblySession
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::AssemblySession
//
////////////////////////////////////////////////////////////////////////////////

AssemblySession::AssemblySession (const InstructionSetProvider & instructionSets, const AssemblerOptions & options) :
    m_instructionSets (instructionSets),
    m_opcodeTable     (&instructionSets.GetBase()),
    m_optimizeEnabled (!options.disableOpt),
    m_options         (options),
    m_dialect         (options.dialectProfile != nullptr ? *options.dialectProfile
                                                         : DialectRegistry::Get (options.dialect)),
    m_listingLevel    (options.generateListing ? 1 : 0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordError
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::RecordError (int lineNumber, const std::string & message)
{
    RecordErrorAt (lineNumber, m_diagnosticColumn, message);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordErrorAt
//
//  The one place an ordinary error is built, with the position stated rather
//  than taken from ambient state.
//
//  RecordError is this with the line's own column, which is what the great
//  majority of diagnostics want. The exceptions are the ones whose subject is a
//  particular field -- a label that duplicates another, an operand that will not
//  evaluate -- and those say where they mean instead of moving the ambient
//  value and hoping the next caller resets it.
//
//  A column of 0 says the field was never written, and the line's own column
//  stands in. That matters most where a diagnostic exists BECAUSE the field is
//  missing: an equate with no expression has no expression to point at, and
//  answering "no column at all" would drop the position from exactly the
//  diagnostics that most need one. The substitution can never invent a position
//  for a dialect that records none, since its line column is 0 as well.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::RecordErrorAt (int lineNumber, int column, const std::string & message)
{
    AssemblyError  error = {};



    error.lineNumber = lineNumber;
    error.message    = message;
    error.file       = m_currentSourceFile;
    error.column     = (column != 0) ? column : m_diagnosticColumn;
    m_result.errors.push_back (error);
    m_result.success = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::PrimaryColumn
//
//  Which column stands for the line as a whole.
//
//  The opcode field, because that is what a line IS -- an instruction, a
//  directive, or a macro call -- and a diagnostic about the line is nearly
//  always about that. The label is the fallback for a line that has no opcode at
//  all, where it is the only thing written.
//
//  The COLUMNS are tested rather than the text, and that is not the same check.
//  A profile clears the text of both fields once it has read an equate, so a
//  test on emptiness would answer 0 for a line whose fields are perfectly well
//  known.
//
////////////////////////////////////////////////////////////////////////////////

int AssemblySession::PrimaryColumn (const ParsedLine & parsed)
{
    return (parsed.mnemonicColumn != 0) ? parsed.mnemonicColumn : parsed.labelColumn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::OperandNamesAnOperation
//
//  Whether the field after the opcode names something this assembler could
//  execute.
//
//  Asked on behalf of the active profile, and dialect-NEUTRAL by construction:
//  the instruction set is the shared one, the alternate spellings are whichever
//  the active profile declares, and the directive lookup is the active profile's
//  own. Nothing here names a dialect, and a profile that declares no aliases and
//  no directives simply gets false.
//
//  Only the FIRST WORD is considered. A field-based dialect hands the whole rest
//  of the operand over, so `LDA #$00` arrives with the instruction and its own
//  operand together.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::OperandNamesAnOperation (const ParsedLine & parsed) const
{
    std::string  word;
    size_t       end   = parsed.operand.find_first_of (" \t");
    bool         names = false;



    word  = Parser::ToUpper ((end == std::string::npos) ? parsed.operand : parsed.operand.substr (0, end));
    names = !word.empty() &&
            (m_opcodeTable->NamesAnInstruction (word) ||
             (m_dialect.GetDirectiveForSpelling (word) != Directive::None));

    for (const MnemonicAlias & alias : m_dialect.GetMnemonicAliases())
    {
        if (word == alias.spelling)
        {
            names = true;
            break;
        }
    }

    return names;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::DescribeUnknownOperation
//
//  What to say about a word the active dialect cannot execute.
//
//  Three answers, in descending order of how much they explain, and the order is
//  the point. The dialect's own account comes first because only it knows the
//  mistakes its line model invites -- an indented label reads as an unknown
//  opcode and nothing about the word itself says so. Failing that, another
//  dialect may simply define the word, and "your source is written for a
//  different assembler" is a different problem from "no such instruction". The
//  plain report is what is left.
//
//  The active dialect is NAMED whenever another one is, because the developer's
//  question at that moment is which of the two they are running.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::DescribeUnknownOperation (const ParsedLine & parsed) const
{
    std::string                          message = m_dialect.ExplainUnknownOperation (parsed,
                                                                                      OperandNamesAnOperation (parsed));
    DialectRegistry::ForeignConstruct    foreign = {};



    if (!message.empty())
    {
        return message;
    }

    message = "Invalid mnemonic: " + parsed.mnemonic;
    foreign = DialectRegistry::FindForeignConstruct (Parser::ToUpper (parsed.mnemonic), m_dialect.GetId());

    if (foreign.dialect != nullptr)
    {
        message += std::string (". ") + parsed.mnemonic + " is " + foreign.category + " belonging to the "
                 + foreign.dialect->GetName() + " dialect, not to " + m_dialect.GetName();
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::DescribeUnknownDirective
//
//  What to say about a directive spelling the active dialect's table did not
//  resolve.
//
//  The spelling is NAMED, because that is the whole content of the report: a
//  mistyped directive and a directive belonging to some other assembler are
//  indistinguishable from the line's shape alone, and the developer can tell
//  them apart the instant the word is quoted back.
//
//  The foreign-construct lookup is the same one the unknown-mnemonic report
//  uses, asked with the leading dot removed -- a dialect that spells the word
//  bare would not recognize it dotted, and it is the WORD that belongs to that
//  dialect rather than this dialect's punctuation of it.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::DescribeUnknownDirective (const ParsedLine & parsed) const
{
    std::string                       message  = "Unknown directive: " + parsed.directive;
    std::string                       bareWord = parsed.directive;
    DialectRegistry::ForeignConstruct foreign  = {};
    bool                              isDotted = !bareWord.empty() && (bareWord[0] == '.');



    if (isDotted)
    {
        bareWord = bareWord.substr (1);
    }

    foreign = DialectRegistry::FindForeignConstruct (bareWord, m_dialect.GetId());

    if (foreign.dialect != nullptr)
    {
        message += std::string (". ") + bareWord + " is " + foreign.category + " belonging to the "
                 + foreign.dialect->GetName() + " dialect, not to " + m_dialect.GetName();
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordRefusal
//
//  A construct Casso understood and declined, as against source it could not
//  make sense of.
//
//  Its own function rather than a flag on RecordError, so the kind is decided
//  by which call is written instead of by remembering an argument. There is one
//  call site, and a refusal that reached the other one would be indistinguishable
//  from a syntax error -- which is the single thing this distinction exists to
//  prevent.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::RecordRefusal (int lineNumber, const std::string & message)
{
    AssemblyError  error = {};



    error.lineNumber = lineNumber;
    error.message    = message;
    error.file       = m_currentSourceFile;
    error.column     = m_diagnosticColumn;
    error.kind       = DiagnosticKind::SubsetBoundary;
    m_result.errors.push_back (error);
    m_result.success = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordWarning
//
//  The single place -Wxxx policy is applied, so callers report a concern once
//  and never branch on the mode themselves.
//
//  FatalWarnings does not merely also-record an error -- it clears
//  m_result.success, which is what actually fails the assembly. A warning
//  routed to the errors list without that flag would print like a failure and
//  still exit zero.
//
//  NoWarn drops the message entirely rather than recording it quietly, so a
//  suppressed warning costs nothing downstream.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::RecordWarning (int lineNumber, const std::string & message)
{
    switch (m_options.warningMode)
    {
        case WarningMode::Warn:
        {
            AssemblyError warning = {};
            warning.lineNumber = lineNumber;
            warning.message    = message;
            warning.file       = m_currentSourceFile;
            warning.column     = m_diagnosticColumn;
            m_result.warnings.push_back (warning);
            break;
        }

        case WarningMode::FatalWarnings:
        {
            AssemblyError error = {};
            error.lineNumber = lineNumber;
            error.message    = message;
            error.file       = m_currentSourceFile;
            error.column     = m_diagnosticColumn;
            m_result.errors.push_back (error);
            m_result.success = false;
            break;
        }

        case WarningMode::NoWarn:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::IsAssembling
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsAssembling() const
{
    return m_condStack.empty() || m_condStack.back().assembling;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::InjectBuiltin
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::InjectBuiltin (const std::string & name, int32_t value)
{
    m_symbols[name]     = (Word) value;
    m_symbolKinds[name] = SymbolKind::Set;
    m_exprSymbols[name] = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitByte
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::EmitByte (Byte b, Word & emitPC)
{
    m_image[emitPC] = b;

    if (emitPC < m_lowestAddr)
    {
        m_lowestAddr = emitPC;
    }

    if (emitPC > m_highestAddr)
    {
        m_highestAddr = emitPC;
    }

    emitPC++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ReserveBytes
//
//  Pass 1's only way to occupy space, and the reason the two cursors cannot
//  drift by accident. Sizing a line moves the program counter and the output
//  cursor by the same amount every time; the two things that separate them are
//  an origin directive in a dialect whose origin relocates, and a dummy
//  section -- where the addresses advance and the output does not, because
//  nothing inside one is ever placed.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::ReserveBytes (Word count)
{
    // How much of what was reserved is actually PLACED. Inside a dummy section
    // the space is described rather than occupied, so none of it is: moving the
    // output cursor there would leave a hole in the object exactly the size of
    // a layout the source asked not to emit.
    Word  placed = m_inDummySection ? 0 : count;



    m_pc        += count;
    m_outputPos += placed;

    // A directive that placed nothing has produced no output, so it must not
    // be what stops the first origin from placing the image.
    if (placed > 0)
    {
        m_outputStarted = true;
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::Initialize
//
//  Resets the session and seeds the symbol table before pass 1 walks a line.
//
//  m_result.success starts TRUE and is cleared by whatever goes wrong, so
//  "nothing failed" needs no final decision -- the absence of a failure is the
//  success. Every recorded error clears it.
//
//  The three InjectBuiltin symbols exist so `IFDEF __65SC02__` is answerable
//  in a plain 6502 assembly rather than an unresolved-symbol error: they are
//  defined-but-zero, which is exactly what IFDEF tests for and IF does not.
//  Caller-supplied predefines land afterwards and may overwrite them.
//
//  Lines become PendingLine records up front rather than being read as the
//  pass goes, because macro expansion splices generated lines into this same
//  queue -- pass 1 consumes a queue that can grow while it is being walked.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::Initialize (const std::string & sourceText)
{
    HRESULT hr = S_OK;



    m_result             = {};
    m_result.success     = true;

    // The caller's answer first, so an in-source directive can see one is
    // already there and leave it alone. Precedence settled in one place beats a
    // comparison at every site that could name an output.
    m_result.outputFileName = m_options.outputFileName;

    // The dialect's own starting address, not a hard zero. Merlin assembles to
    // $8000 when the source names no origin, and a source that names one
    // overwrites this at its first ORG -- so the default is only ever visible
    // where the source said nothing. Getting it wrong yields byte-perfect
    // output at the wrong address, which reads as a far deeper problem.
    m_result.startAddress = m_dialect.GetDefaultOrigin();
    m_pc                 = m_result.startAddress;

    // The output cursor starts wherever the program counter does. Only an
    // origin directive can separate them, and only in a dialect that says so.
    m_outputPos          = m_result.startAddress;
    m_outputStarted      = false;

    // Both passes evaluate with the dialect's binding rule, or an expression
    // would fold one way while sizing a line and the other way while emitting
    // it. Same for the character-constant spelling, and for the same reason.
    m_pass1Ctx.binding   = m_dialect.GetOperatorBinding();
    m_pass2Ctx.binding   = m_dialect.GetOperatorBinding();

    m_pass1Ctx.highAsciiCharDelimiter = m_dialect.GetHighAsciiCharDelimiter();
    m_pass2Ctx.highAsciiCharDelimiter = m_dialect.GetHighAsciiCharDelimiter();

    m_pass1Ctx.operatorSpellings      = m_dialect.GetOperatorSpellings();
    m_pass2Ctx.operatorSpellings      = m_dialect.GetOperatorSpellings();

    m_pass1Ctx.arithmetic             = m_dialect.GetArithmeticWidth();
    m_pass2Ctx.arithmetic             = m_dialect.GetArithmeticWidth();

    m_pass1Ctx.extraSymbolCharacters  = m_dialect.GetExtraSymbolCharacters();
    m_pass2Ctx.extraSymbolCharacters  = m_dialect.GetExtraSymbolCharacters();

    m_lines = Parser::SplitLines (sourceText);

    InjectBuiltin ("ERRORS",     0);
    InjectBuiltin ("__65SC02__", 0);
    InjectBuiltin ("__6502X__",  0);

    for (const auto & predef : m_options.predefinedSymbols)
    {
        m_symbols[predef.first]     = (Word) predef.second;
        m_symbolKinds[predef.first] = SymbolKind::Equ;
        m_exprSymbols[predef.first] = predef.second;
    }

    for (int i = 0; i < (int) m_lines.size(); i++)
    {
        PendingLine pl = {};
        pl.text             = m_lines[i];
        pl.sourceLineNumber = i + 1;
        pl.macroDepth       = 0;
        m_pendingLines.push_back (pl);
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::SortDiagnosticsByLine
//
//  Puts errors and warnings into source order. See Run for why they are not
//  already in it. std::stable_sort, not sort: two diagnostics on the same
//  line must keep the order they were produced in, since the second is often
//  a consequence of the first and reads as nonsense before it.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::SortDiagnosticsByLine()
{
    auto byLine = [] (const AssemblyError & a, const AssemblyError & b)
    {
        return a.lineNumber < b.lineNumber;
    };



    std::stable_sort (m_result.errors.begin(),   m_result.errors.end(),   byLine);
    std::stable_sort (m_result.warnings.begin(), m_result.warnings.end(), byLine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::Run
//
//  Drives the whole assembly and hands back the result.
//
//  Diagnostics are sorted by line on the way out because recording order is
//  not source order, and nothing downstream can recover it: pass 1 walks the
//  file, then ValidateAssemblyCompletion reports unclosed blocks whose lines
//  are behind it, then pass 2 starts again from the top, then unused-label
//  detection adds more. An error at line 5 could otherwise print below one at
//  line 500 purely because of which pass noticed it -- which is an
//  implementation detail no reader of the list can see or use.
//
//  Stable, so several diagnostics on one line keep the order they were found
//  in and a cascade still reads the way it happened.
//
////////////////////////////////////////////////////////////////////////////////

AssemblyResult AssemblySession::Run (const std::string & sourceText)
{
    HRESULT  hr                 = S_OK;
    bool     crossedTheBoundary = false;



    hr = Initialize (sourceText);
    CHR (hr);

    hr = RunPass1();
    CHR (hr);

    // A source using a construct Casso deliberately does not support has been
    // read in full and every offender named. Emitting bytes for the rest of it
    // would answer a question nobody asked, and pass 2 would bury the refusals
    // under the cascade an unsupported construct always produces -- the entry
    // symbols a linker would have resolved are simply undefined here.
    crossedTheBoundary = !m_boundaryOffenses.empty();
    BAIL_OUT_IF (crossedTheBoundary, S_OK);

    hr = RunPass2();
    CHR (hr);

    hr = DetectUnusedLabels();
    CHR (hr);

Error:
    // Also on the bail path: a run that stopped early still reports whatever
    // it collected, and that list deserves the same order as a complete one.
    SortDiagnosticsByLine();

    return m_result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RunPass1
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RunPass1()
{
    HRESULT hr = S_OK;



    while (!m_pendingLines.empty())
    {
        PendingLine current = m_pendingLines.front();
        m_pendingLines.pop_front();

        if (!m_endAssembly)
        {
            hr = ProcessPass1Line (current);
            CHR (hr);
        }
    }

    hr = ValidateAssemblyCompletion();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ProcessPass1Line
//
//  One source line through pass 1: parse it, seed a LineInfo with the state
//  pass 2 will need, run the stage chain, and record the result.
//
//  The seeding is exhaustive on purpose. Every field is set before the stages
//  run -- including the false / zero ones -- so a stage that claims the line
//  without touching a field leaves a known value rather than whatever the
//  previous iteration left. `pc` in particular is captured HERE, before any
//  stage can advance it, because pass 2 needs the address this line started
//  at, not the one after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ProcessPass1Line (const PendingLine & current)
{
    HRESULT   hr   = S_OK;
    LineInfo  info = {};



    m_lastSourceLine = current.sourceLineNumber;

    // Before anything can fail: a diagnostic raised while processing this line
    // must name the file the line came from, and this is the last point at
    // which that is known.
    m_currentSourceFile    = current.sourceFile;

    info.parsed            = Parser::ParseLine (current.text, current.sourceLineNumber, m_dialect);
    info.sourceFile        = current.sourceFile;

    // And where on the line. Only knowable after the parse, since the profile is
    // what segmented the fields -- but set unconditionally, so a line whose
    // dialect records no columns leaves 0 rather than the previous line's answer.
    m_diagnosticColumn     = PrimaryColumn (info.parsed);

    // Which instruction set sized this line. Recorded here so pass 2 replays it
    // rather than recomputing -- see LineInfo::usedExtendedSet for why
    // recomputation is not equivalent.
    info.usedExtendedSet   = m_extendedActive;

    info.pc                = m_pc;
    info.outputPos         = m_outputPos;

    // Recorded before any stage runs, for the reason usedExtendedSet is: pass 2
    // walks this record rather than the source, so it cannot see which section
    // the line sat in unless pass 1 says so.
    info.emitsNoBytes      = m_inDummySection;

    info.isInstruction     = false;
    info.isDirective       = false;
    info.isConstant        = false;
    info.hasError          = false;
    info.valueResolved     = false;
    info.resolvedValue     = 0;
    info.resolvedMode      = GlobalAddressingMode::SingleByteNoOperand;
    info.macroDepth        = current.macroDepth;
    info.conditionalSkip   = false;
    info.listingSuppressed = (m_listingLevel <= 0);

    // Before any stage reads the line: local labels and local references become
    // the scoped names everything downstream will look up.
    hr = ApplyLocalLabelScope (current, info);
    CHR (hr);

    hr = RunPass1Stages (current, info);
    CHR (hr);

    // Every stage that claims the line leaves through here, so the record is
    // written in exactly one place. A stage that FAILS records nothing --
    // pass 2 must not see a half-processed line.
    m_lineInfos.push_back (info);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::IsMacroDefinitionStart
//
//  Two shapes open a definition, and which one a dialect uses is grammar rather
//  than vocabulary.
//
//    * as65 writes "NAME macro [params]" -- the name is the first word and the
//      keyword sits in the operand, so there is nothing for a directive table
//      to resolve and this reads the operand against the keyword the ACTIVE
//      dialect supplies.
//    * A dialect whose opening directive IS in its table -- Merlin's MAC --
//      puts the keyword in the opcode field and the name in the label, where
//      the token settles it with no string comparison at all.
//
//  Recognizing the token form rather than a spelling is what keeps this a
//  vocabulary difference: nothing here knows the word MAC.
//
//  The operand form is a spelling and so comes from the profile, for the same
//  reason the terminator and the local declaration do. A dialect answering
//  empty has no operand form at all, and the difference is not academic: the
//  vendor macro library is included with `PUT MACRO LIBRARY`, whose operand
//  begins with as65's keyword, and a fixed spelling reads that as a definition
//  of a macro named PUT.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsMacroDefinitionStart (const ParsedLine & parsed, const std::string & operandUpper,
                                               const std::string & defKeyword)
{
    bool  looksLikeMacro = !defKeyword.empty() &&
                           (operandUpper.compare (0, defKeyword.size(), defKeyword) == 0) &&
                           (operandUpper.size() <= defKeyword.size()   ||
                            operandUpper[defKeyword.size()] == ' '     ||
                            operandUpper[defKeyword.size()] == '\t');
    bool  namedByLabel   = (parsed.directiveToken == Directive::MacroDef) && !parsed.label.empty();



    return namedByLabel || (!parsed.mnemonic.empty() && !parsed.isEmpty && looksLikeMacro);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::IsConditionalDirective
//
//  The five tokens that steer conditional assembly. Shared by the classifier
//  and the handler, so the two cannot disagree about what a conditional is.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsConditionalDirective (Directive token)
{
    return token == Directive::If     || token == Directive::Ifdef ||
           token == Directive::Ifndef || token == Directive::Else  ||
           token == Directive::Endif;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::TokenForLine
//
//  Which directive, if any, a parsed line names -- through the ACTIVE PROFILE
//  and nothing else.
//
//  A dialect whose parser already resolved the word says so on the line, and
//  that answer is taken as it stands. A dialect that spells a directive where a
//  mnemonic would go leaves the word unresolved instead, so it is offered back
//  to the profile that produced the line.
//
//  Reading a fixed table here was a leak between dialects, and a wide one: the
//  word a profile DECLINED was then offered to another vocabulary, so 55
//  spellings one dialect does not have still resolved, eight of them steering
//  conditional assembly. A profile declining a word is the answer, not the
//  beginning of a search.
//
//  Upper-casing is part of that and not a nicety. Profiles are asked in upper
//  case by contract, one shipped dialect stores its mnemonic raw, and the
//  mismatch made the old leak fire on conventional upper-case source and not on
//  lower-case -- which reads like a guard and is not one. Normalizing in ONE
//  place is what keeps the three callers from disagreeing about it.
//
////////////////////////////////////////////////////////////////////////////////

Directive AssemblySession::TokenForLine (const ParsedLine & parsed) const
{
    return parsed.isDirective
               ? parsed.directiveToken
               : m_dialect.GetDirectiveForSpelling (ToUpperCase (parsed.mnemonic));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::IsConditionalLine
//
//  Both spellings are accepted: the directive form the parser resolved, and the
//  bare mnemonic form a dialect may also allow. The bare form never takes the
//  parser's directive path and so carries no token, which is why the line is
//  resolved rather than read straight off ParsedLine.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsConditionalLine (const ParsedLine & parsed) const
{
    return IsConditionalDirective (TokenForLine (parsed));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::IsSegmentDirective
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsSegmentDirective (Directive token)
{
    return token == Directive::SegmentCode || token == Directive::SegmentData ||
           token == Directive::SegmentBss;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ClassifyPrelude
//
//  Which prelude directive, if any, owns this line. Order is load-bearing:
//
//    * A macro definition is only recognized while assembling, so a .MACRO
//      inside an inactive conditional is skipped like any other line.
//    * Conditional directives MUST be recognized while skipping, otherwise a
//      false block could never see its own .ELSE / .ENDIF and would swallow
//      the rest of the file. So they are tested before the skip.
//    * .ORG and the segment switches move the PC, which is why the whole
//      prelude runs before RecordLabel.
//
////////////////////////////////////////////////////////////////////////////////

AssemblySession::Pass1Prelude AssemblySession::ClassifyPrelude (
    const LineInfo    & info,
    const std::string & operandUpper) const
{
    Pass1Prelude               kind        = Pass1Prelude::None;
    const SubsetBoundaryRow *  boundaryRow = SubsetBoundary::Find (m_dialect.GetSubsetBoundary(),
                                                                   info.parsed.directiveToken);



    if (IsAssembling() && IsMacroDefinitionStart (info.parsed, operandUpper,
                                                  m_dialect.GetMacroSyntax().defKeyword))
    {
        kind = Pass1Prelude::MacroDefinition;
    }
    else if (IsConditionalLine (info.parsed))
    {
        kind = Pass1Prelude::Conditional;
    }
    else if (!IsAssembling())
    {
        kind = Pass1Prelude::Skipped;
    }
    else if (info.parsed.isDirective && info.parsed.directiveToken == Directive::Org)
    {
        // The TOKEN, not the canonical spelling. Matching ".ORG" made the origin
        // directive reachable only from a dialect that spells it with a dot, so
        // a second dialect's ORG parsed correctly, resolved to the right token,
        // and then silently did nothing. AS65 is unaffected: both its spellings
        // already reported the same canonical name.
        kind = Pass1Prelude::Org;
    }
    else if (info.parsed.isDirective && info.parsed.directiveToken == Directive::KeyboardInput)
    {
        kind = Pass1Prelude::KeyboardInput;
    }
    else if (info.parsed.isDirective && boundaryRow != nullptr)
    {
        // Classified, not yet refused: a row whose trigger is the second
        // occurrence answers for a construct whose first occurrence is
        // accepted, and only the handler counts. Tested AFTER the skip arm on
        // purpose -- a construct inside a false conditional is not assembled,
        // so refusing it would report a boundary the source never crossed.
        kind = Pass1Prelude::SubsetBoundary;
    }
    else if (info.parsed.isDirective && IsSegmentDirective (info.parsed.directiveToken))
    {
        kind = Pass1Prelude::SegmentSwitch;
    }

    return kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ClassifyContent
//
////////////////////////////////////////////////////////////////////////////////

AssemblySession::Pass1Content AssemblySession::ClassifyContent (const LineInfo & info)
{
    Pass1Content  kind = Pass1Content::Instruction;



    if (info.parsed.isConstant)
    {
        kind = Pass1Content::ConstantDefinition;
    }
    else if (info.parsed.isDirective)
    {
        kind = Pass1Content::Directive;
    }
    else if (info.parsed.mnemonic.empty())
    {
        kind = Pass1Content::Empty;
    }

    return kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RunCollectingState
//
//  Phase 1. A struct or macro body swallows the line whole, so this outranks
//  everything -- including conditionals. The handlers can end collection on
//  the way out, which is why the claim is read from the state on entry.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RunCollectingState (const PendingLine & current, LineInfo & info, bool & outClaimed)
{
    HRESULT  hr = S_OK;



    outClaimed = (m_pass1State != Pass1State::Normal);

    switch (m_pass1State)
    {
    case Pass1State::CollectingStruct:
        hr = HandleStructCollection (current, info);
        break;

    case Pass1State::CollectingMacro:
        hr = CollectMacroBody (current, info);
        break;

    case Pass1State::CollectingLoop:
        hr = CollectLoopBody (current, info);
        break;

    case Pass1State::Normal:
        break;
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RunPreludeDirectives
//
//  Phase 2. Everything that decides whether the line assembles at all, or
//  that moves the PC. Runs before a label can bind.
//
//  `entryPC` is the program counter as the line was REACHED, read before any
//  handler can move it. That is where a label on this line binds -- see the
//  origin case for why nothing the directive then does may change it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RunPreludeDirectives (const PendingLine & current, LineInfo & info, bool & outClaimed)
{
    HRESULT       hr           = S_OK;
    std::string   operandUpper = ToUpperCase (info.parsed.operand);
    Pass1Prelude  kind         = ClassifyPrelude (info, operandUpper);
    Word          entryPC      = m_pc;



    outClaimed = (kind != Pass1Prelude::None);

    switch (kind)
    {
    case Pass1Prelude::MacroDefinition:
        hr = DetectMacroDefinition (current, info, operandUpper, outClaimed);
        break;

    case Pass1Prelude::Conditional:
        hr = HandleConditionalDirective (current, info, outClaimed);
        break;

    case Pass1Prelude::Skipped:
        info.conditionalSkip = true;
        break;

    case Pass1Prelude::Org:
        hr = HandleOrgDirective (current, info);
        CHR (hr);

        // An origin claims the line before the label stage runs, so a label
        // sharing it had been dropped without a word. It binds to the program
        // counter AS THE LINE WAS REACHED -- exactly where a label on any other
        // line binds -- rather than to anything the directive then does.
        hr = RecordLabel (current, info, entryPC);
        info.isDirective = true;
        break;

    case Pass1Prelude::KeyboardInput:
        hr = HandleKeyboardInput (current, info);
        CHR (hr);

        info.isDirective = true;
        break;

    // A refused construct is claimed and nothing else happens to the line --
    // deliberately including its label, which is how an entry symbol is
    // written. The refusal is the whole answer for that line, and binding a
    // label there could only add a second complaint about a construct already
    // declined.
    case Pass1Prelude::SubsetBoundary:
        hr = HandleSubsetBoundary (current, info, outClaimed);
        break;

    case Pass1Prelude::SegmentSwitch:
        hr = HandleSegmentSwitch (info, outClaimed);
        break;

    case Pass1Prelude::None:
        break;
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RunContentStages
//
//  Phase 3. The line's payload, at the settled PC.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RunContentStages (const PendingLine & current, LineInfo & info)
{
    HRESULT  hr      = S_OK;
    bool     claimed = false;



    switch (ClassifyContent (info))
    {
    case Pass1Content::ConstantDefinition:
        hr = HandleConstantDefinition (current, info);
        break;

    case Pass1Content::Directive:
        hr = HandlePass1Directives (current, info, claimed);
        break;

    case Pass1Content::Empty:
        break;

    case Pass1Content::Instruction:
        hr = ResolveInstructionLine (current, info);
        break;
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RunPass1Stages
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RunPass1Stages (const PendingLine & current, LineInfo & info)
{
    HRESULT  hr      = S_OK;
    bool     claimed = false;



    hr = RunCollectingState (current, info, claimed);
    CHR (hr);
    BAIL_OUT_IF (claimed, S_OK);

    hr = RunPreludeDirectives (current, info, claimed);
    CHR (hr);
    BAIL_OUT_IF (claimed, S_OK);

    // The PC has stopped moving, so a label on this line binds here.
    hr = RecordLabel (current, info, m_pc);
    CHR (hr);

    hr = RunContentStages (current, info);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ResolveInstructionLine
//
//  The tail no classifier can decide. Each of these three can only tell
//  whether it owns the line by starting work on it -- a multi-NOP has to
//  evaluate its operand and declines when the count is not positive, a macro
//  call has to be found in the table, and a colon-less label is whatever is
//  left once every real mnemonic form has had its turn. The order is the
//  precedence.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ResolveInstructionLine (const PendingLine & current, LineInfo & info)
{
    HRESULT  hr      = S_OK;
    bool     claimed = false;



    hr = HandleMultiNop (current, info, claimed);
    CHR (hr);
    BAIL_OUT_IF (claimed, S_OK);

    hr = ExpandMacro (current, info, claimed);
    CHR (hr);
    BAIL_OUT_IF (claimed, S_OK);

    hr = HandleColonlessLabel (current, info, claimed);
    CHR (hr);
    BAIL_OUT_IF (claimed, S_OK);

    hr = ClassifyAndResolve (current, info);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleStructCollection
//
//  One line of a .STRUCT body. .ENDSTRUCT closes the definition and publishes
//  the struct's total size as an EQU symbol under its own name; anything else
//  non-blank is a member declaration.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleStructCollection (const PendingLine & current, LineInfo & info)
{
    HRESULT  hr          = S_OK;
    bool     isEndStruct = false;
    int32_t  structSize  = 0;



    hr = CheckEndStruct (current, info, isEndStruct);
    CHR (hr);

    if (isEndStruct)
    {
        structSize = m_currentStruct.currentOffset - m_currentStruct.startOffset;

        m_symbols[m_currentStruct.name]     = (Word) structSize;
        m_symbolKinds[m_currentStruct.name] = SymbolKind::Equ;
        m_exprSymbols[m_currentStruct.name] = structSize;
        m_structs[m_currentStruct.name]     = m_currentStruct;

        m_pass1State = Pass1State::Normal;
    }
    else if (!info.parsed.isEmpty)
    {
        hr = ParseStructMember (current, info);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CheckEndStruct
//
//  Does this line close a STRUCT block? Two spellings reach here through
//  different parse paths, which is the whole reason this is a function rather
//  than an inline test: `.END STRUCT` arrives as a directive carrying an
//  argument, while bare `end struct` arrives as a mnemonic carrying an
//  operand. Only the first word of what follows is compared, so `.END STRUCT`
//  and `.END STRUCT ; done` both close it.
//
//  `isEnd` is the answer; the HRESULT reports only whether the check itself
//  could run, so a caller must not read one as the other.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::CheckEndStruct (const PendingLine & current, LineInfo & info, bool & isEnd)
{
    HRESULT      hr       = S_OK;
    std::string  endsWhat;



    isEnd = false;



    // The TOKEN, not the canonical spelling. Comparing ".END" recognized the
    // end directive only in a dialect that spells it with a dot, so a dialect
    // spelling it otherwise would resolve to the right token and then fail to
    // close the block -- every following line swallowed into the struct with no
    // diagnostic. Same trap the origin directive was caught in.
    //
    // The mnemonic arm survives for a dialect that spells its end directive
    // where a mnemonic would go; both spellings name what they close in the
    // first word of what follows.
    //
    // WHICH WORD spells either of them is the active profile's business and not
    // this function's. Comparing against fixed text let one dialect's source be
    // closed by another dialect's word, and it did so in both halves at once:
    // the opening word and the name of the thing it closes.
    if (TokenForLine (info.parsed) == Directive::End)
    {
        endsWhat = info.parsed.isDirective ? info.parsed.directiveArg : info.parsed.operand;
    }

    isEnd = (m_dialect.GetDirectiveForSpelling (GetLeadingWord (ToUpperCase (endsWhat))) == Directive::Struct);

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::GetStructMemberTypes
//
//  The storage directives a struct member may be declared with, and how wide
//  one element of each is. Only the *widths* live here -- the spellings do not,
//  because DirectiveTable already owns those. Before this was a token table it
//  was an if/else chain naming DS/DSB/RMB/DB/BYT/BYTE/FCB/DW/WORD/FCW/FDB/DD, a
//  second copy of the vocabulary that a dialect adding a synonym would not have
//  reached; struct members would silently have stopped recognizing it.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const AssemblySession::StructMemberType> AssemblySession::GetStructMemberTypes()
{
    static constexpr StructMemberType  s_kTypes[] =
    {
        { Directive::Ds,   kSizeFromOperand },
        { Directive::Byte, 1                },
        { Directive::Word, 2                },
        { Directive::Dd,   4                },
    };



    return std::span<const StructMemberType> (s_kTypes, std::size (s_kTypes));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::GetStructMemberSize
//
//  Splits a member declaration's operand -- `<directive> [count]` -- and
//  returns the number of bytes it reserves, or 0 when the leading word is not a
//  storage directive at all.
//
//  The ambiguous vocabulary is consulted as well as the ordinary one, and this
//  is the only place that is sound: inside a structure body there is no
//  instruction to be ambiguous with, so `count rmb 4` is unambiguously four
//  reserved bytes. Both answers come from the ACTIVE profile -- a dialect with
//  no structures claims neither spelling and reserves nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::GetStructMemberSize (const std::string & operand, int32_t & outSize)
{
    HRESULT                   hr    = S_OK;
    const StructMemberType  * match = nullptr;
    Directive                 token = {};
    size_t                    split     = operand.find_first_of (" \t");
    std::string               spelling  = ToUpperCase (operand.substr (0, split));
    std::string               countExpr = (split == std::string::npos) ? "" : operand.substr (split);
    size_t                    exprStart = countExpr.find_first_not_of (" \t");



    outSize   = 0;
    countExpr = (exprStart == std::string::npos) ? countExpr : countExpr.substr (exprStart);
    token     = m_dialect.GetDirectiveForSpelling (spelling);

    if (token == Directive::None)
    {
        token = m_dialect.GetAmbiguousDirectiveForSpelling (spelling);
    }



    for (const StructMemberType & type : GetStructMemberTypes())
    {
        if (type.token == token)
        {
            match = &type;
            break;
        }
    }

    // No match leaves outSize at 0, and the caller drops the line.
    if (match != nullptr)
    {
        if (match->elementSize == kSizeFromOperand)
        {
            ExprResult  er;

            // `.DS <count>` -- the operand carries the width. An expression that
            // does not evaluate falls back to one byte rather than dropping the
            // member, so offsets after it stay plausible for the rest of pass 1.
            m_pass1Ctx.currentPC = (int32_t) m_pc;

            er = ExpressionEvaluator::Evaluate (countExpr, m_pass1Ctx);

            outSize = er.success ? er.value : 1;
        }
        else
        {
            outSize = match->elementSize;
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordStructMember
//
//  Publishes one member as `<Struct>.<member>` and advances the running offset.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RecordStructMember (const std::string & name, int32_t size)
{
    HRESULT       hr      = S_OK;
    StructMember  member  = {};
    std::string   symName = m_currentStruct.name + "." + name;



    member.name   = name;
    member.offset = m_currentStruct.currentOffset;
    member.size   = size;
    m_currentStruct.members.push_back (member);

    m_symbols[symName]     = (Word) m_currentStruct.currentOffset;
    m_symbolKinds[symName] = SymbolKind::Equ;
    m_exprSymbols[symName] = m_currentStruct.currentOffset;

    m_currentStruct.currentOffset += size;

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ParseStructMember
//
//  One `<name> <directive> [count]` line inside a .STRUCT body. The name comes
//  from the raw text rather than the parsed mnemonic so it keeps its original
//  case; the parser has already upper-cased its copy.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ParseStructMember (const PendingLine & current, LineInfo & info)
{
    HRESULT      hr         = S_OK;
    std::string  memberName = GetLeadingWord (current.text);
    int32_t      memberSize = 0;



    BAIL_OUT_IF (info.parsed.mnemonic.empty() || info.parsed.operand.empty(), S_OK);

    hr = GetStructMemberSize (info.parsed.operand, memberSize);
    CHR (hr);

    BAIL_OUT_IF (memberName.empty() || memberSize <= 0, S_OK);

    hr = RecordStructMember (memberName, memberSize);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CollectMacroBody
//
//  Pass-1 state handler while inside a MACRO definition: every line is
//  swallowed into the pending body verbatim rather than assembled, until
//  ENDM closes it and the finished definition lands in m_macros.
//
//  Verbatim is the point. The body is re-parsed at each expansion site with
//  the arguments substituted, so assembling it here would resolve labels and
//  expressions against the definition's PC instead of the call site's.
//
//  EVERY OPEN DEFINITION RECEIVES EVERY LINE, and one terminator closes them
//  all. A definition opened here does not become body text of the one already
//  collecting -- it opens beside it, and from that point the two share a tail.
//  So a pair written as
//
//      ADDX MAC        ADDX is TXA CLC ADC ]1
//       TXA            ADDA is      CLC ADC ]1
//      ADDA MAC
//       CLC
//       ADC ]1
//       <<<
//
//  makes both, the first ending with the second's body. Collecting the opening
//  line as text instead makes only the first, containing an opener that nothing
//  ever closes, and the source reads as one unterminated definition.
//
//  The local-label declaration is the one directive read on the way past,
//  because its names have to be known before expansion can rename them
//  per-invocation -- otherwise two invocations in the same file would define
//  the same label twice. It is still pushed into the body as well, since
//  expansion re-reads it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::CollectMacroBody (const PendingLine & current, LineInfo & info)
{
    HRESULT      hr            = S_OK;
    MacroSyntax  syntax        = m_dialect.GetMacroSyntax();
    std::string  mnemonic      = ToUpperCase (info.parsed.mnemonic);
    std::string  operandUpper  = ToUpperCase (info.parsed.operand);
    std::string  endKeyword    = syntax.endKeyword;
    std::string  localKeyword  = syntax.localKeyword;
    bool         opensAnother  = false;
    bool         endsBody      = false;
    bool         declaresLocal = false;



    // A further definition OPENS rather than being collected, recognized by the
    // same predicate the prelude opens the first one with -- so nothing decides
    // twice what an opening line looks like. Tested first because such a line is
    // never a terminator and never a local declaration.
    opensAnother = IsMacroDefinitionStart (info.parsed, operandUpper, syntax.defKeyword);

    // The macro-end TOKEN closes the body, and so does the active dialect's
    // bare keyword. as65 needs the keyword: it has no token for this, since
    // `.ENDM` is absent from its spelling table and parses as an unrecognized
    // dotted directive that merely keeps its text. A dialect that DOES carry
    // the token was the one being lost -- it would spell its terminator,
    // resolve it to exactly the right token, and then have it swallowed into
    // the body it was meant to close, leaving the remainder of the file inside
    // a macro nobody calls.
    endsBody = (info.parsed.directiveToken == Directive::MacroEnd) ||
               (!endKeyword.empty() &&
                ((mnemonic == endKeyword) ||
                 (info.parsed.isDirective && info.parsed.directive == "." + endKeyword)));

    if (opensAnother)
    {
        OpenMacroDefinition (current, info, operandUpper);
    }
    else if (endsBody)
    {
        // The terminator may carry a LABEL, and the vendor sources do it:
        // KEYMAC.S ends a macro with `NI <<<`, so the body's own branch target
        // sits on the same line that closes the definition. Dropping the line
        // whole -- which is what closing the body does -- takes that label with
        // it and leaves every branch to it unresolvable. The label is re-emitted
        // on a line of its own instead, where expansion renames it like any
        // other body label.
        if (syntax.labelsArePerExpansion && !info.parsed.label.empty())
        {
            for (MacroDefinition & pending : m_pendingMacros)
            {
                pending.localLabels.push_back (info.parsed.label);
                pending.body.push_back (info.parsed.label);
            }
        }

        // One terminator closes every definition that is open, which is what
        // makes the shorthand work at all -- the shorter definitions were never
        // going to get a terminator of their own.
        for (const MacroDefinition & pending : m_pendingMacros)
        {
            m_macros[pending.name] = pending;
        }

        m_pendingMacros.clear();
        m_pass1State = Pass1State::Normal;
    }
    else
    {
        // The declaring keyword comes from the ACTIVE dialect rather than being
        // compared against a fixed word. A dialect without one answers empty and
        // no line is claimed -- which matters, because in a field-based dialect a
        // line beginning with that word is a LABEL in column 1, and treating it
        // as a declaration deletes the line's instruction and its label together.
        declaresLocal = !localKeyword.empty() &&
                        ((mnemonic == localKeyword) ||
                         (info.parsed.isDirective && info.parsed.directive == "." + localKeyword));

        if (declaresLocal)
        {
            std::string  localArg   = (mnemonic == localKeyword) ? info.parsed.operand : info.parsed.directiveArg;
            auto         localNames = Parser::SplitArgList (localArg);

            for (const auto & ln : localNames)
            {
                std::string name = ln;
                size_t ns = name.find_first_not_of (" \t");
                size_t ne = name.find_last_not_of (" \t");

                if (ns != std::string::npos)
                {
                    name = name.substr (ns, ne - ns + 1);
                }

                if (!name.empty())
                {
                    for (MacroDefinition & pending : m_pendingMacros)
                    {
                        pending.localLabels.push_back (name);
                    }
                }
            }
        }

        // A dialect whose body labels are unique per expansion declares nothing,
        // so every label the body defines is collected on the way past. Without
        // it the second expansion redefines the first's labels and every branch
        // in it resolves to the wrong copy -- which is why Merlin's own sources
        // can expand one macro three times and still ship a working object.
        if (syntax.labelsArePerExpansion && !info.parsed.label.empty())
        {
            for (MacroDefinition & pending : m_pendingMacros)
            {
                pending.localLabels.push_back (info.parsed.label);
            }
        }

        for (MacroDefinition & pending : m_pendingMacros)
        {
            pending.body.push_back (current.text);
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::DetectMacroDefinition
//
//  Recognizes `NAME MACRO [params]` and flips pass 1 into CollectingMacro, so
//  every following line is swallowed by CollectMacroBody until ENDM.
//
//  Skipped entirely when not assembling: a macro inside a false conditional
//  must not be defined, or a later invocation would silently expand a
//  definition the author excluded.
//
//  `handled` reports whether this line opened a definition; the HRESULT
//  reports only whether the attempt itself ran.
//
//  This is the FIRST definition only. A line opening one while another is
//  already collecting never reaches the prelude -- the collecting state claims
//  it first -- and is opened from there, through the same helper.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::DetectMacroDefinition (const PendingLine & current, LineInfo & info,
                                                 const std::string & operandUpper, bool & handled)
{
    HRESULT  hr = S_OK;



    handled = false;

    // Same predicate ClassifyPass1Line used to route here, so the two cannot
    // disagree about what opens a definition.
    BAIL_OUT_IF (!IsAssembling(), S_OK);
    BAIL_OUT_IF (!IsMacroDefinitionStart (info.parsed, operandUpper,
                                          m_dialect.GetMacroSyntax().defKeyword), S_OK);

    OpenMacroDefinition (current, info, operandUpper);

    handled = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::OpenMacroDefinition
//
//  Pushes one definition onto the collecting stack and puts pass 1 into the
//  collecting state. Everything that makes a definition -- its name, where it
//  opened, and any parameter names it declared -- is decided here and nowhere
//  else, because both the prelude and the collector open definitions and a
//  second copy of this reasoning is a second answer waiting to disagree.
//
//  A name that collides with a real mnemonic is recorded as an error but the
//  definition still proceeds -- reporting one clear "conflicts with mnemonic"
//  beats abandoning the definition and then emitting an "unknown macro" at
//  every call site, which buries the actual cause.
//
//  WHERE THE NAME IS depends on the shape. In as65's `NAME macro` the keyword
//  is the operand, so the name is the first word; in the token form the keyword
//  occupies the opcode field and the name is the label beside it. Reading the
//  wrong field would name the macro after its own opening directive, and every
//  call site would then fail to find it.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::OpenMacroDefinition (const PendingLine & current, const LineInfo & info,
                                             const std::string & operandUpper)
{
    MacroDefinition  opened     = {};
    std::string      defKeyword = m_dialect.GetMacroSyntax().defKeyword;



    opened.name = (info.parsed.directiveToken == Directive::MacroDef)
                      ? info.parsed.label
                      : info.parsed.mnemonic;

    // Name collision check
    if (m_opcodeTable->NamesAnInstruction (opened.name))
    {
        RecordError (current.sourceLineNumber, "Macro name conflicts with mnemonic: " + opened.name);
    }

    opened.lineNumber = current.sourceLineNumber;
    opened.sourceFile = current.sourceFile;
    opened.openColumn = PrimaryColumn (info.parsed);

    // Parameter names follow the opening keyword in the operand, so this reads
    // the operand shape only. The token form declares no names at all -- its
    // parameters are positional -- and taking a substring of its operand would
    // turn whatever the line carried into a parameter list.
    if ((info.parsed.directiveToken != Directive::MacroDef) &&
        !defKeyword.empty() && (operandUpper.size() > defKeyword.size()))
    {
        std::string paramStr = info.parsed.operand.substr (defKeyword.size() + 1);

        if (!paramStr.empty())
        {
            auto paramNames = Parser::SplitArgList (paramStr);

            for (const auto & pn : paramNames)
            {
                std::string name = pn;
                size_t ns = name.find_first_not_of (" \t");
                size_t ne = name.find_last_not_of (" \t");

                if (ns != std::string::npos)
                {
                    name = name.substr (ns, ne - ns + 1);
                }

                if (!name.empty())
                {
                    opened.paramNames.push_back (name);
                }
            }
        }
    }

    m_pendingMacros.push_back (opened);
    m_pass1State = Pass1State::CollectingMacro;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleConditionalDirective
//
//  Single entry point for IF / IFDEF / IFNDEF / ELSE / ENDIF, dispatching to
//  the one that matches. `handled` tells the caller whether this line was a
//  conditional at all, so a non-conditional falls through to normal assembly
//  untouched -- that is what the bail is for, not an error.
//
//  Both spellings normalize here first. A dotted `.IF` carries its token from
//  the parser, while a bare `IF` never took the directive path and so has to
//  be resolved from its mnemonic, with the argument coming from whichever
//  field that spelling filled. Everything downstream sees one shape.
//
//  Conditionals must be recognized even inside a skipped block -- a nested
//  IF/ENDIF has to keep the nesting depth honest, or the ENDIF that closes
//  the outer block would be consumed by the inner one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleConditionalDirective (const PendingLine & current, LineInfo & info,
                                                      bool & handled)
{
    HRESULT      hr      = S_OK;
    Directive    token   = Directive::None;
    std::string  condArg;



    handled = false;



    // The token comes from the active profile either way -- see TokenForLine.
    // Only the ARGUMENT differs, because the two spellings put it in different
    // fields.
    token   = TokenForLine (info.parsed);
    condArg = info.parsed.isDirective ? info.parsed.directiveArg : info.parsed.operand;

    BAIL_OUT_IF (!IsConditionalDirective (token), S_OK);

    if (token == Directive::If)
    {
        hr = HandleIfDirective (current, condArg);
        CHR (hr);
    }
    else if (token == Directive::Ifdef || token == Directive::Ifndef)
    {
        hr = HandleIfdefDirective (current, token, condArg);
        CHR (hr);
    }
    else if (token == Directive::Else)
    {
        hr = HandleElseDirective (current);
        CHR (hr);
    }
    else if (token == Directive::Endif)
    {
        hr = HandleEndifDirective (current);
        CHR (hr);
    }

    info.isDirective = true;
    handled = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleIfDirective
//
//  Opens a conditional block, pushing its state onto m_condStack.
//
//  The condition is evaluated ONLY when the enclosing block is itself
//  assembling. Inside a skipped region the expression may reference symbols
//  that were never defined -- that is usually the whole point of skipping it --
//  so evaluating anyway would report errors for code the author excluded.
//  parentAssembling is recorded so ELSE cannot later switch a nested block
//  back on inside a parent that is off.
//
//  An expression that fails to evaluate records the error and skips the block.
//  Assembling it would be worse: the condition is unknown, so emitting is a
//  guess, and a guess produces a second, misleading round of errors from
//  inside a block that may not belong in the output at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleIfDirective (const PendingLine & current, const std::string & condArg)
{
    HRESULT hr = S_OK;



    ConditionalState state = {};



    state.parentAssembling = IsAssembling();
    state.seenElse         = false;
    state.openLineNumber   = current.sourceLineNumber;
    state.openFile         = current.sourceFile;
    state.openColumn       = m_diagnosticColumn;

    if (state.parentAssembling)
    {
        ExprResult  er;

        m_pass1Ctx.currentPC = (int32_t) m_pc;
        er = ExpressionEvaluator::Evaluate (condArg, m_pass1Ctx);

        if (!er.success)
        {
            RecordError (current.sourceLineNumber, "Cannot evaluate if expression: " + er.error);
            state.assembling = false;
        }
        else
        {
            state.assembling = (er.value != 0);
        }
    }
    else
    {
        state.assembling = false;
    }

    m_condStack.push_back (state);

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleIfdefDirective
//
//  IFDEF and IFNDEF share every step but the final sense, so they share a
//  handler and `token` picks the polarity at the end.
//
//  Existence in m_exprSymbols is the whole test -- the symbol's VALUE is never
//  read, so `sym = 0` is still defined. That is the difference from IF, which
//  evaluates and tests for non-zero.
//
//  Same skip rule as HandleIfDirective: nothing is tested unless the enclosing
//  block is assembling. Here it matters for a second reason -- pass 1 defines
//  symbols as it goes, so a symbol's definedness depends on how far the pass
//  has reached, and asking inside a region that will not be emitted invites an
//  answer that changes between passes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleIfdefDirective (const PendingLine & current,
                                                Directive           token,
                                                const std::string & condArg)
{
    HRESULT hr = S_OK;



    ConditionalState state = {};



    state.parentAssembling = IsAssembling();
    state.seenElse         = false;
    state.openLineNumber   = current.sourceLineNumber;
    state.openFile         = current.sourceFile;
    state.openColumn       = m_diagnosticColumn;

    if (state.parentAssembling)
    {
        std::string  symName = condArg;
        bool         defined = false;
        size_t s = symName.find_first_not_of (" \t");
        size_t e = symName.find_last_not_of (" \t");

        if (s != std::string::npos)
        {
            symName = symName.substr (s, e - s + 1);
        }

        defined = (m_exprSymbols.find (symName) != m_exprSymbols.end());
        state.assembling = (token == Directive::Ifdef) ? defined : !defined;
    }
    else
    {
        state.assembling = false;
    }

    m_condStack.push_back (state);

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleElseDirective
//
//  Flips the innermost conditional -- but only when its PARENT was assembling.
//  Inside a block the enclosing conditional already excluded, both arms must
//  stay off; flipping unconditionally would turn the else arm on and emit code
//  from a region the author ruled out two levels up.
//
//  seenElse makes a second ELSE an error rather than a second flip, which
//  would otherwise toggle the block back on and assemble both arms.
//
//  A stray ELSE records an error and changes nothing. Assembly continues, so
//  one unbalanced directive reports itself instead of cascading into every
//  conditional after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleElseDirective (const PendingLine & current)
{
    HRESULT hr = S_OK;



    if (m_condStack.empty())
    {
        RecordError (current.sourceLineNumber, "else without matching if");
    }
    else if (m_condStack.back().seenElse)
    {
        RecordError (current.sourceLineNumber, "Duplicate else");
    }
    else
    {
        m_condStack.back().seenElse = true;

        if (m_condStack.back().parentAssembling)
        {
            m_condStack.back().assembling = !m_condStack.back().assembling;
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleEndifDirective
//
//  Closes the innermost conditional by popping it, which restores whatever
//  the enclosing block's assembling state was -- the stack IS the nesting, so
//  no state has to be saved or restored by hand.
//
//  An ENDIF with nothing open records an error and pops nothing. The guard is
//  load-bearing rather than defensive: pop_back on an empty vector is
//  undefined, so a source file with one stray ENDIF could otherwise take the
//  assembler down instead of reporting itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleEndifDirective (const PendingLine & current)
{
    HRESULT hr = S_OK;



    if (m_condStack.empty())
    {
        RecordError (current.sourceLineNumber, "endif without matching if");
    }
    else
    {
        m_condStack.pop_back();
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleOrgDirective
//
//  Moves the assembly PC. The expression must resolve in pass 1 -- every
//  label defined after this point takes its address from the new PC, so a
//  value that only arrives in pass 2 would mean every one of them was wrong
//  the first time round.
//
//  WHETHER THE OUTPUT CURSOR COMES ALONG is the dialect's answer, not this
//  function's. A seeking dialect writes into an address-indexed image, so the
//  cursor follows and the gap becomes fill. A relocating one leaves the output
//  contiguous and changes only what labels bind to -- which is how one Merlin
//  object holds three sections assembled at three addresses.
//
//  Either way the FIRST origin places the image: until something has been
//  reserved there is no output to strand, and the byte stream has to begin
//  where the source said it would.
//
//  An origin with NO OPERAND resyncs the program counter to the cursor -- "put
//  the address back where the bytes actually are". That is meaningless while
//  the two cannot differ, so a seeking dialect keeps reporting it as a missing
//  operand rather than silently accepting a no-op.
//
//  The FIRST .org also sets m_result.startAddress, which is the load address
//  of the emitted image. Later ones only move the PC; the image still begins
//  where the first one put it.
//
//  A repeat .org to the address the PC is already at is a warning, not an
//  error: harmless, but almost always a leftover from moving code around,
//  and silent success would leave it there forever.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleOrgDirective (const PendingLine & current, LineInfo & info)
{
    HRESULT     hr        = S_OK;
    bool        relocates = (m_dialect.GetOriginSemantic() == OriginSemantic::ProgramCounterOnly);
    bool        isResync  = relocates && info.parsed.directiveArg.empty();
    ExprResult  er;



    if (isResync)
    {
        m_pc    = m_outputPos;
        info.pc = m_pc;
    }

    BAIL_OUT_IF (isResync, S_OK);

    m_pass1Ctx.currentPC = (int32_t) m_pc;
    er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass1Ctx);

    if (!er.success)
    {
        RecordErrorAt (current.sourceLineNumber, info.parsed.operandColumn,
                       info.parsed.directive + " expression must be resolvable: " + er.error);
    }
    else
    {
        Word newAddr = (Word) er.value;

        if (m_originSet && newAddr == m_pc)
        {
            RecordWarning (current.sourceLineNumber, "Redundant .org to current address");
        }

        m_pc    = newAddr;
        info.pc = m_pc;

        if (!relocates || !m_outputStarted)
        {
            m_outputPos    = newAddr;
            info.outputPos = newAddr;
        }

        if (!m_originSet)
        {
            m_result.startAddress = newAddr;
            m_originSet = true;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleKeyboardInput
//
//  A line that names a symbol and asks for its value from outside the source.
//
//  Merlin stops the assembly and prompts the operator at the keyboard; the
//  answer becomes the symbol's value, and conditional assembly downstream reads
//  it. A batch assembler has nobody to ask, so the answer comes from the
//  predefined symbols the caller supplied -- the same channel every other
//  externally-supplied value arrives on -- and nothing is ever read from a
//  console.
//
//  THE MISSING ANSWER IS AN ERROR, deliberately, and neither of the two easier
//  outcomes is acceptable. Blocking on a prompt turns an unattended build into a
//  hang, and defaulting to zero assembles a DIFFERENT PROGRAM in silence: the
//  vendor sources gate whole sections on these symbols, so a wrong answer
//  produces a clean assembly of code nobody asked for. The diagnostic carries
//  the symbol and the prompt, because the prompt is the only place the source
//  says what the answer means.
//
//  The binding is written here rather than left to the blanket predefine so the
//  directive does its own work: it is the same value from the same map, but a
//  reader of this function can see what a KBD line results in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleKeyboardInput (const PendingLine & current, LineInfo & info)
{
    HRESULT             hr        = S_OK;
    const std::string & name      = info.parsed.label;
    std::string         prompt    = StripDelimitedText (info.parsed.directiveArg);
    bool                hasName   = !name.empty();
    auto                answer    = m_options.predefinedSymbols.end();
    bool                hasAnswer = false;



    if (!hasName)
    {
        RecordError (current.sourceLineNumber,
            info.parsed.directive + " needs a symbol name in the label field");
    }

    BAIL_OUT_IF (!hasName, S_OK);

    answer    = m_options.predefinedSymbols.find (name);
    hasAnswer = (answer != m_options.predefinedSymbols.end());

    if (!hasAnswer)
    {
        std::string  message = "No answer supplied for " + name;

        if (!prompt.empty())
        {
            message += " (" + prompt + ")";
        }

        message += ". Define it on the command line, for example "
                 + std::string (1, m_options.flagPrefix) + "d " + name + "=0";

        RecordError (current.sourceLineNumber, message);
    }

    BAIL_OUT_IF (!hasAnswer, S_OK);

    m_symbols[name]     = (Word) answer->second;
    m_symbolKinds[name] = SymbolKind::Equ;
    m_exprSymbols[name] = answer->second;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleSubsetBoundary
//
//  One construct the active profile refuses, recorded rather than reported.
//
//  Nothing is said here, and that is the design. The advice a relocatable
//  module gets turns on whether ANY line of it declares an external symbol, so
//  the earliest moment the message can be right is after the last line has been
//  read. Reporting at the point of the construct would mean choosing between
//  offering a fix that may not work and offering none at all.
//
//  The claim is conditional because a construct can be inside the subset once
//  and outside it twice. An occurrence that the row does not refuse is left
//  entirely alone -- unclaimed, so whatever handles it ordinarily still does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleSubsetBoundary (const PendingLine & current, LineInfo & info, bool & outClaimed)
{
    HRESULT                    hr         = S_OK;
    const SubsetBoundaryRow *  row        = SubsetBoundary::Find (m_dialect.GetSubsetBoundary(),
                                                                  info.parsed.directiveToken);
    int                        occurrence = 0;
    BoundaryOffense            offense    = {};



    // The classifier found a row for this token a moment ago, so its absence
    // now would mean the boundary table changed mid-line.
    CBRA (row);

    occurrence = ++m_boundaryOccurrences[(int) info.parsed.directiveToken];
    outClaimed = (row->trigger == SubsetBoundaryTrigger::EveryOccurrence) || (occurrence > 1);

    BAIL_OUT_IF (!outClaimed, S_OK);

    offense.row        = row;
    offense.lineNumber = current.sourceLineNumber;
    offense.column     = PrimaryColumn (info.parsed);
    offense.file       = current.sourceFile;

    m_boundaryOffenses.push_back (offense);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ReportSubsetBoundaryRefusals
//
//  Every construct the boundary refused, reported once the whole source has
//  been read.
//
//  Every one of them, not the first. A developer meeting this boundary is
//  deciding whether to port a file at all, and that decision needs the size of
//  the gap; stopping at the first refusal turns one answer into as many
//  assembly runs as there are constructs.
//
//  The module's linkage is settled before anything is composed, because it is a
//  property of the file rather than of a line: one external declaration
//  anywhere rules out the workaround for every refusal in the file, including
//  those above it in the source.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::ReportSubsetBoundaryRefusals()
{
    ModuleLinkage  linkage = ModuleLinkage::SelfContained;



    for (const BoundaryOffense & offense : m_boundaryOffenses)
    {
        if (offense.row->makesModuleDependOnAnother)
        {
            linkage = ModuleLinkage::DependsOnOther;
        }
    }

    for (const BoundaryOffense & offense : m_boundaryOffenses)
    {
        // Deferred, so the ambient file is whichever was processed last. The
        // file the construct was MET in is restored before recording, exactly
        // as the unclosed-block diagnostics below do -- and so is the column,
        // which would otherwise point at wherever the last line's opcode
        // happened to sit.
        m_currentSourceFile = offense.file;
        m_diagnosticColumn  = offense.column;

        RecordRefusal (offense.lineNumber,
                       SubsetBoundary::ComposeRefusal (*offense.row, linkage, m_dialect.GetName()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::StripDelimitedText
//
//  The text inside a delimited operand, with its opening and closing delimiter
//  removed. Merlin takes ANY character as the delimiter, so the pair is read off
//  the operand rather than compared against a quote set -- the same rule the
//  string directives follow, and for the same reason.
//
//  An operand that is not delimited comes back unchanged, which is what a
//  prompt-less line wants: there is nothing to strip and nothing to complain
//  about.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::StripDelimitedText (const std::string & operand)
{
    bool  isDelimited = (operand.size() >= 2) && (operand.front() == operand.back());



    return isDelimited ? operand.substr (1, operand.size() - 2) : operand;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleSegmentSwitch
//
//  Switches between CODE / DATA / BSS. Each segment keeps its own PC in
//  m_segmentPC, so the outgoing one is saved and the incoming one restored --
//  that is what lets a source alternate between segments and have each pick up
//  where it left off rather than restarting or running on from the other.
//
//  `handled` says whether this line was a segment directive at all, so
//  anything else falls through to normal assembly untouched.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleSegmentSwitch (LineInfo & info, bool & handled)
{
    HRESULT    hr    = S_OK;
    Directive  token = info.parsed.directiveToken;



    handled = false;

    BAIL_OUT_IF (!IsSegmentDirective (token), S_OK);

    // Save current PC to current segment, and the output cursor beside it --
    // a segment picks up where it left off in BOTH, or its bytes would resume
    // at the right address and the wrong place in the file.
    m_segmentPC[(int) m_currentSegment]        = m_pc;
    m_segmentOutputPos[(int) m_currentSegment] = m_outputPos;

    // The long and short spellings share a token, so each segment is one
    // comparison rather than two -- and .CODE can no longer drift from
    // .SEGMENT_CODE by being added to one list and not the other.
    if (token == Directive::SegmentCode)
    {
        m_currentSegment = Segment::Code;
    }
    else if (token == Directive::SegmentData)
    {
        m_currentSegment = Segment::Data;
    }
    else
    {
        m_currentSegment = Segment::Bss;
    }

    // Restore target segment's PC
    m_pc           = m_segmentPC[(int) m_currentSegment];
    m_outputPos    = m_segmentOutputPos[(int) m_currentSegment];
    info.pc        = m_pc;
    info.outputPos = m_outputPos;

    info.isDirective = true;
    handled = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::QualifyLocalName
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::QualifyLocalName (const std::string & scope, const std::string & name)
{
    return scope + kLocalScopeSeparator + name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::QualifyLocalReferences
//
//  Rewrites every local-label reference in one operand to the scoped name its
//  definition was recorded under.
//
//  Done as a TEXT rewrite rather than as a second symbol-lookup path, because a
//  local label is referenced from inside an expression -- `LDA :TABLE+5,X` --
//  and every consumer downstream reads that expression as text. Qualifying it
//  once here means the operand classifier, the evaluator, and the unused-label
//  sweep all see the same name without any of them learning what a local label
//  is.
//
//  The prefix is only taken as one where an identifier follows it, so a colon
//  used for anything else is left alone.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::QualifyLocalReferences (const std::string & text, char prefix,
                                                     const std::string & scope, bool & outSawLocal)
{
    std::string  result;
    size_t       i          = 0;
    bool         startsName = false;



    while (i < text.size())
    {
        startsName = (text[i] == prefix) && ((i + 1) < text.size()) &&
                     (isalpha ((unsigned char) text[i + 1]) || (text[i + 1] == '_'));

        if (startsName)
        {
            outSawLocal = true;
        }

        if (startsName && !scope.empty())
        {
            result += scope;
            result += kLocalScopeSeparator;
        }
        else
        {
            result += text[i];
        }

        i++;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ApplyLocalLabelScope
//
//  Binds this line to the local-label scope it sits in, before any stage reads
//  it.
//
//  Two things happen and their ORDER is the rule. A label that is not local
//  OPENS a new scope, and every local label and local reference after it belongs
//  to that one; references on this same line then resolve against it. Reversing
//  the two would attach a line's own references to the previous scope.
//
//  A string operand is skipped outright. Its text is payload, and one of the
//  vendor sources contains `ASC ":::6::6:6:"` -- rewriting inside it would emit
//  different bytes rather than resolve a symbol.
//
//  Does nothing at all for a dialect with no local-label prefix, which is what
//  keeps AS65 byte-identical.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ApplyLocalLabelScope (const PendingLine & current, LineInfo & info)
{
    HRESULT       hr         = S_OK;
    ParsedLine &  parsed     = info.parsed;
    char          prefix     = m_dialect.GetLocalLabelPrefix();
    bool          isPrivate  = m_dialect.GetMacroSyntax().labelsArePerExpansion && (current.macroDepth > 0);
    bool          isLocal    = false;
    bool          isPayload  = false;
    bool          sawLocal   = false;



    BAIL_OUT_IF (prefix == 0, S_OK);

    isLocal = !parsed.label.empty() && (parsed.label[0] == prefix);

    // A label a macro expansion produced does NOT open a scope. It is private to
    // the expansion -- renamed per invocation, so no source can name it -- and
    // letting it become the enclosing global would re-scope every local after
    // the call site. The vendor sources prove it matters: `MAKE DUMP.S` calls
    // macros defining `LP` and `ND` in the middle of a routine whose own locals
    // belong to a global label further up, and each call would otherwise strand
    // the locals that follow it.
    // Nor does a REASSIGNABLE one. The same name is defined over and over -- the
    // whole reason a dialect offers the form -- so treating each as a new scope
    // would put successive locals under names that differ only by which
    // definition happened to come last. CLOCK.S has `INCTIME` open a scope,
    // `]LOOP` follow immediately, and `:OUT` further down belong to `INCTIME`.
    if (!isLocal && !parsed.label.empty() && !isPrivate && (parsed.labelKind == SymbolKind::Label))
    {
        m_localLabelScope = parsed.label;
    }

    isPayload = (parsed.directiveToken == Directive::StringData);

    if (!isPayload)
    {
        parsed.operand      = QualifyLocalReferences (parsed.operand,      prefix, m_localLabelScope, sawLocal);
        parsed.directiveArg = QualifyLocalReferences (parsed.directiveArg, prefix, m_localLabelScope, sawLocal);
        parsed.constantExpr = QualifyLocalReferences (parsed.constantExpr, prefix, m_localLabelScope, sawLocal);
    }

    if ((isLocal || sawLocal) && m_localLabelScope.empty())
    {
        RecordError (current.sourceLineNumber,
                     "Local label used before any global label: " + (isLocal ? parsed.label : parsed.operand));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordLabel
//
//  Binds a label to `address` in pass 1, so pass 2 can resolve forward
//  references to it. Three tables are written together and must stay in step:
//  m_symbols (the Word address), m_symbolKinds (Label vs Equ vs Set, which is
//  what lets .SET be redefined and a label not), and m_exprSymbols (the
//  int32_t view the expression evaluator reads).
//
//  A duplicate is an error rather than a silent rebind: the second definition
//  would move every forward reference already resolved against the first, so
//  the output would depend on which pass saw which.
//
//  A label that differs from a mnemonic only by case -- `lda:` against LDA --
//  is a warning, not an error. It is legal and occasionally deliberate, but
//  far more often a typo that would otherwise assemble into something silently
//  wrong.
//
//  The address is passed in rather than read from m_pc because a label sharing
//  a line with an origin directive binds to the OUTPUT CURSOR, not to the
//  relocated program counter. Merlin's `HEREINT ORG INTRFACE` is the case: the
//  loader that copies the interface section to page 3 needs to know where the
//  section sits in the file it was loaded from, so binding the label to $0300
//  would be silently, plausibly wrong.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RecordLabel (const PendingLine & current, LineInfo & info, Word address)
{
    HRESULT      hr           = S_OK;
    char         prefix       = m_dialect.GetLocalLabelPrefix();
    bool         isLocal      = false;
    bool         isRebindable = (info.parsed.labelKind == SymbolKind::Set);
    std::string  spelled;
    std::string  stored;



    // Most lines carry no label; that is not a failure, just nothing to record.
    BAIL_OUT_IF (info.parsed.label.empty(), S_OK);

    if (isRebindable)
    {
        hr = RecordRebindableLabel (current, info, address);
        CHR (hr);
    }

    BAIL_OUT_IF (isRebindable, S_OK);

    // A local label is validated as the name it SPELLS and stored under the name
    // it BINDS to. Validating the joined name instead would reject every one of
    // them, since the separator is deliberately a character no label may hold.
    isLocal = (prefix != 0) && (info.parsed.label[0] == prefix);
    spelled = isLocal ? info.parsed.label.substr (1) : info.parsed.label;
    stored  = isLocal ? QualifyLocalName (m_localLabelScope, spelled) : info.parsed.label;

    // A local with no scope to belong to was already reported where the scope is
    // known; binding it to a name beginning with the separator would only
    // produce a second, stranger diagnostic later.
    BAIL_OUT_IF (isLocal && m_localLabelScope.empty(), S_OK);

    {
        std::string labelError;
        HRESULT     hrLabel = Parser::ValidateLabel (spelled, *m_opcodeTable, labelError,
                                                     m_dialect.GetExtraSymbolCharacters());

        if (FAILED (hrLabel))
        {
            RecordErrorAt (current.sourceLineNumber, info.parsed.labelColumn, labelError);
        }
        else if (m_symbols.count (stored) > 0)
        {
            RecordErrorAt (current.sourceLineNumber, info.parsed.labelColumn,
                           "Duplicate label: " + info.parsed.label);
        }
        else
        {
            std::string  upper;

            m_symbols[stored]     = address;
            m_symbolKinds[stored] = SymbolKind::Label;
            m_exprSymbols[stored] = (int32_t) address;

            // Warn if label resembles mnemonic by case
            upper = ToUpperCase (spelled);

            if (upper != spelled && m_opcodeTable->IsMnemonic (upper))
            {
                RecordWarning (current.sourceLineNumber, "Label name resembles mnemonic: " + info.parsed.label);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordRebindableLabel
//
//  Binds a label whose dialect allows the same name again further down.
//
//  Redefinition is the POINT of the form rather than something tolerated, so
//  there is no duplicate check -- the second definition replaces the first and
//  every reference between them kept the value it saw. What is still an error is
//  colliding with a name bound some other way: a label or an equate is immutable,
//  and rebinding one would move references already resolved against it. That is
//  the same rule a reassignable constant follows, and it is the same table that
//  records which kind a name is.
//
//  The name is NOT validated the way an ordinary label is. It is the profile's
//  construction rather than the source's spelling, exactly like a constant's
//  name -- the source wrote a sigil the shared label rules reject, and the
//  profile has already turned it into something the expression tokenizer can lex.
//
//  THE COLLISION CHECK IS UNREACHABLE TODAY and is recorded rather than removed.
//  Only a dialect's variable forms produce a name in this namespace, and both of
//  them -- the assignment and this -- bind it as reassignable, so nothing can
//  make one immutable first. It stays because it is what keeps that true by
//  construction: a dialect adding a third form gets the diagnostic rather than a
//  silent rebind. Verified by mutation, and nothing caught it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RecordRebindableLabel (const PendingLine & current, LineInfo & info, Word address)
{
    HRESULT              hr        = S_OK;
    const std::string &  name      = info.parsed.label;
    auto                 kindIt    = m_symbolKinds.find (name);
    bool                 isTaken   = (kindIt != m_symbolKinds.end()) && (kindIt->second != SymbolKind::Set);



    if (isTaken)
    {
        RecordError (current.sourceLineNumber,
            "Cannot redefine " + name + " (was defined as immutable)");
    }

    BAIL_OUT_IF (isTaken, S_OK);

    m_symbols[name]     = address;
    m_symbolKinds[name] = SymbolKind::Set;
    m_exprSymbols[name] = (int32_t) address;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleConstantDefinition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleConstantDefinition (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    info.isConstant = true;
    m_pass1Ctx.currentPC = (int32_t) m_pc;

    if (info.parsed.constantKind == SymbolKind::Set)
    {
        hr = HandleSetConstant (current, info);
        CHR (hr);
    }
    else
    {
        hr = HandleEquConstant (current, info);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleSetConstant
//
//  .SET defines a MUTABLE symbol -- that is the whole difference from .EQU,
//  and it is what SymbolKind exists to record. Redefining is allowed only when
//  the existing symbol is itself a Set; a label or an .EQU is immutable and
//  rebinding it would move references already resolved against the old value.
//
//  The expression is evaluated at each definition, so a .SET inside a repeated
//  block takes the value current at that point rather than the first one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleSetConstant (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    {
        ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, m_pass1Ctx);

        if (!er.success)
        {
            RecordErrorAt (current.sourceLineNumber, info.parsed.operandColumn,
                           "Cannot evaluate constant expression: " + er.error);
        }
        else
        {
            auto kindIt = m_symbolKinds.find (info.parsed.constantName);

            if (kindIt != m_symbolKinds.end() && kindIt->second != SymbolKind::Set)
            {
                RecordError (current.sourceLineNumber,
                    "Cannot redefine " + info.parsed.constantName + " (was defined as immutable)");
            }
            else
            {
                m_symbols[info.parsed.constantName]     = (Word) er.value;
                m_symbolKinds[info.parsed.constantName] = SymbolKind::Set;
                m_exprSymbols[info.parsed.constantName] = er.value;
            }
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleEquConstant
//
//  .EQU defines an IMMUTABLE symbol, so any prior definition is an error --
//  reported differently depending on whether it was another .EQU (a plain
//  duplicate) or a label / .SET (a kind clash), because those are different
//  mistakes with different fixes.
//
//  A quoted string binds to its LENGTH, not its contents. That is what makes
//  `MSG_LEN equ "hello"` work as a length constant without a separate
//  directive, and it is why the string case is handled before evaluation --
//  the expression evaluator has no notion of a string literal.
//
//  The kind is recorded BEFORE the value is known, so a self-referential or
//  unresolvable .EQU is still immutable: a later attempt to redefine it fails
//  as a redefinition rather than quietly succeeding because the first one had
//  no value. An expression that fails to evaluate here simply leaves the
//  symbol valueless, and ResolveEquConstants reports it after pass 1, by which
//  point forward references it depends on may have been defined.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleEquConstant (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    {
        auto kindIt = m_symbolKinds.find (info.parsed.constantName);

        if (kindIt != m_symbolKinds.end())
        {
            if (kindIt->second == SymbolKind::Equ)
            {
                RecordError (current.sourceLineNumber,
                    "Duplicate equ definition: " + info.parsed.constantName);
            }
            else
            {
                RecordError (current.sourceLineNumber,
                    "Cannot redefine " + info.parsed.constantName + " as equ (already defined as different kind)");
            }
        }
        else
        {
            const std::string & expr = info.parsed.constantExpr;

            m_symbolKinds[info.parsed.constantName] = SymbolKind::Equ;

            if (expr.size() >= 2 && expr.front() == '"' && expr.back() == '"')
            {
                int32_t len = (int32_t) (expr.size() - 2);
                m_symbols[info.parsed.constantName]     = (Word) len;
                m_exprSymbols[info.parsed.constantName] = len;
            }
            else
            {
                ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, m_pass1Ctx);

                if (er.success)
                {
                    m_symbols[info.parsed.constantName]     = (Word) er.value;
                    m_exprSymbols[info.parsed.constantName] = er.value;
                }
            }
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Word
//
//  Two bytes per argument. Pass 2 evaluates them; pass 1 only needs the size.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Word (const PendingLine & /*current*/, LineInfo & info)
{
    std::vector<std::string>  args = Parser::SplitArgList (info.parsed.directiveArg);



    ReserveBytes ((Word) (args.size() * 2));

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Text
//
//  One byte per character of the quoted string, before character mapping --
//  .CMAP substitutes bytes one-for-one, so the length is the same either way.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Text (const PendingLine & /*current*/, LineInfo & info)
{
    std::string  text = Parser::ParseQuotedString (info.parsed.directiveArg);



    ReserveBytes ((Word) text.size());

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::TryEncodeStringOperand
//
//  One encoded-string operand into bytes.
//
//  The delimiter is whatever character opens the text -- ANY character, not a
//  fixed quote set -- and it also decides whether the text carries the high bit,
//  so it is read before the payload rather than skipped past.
//
//  Data after the closing delimiter (`ASC "TEXT"8D8D`) is a run of RAW
//  HEXADECIMAL BYTE PAIRS, appended verbatim after the encoded text.
//
//  Settled against the shipped object rather than reasoned about. `MAKE DUMP`
//  carries `ASC "This destroys current source."8D8D` followed by
//  `ASC "Do you really want it (Y/N)? "00`, and its object holds the high-ASCII
//  text with `8D 8D` and then `00` immediately after -- so the digits are
//  hexadecimal, two per byte, and the bytes are NOT forced to the delimiter's
//  high-bit convention the way the text is. `00` staying `00` is what proves the
//  second half; a high-bit rule would have made it `80`.
//
//  Every one of the 14 such lines across the vendor sources is a bare digit run
//  with no separator, so no comma form is accepted here. Adding one would be
//  guessing at a spelling the corpus does not contain.
//
//  UNVERIFIED: whether a trailing run after DCI counts toward the terminator.
//  Every trailing-run line on the disk is ASC, so nothing pins it. The text is
//  encoded first and the run appended after, which leaves DCI's inversion on the
//  last character of the TEXT.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::TryEncodeStringOperand (const ParsedLine & parsed, std::vector<Byte> & outBytes,
                                              std::string & outError)
{
    const std::string &  operand   = parsed.directiveArg;
    std::string          extra;
    char                 delimiter = 0;
    size_t               closing   = std::string::npos;
    bool                 hasText   = (operand.size() >= 2);
    bool                 closed    = false;
    bool                 encoded   = false;



    if (!hasText)
    {
        outError = "String directive needs delimited text";
    }
    else
    {
        delimiter = operand[0];
        closing   = operand.find (delimiter, 1);
        closed    = (closing != std::string::npos);

        if (!closed)
        {
            outError = std::string ("Unterminated string: no closing ") + delimiter;
        }
        else
        {
            extra = operand.substr (closing + 1);

            StringEncoding::Encode (operand.substr (1, closing - 1),
                                    parsed.stringMode,
                                    StringEncoding::HighBitFromDelimiter (delimiter),
                                    outBytes);

            encoded = TryParseHexBytes (extra, outBytes);

            if (!encoded)
            {
                outError = "Trailing data after a string operand must be whole hexadecimal bytes: " + extra;
                outBytes.clear();
            }
        }
    }

    return encoded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::TryParseHexBytes
//
//  A run of hexadecimal digit pairs APPENDED to whatever the caller already
//  has, so one call can extend an encoded string rather than replace it.
//
//  An odd number of digits is refused rather than padded. A half byte means the
//  source says something the reader cannot resolve, and both plausible repairs
//  -- pad the front, pad the back -- change every byte after it.
//
//  Empty text is a success that appends nothing, which is what makes this
//  usable for the common case of no trailing data at all.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::TryParseHexBytes (const std::string & text, std::vector<Byte> & outBytes)
{
    size_t  pos    = 0;
    bool    parsed = ((text.size() % 2) == 0);



    while (parsed && (pos < text.size()))
    {
        int  value = HexByte (text, pos);

        if (value < 0)
        {
            parsed = false;
            break;
        }

        outBytes.push_back ((Byte) value);
        pos += 2;
    }

    return parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::TryEncodeHexOperand
//
//  One raw-hexadecimal directive into bytes.
//
//  The digits are the bytes: no expression is evaluated and no value is
//  range-checked, which is the whole point of the directive -- it is how a
//  source writes data the assembler has no other way to spell.
//
//  Separators are removed before parsing rather than parsed as structure.
//  Every one of the nine occurrences across the vendor sources is an unbroken
//  digit run, so the comma form is UNVERIFIED here and accepted only because
//  the directive documents it and refusing it could only cost a user source
//  that assembles elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::TryEncodeHexOperand (const ParsedLine & parsed, std::vector<Byte> & outBytes,
                                           std::string & outError)
{
    std::string  digits;
    bool         encoded = false;



    for (char ch : parsed.directiveArg)
    {
        if ((ch != ',') && (ch != ' ') && (ch != '\t'))
        {
            digits += ch;
        }
    }

    if (digits.empty())
    {
        outError = "Hexadecimal data directive needs at least one byte";
    }
    else
    {
        encoded = TryParseHexBytes (digits, outBytes);

        if (!encoded)
        {
            outError = "Hexadecimal data must be whole bytes of hexadecimal digits: " + parsed.directiveArg;
            outBytes.clear();
        }
    }

    return encoded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Hex
//
//  Sized by running the encoder and measuring, so the two passes cannot
//  disagree about how many bytes the line occupies.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Hex (const PendingLine & current, LineInfo & info)
{
    std::vector<Byte>  bytes;
    std::string        error;
    bool               encoded = TryEncodeHexOperand (info.parsed, bytes, error);



    if (!encoded)
    {
        RecordError (current.sourceLineNumber, error);
    }

    ReserveBytes ((Word) bytes.size());

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitHexDirective
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitHexDirective (const LineInfo & info, Word & emitPC)
{
    std::vector<Byte>  bytes;
    std::string        error;
    bool               encoded = TryEncodeHexOperand (info.parsed, bytes, error);



    IGNORE_RETURN_VALUE (encoded, false);

    for (Byte value : bytes)
    {
        EmitByte (value, emitPC);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1String
//
//  Sizes an encoded-string directive by ENCODING it and measuring the result,
//  rather than by counting characters. The encodings differ in what they add at
//  the ends -- a length prefix, or nothing -- so a character count is right for
//  some modes and quietly wrong for others.
//
//  The operand text is all the encoding depends on, so this cannot disagree with
//  pass 2 over a forward reference: there are none to have.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1String (const PendingLine & current, LineInfo & info)
{
    std::vector<Byte>  bytes;
    std::string        error;
    bool               encoded = TryEncodeStringOperand (info.parsed, bytes, error);



    if (!encoded)
    {
        RecordError (current.sourceLineNumber, error);
    }

    ReserveBytes ((Word) bytes.size());

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Dd
//
//  Four bytes per argument.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Dd (const PendingLine & /*current*/, LineInfo & info)
{
    std::vector<std::string>  args = Parser::SplitArgList (info.parsed.directiveArg);



    ReserveBytes ((Word) (args.size() * 4));

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Ds
//
//  Reserves storage. The size must resolve in pass 1 because every later
//  address depends on it, so an unresolvable expression is an error here
//  rather than something pass 2 could still fix up.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Ds (const PendingLine & current, LineInfo & info)
{
    std::vector<std::string>  args;
    ExprResult                er;



    m_pass1Ctx.currentPC = (int32_t) m_pc;
    args                 = Parser::SplitArgList (info.parsed.directiveArg);

    if (!args.empty())
    {
        er = ExpressionEvaluator::Evaluate (args[0], m_pass1Ctx);

        if (!er.success)
        {
            RecordErrorAt (current.sourceLineNumber, info.parsed.operandColumn,
                           info.parsed.directive + " size must be resolvable: " + er.error);
        }
        else
        {
            ReserveBytes ((Word) er.value);
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Align
//
//  Advances the PC to the next multiple of the alignment, defaulting to 2.
//  Like .DS this has to resolve in pass 1, for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Align (const PendingLine & current, LineInfo & info)
{
    int         alignment = 2;
    int         overshoot = 0;
    ExprResult  er;



    m_pass1Ctx.currentPC = (int32_t) m_pc;

    if (!info.parsed.directiveArg.empty())
    {
        er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass1Ctx);

        if (!er.success)
        {
            RecordError (current.sourceLineNumber, ".align expression must be resolvable: " + er.error);
        }
        else
        {
            alignment = er.value;
        }
    }

    if (alignment > 0)
    {
        overshoot = m_pc % alignment;

        if (overshoot != 0)
        {
            ReserveBytes ((Word) (alignment - overshoot));
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1End
//
//  Stops assembly at this line; the rest of the source is not processed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1End (const PendingLine & /*current*/, LineInfo & /*info*/)
{
    m_endAssembly = true;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Error
//
//  Records a user-authored diagnostic. An unquoted argument is taken
//  verbatim, so `.error out of space` reads naturally.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Error (const PendingLine & current, LineInfo & info)
{
    std::string  msg = Parser::ParseQuotedString (info.parsed.directiveArg);



    if (msg.empty() && !info.parsed.directiveArg.empty())
    {
        msg = info.parsed.directiveArg;
    }

    RecordError (current.sourceLineNumber, msg.empty() ? "User error directive" : msg);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1List
//
//  Listing output nests, so this is a depth counter rather than a flag.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1List (const PendingLine & /*current*/, LineInfo & /*info*/)
{
    m_listingLevel++;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Nolist
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Nolist (const PendingLine & /*current*/, LineInfo & /*info*/)
{
    m_listingLevel--;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Title
//
//  Sets the listing title. Unquoted arguments are taken verbatim, as .ERROR.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Title (const PendingLine & /*current*/, LineInfo & info)
{
    m_result.listingTitle = Parser::ParseQuotedString (info.parsed.directiveArg);

    if (m_result.listingTitle.empty() && !info.parsed.directiveArg.empty())
    {
        m_result.listingTitle = info.parsed.directiveArg;
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Loop
//
//  Opens a repeat block: everything up to the terminator is collected, and the
//  terminator requeues the collection once per iteration.
//
//  The count is settled HERE, in pass 1, and it has to be: the block becomes
//  lines while pass 1 is still walking, so an expression naming a label defined
//  further down has no answer yet and never will. That is a property of what
//  expansion is rather than a limitation worth working around -- pass 2 sees the
//  expanded lines, not the block.
//
//  The collecting state is entered even when the count is refused, so the
//  terminator still closes something. Bailing out instead would leave the body
//  assembling once as ordinary lines and the terminator reported as an orphan,
//  which buries one clear diagnostic under two misleading ones.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Loop (const PendingLine & current, LineInfo & info)
{
    HRESULT     hr        = S_OK;
    bool        inRange   = false;
    ExprResult  er;



    m_pass1Ctx.currentPC = (int32_t) m_pc;
    er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass1Ctx);

    m_loopBody.clear();
    m_loopCount     = 0;
    m_loopNesting   = 0;
    m_loopLine      = current.sourceLineNumber;
    m_loopColumn    = m_diagnosticColumn;
    m_loopFile      = current.sourceFile;
    m_loopDirective = info.parsed.directive;
    m_pass1State    = Pass1State::CollectingLoop;

    inRange = er.success && (er.value >= 0) && (er.value <= kMaxLoopIterations);

    if (!er.success)
    {
        RecordErrorAt (current.sourceLineNumber, info.parsed.operandColumn,
                       info.parsed.directive + " repeat count must be resolvable: " + er.error);
    }
    else if (!inRange)
    {
        RecordErrorAt (current.sourceLineNumber, info.parsed.operandColumn,
                       info.parsed.directive + " repeat count must be 0 to "
                       + std::to_string (kMaxLoopIterations));
    }
    else
    {
        m_loopCount = er.value;
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CollectLoopBody
//
//  One line swallowed by an open repeat block.
//
//  A nested block is collected as ORDINARY BODY TEXT, counted only so the right
//  terminator closes the outer one. Every copy of the outer body then meets the
//  inner opening directive again and expands it afresh, which is what makes
//  nesting cost a counter rather than a stack.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::CollectLoopBody (const PendingLine & current, LineInfo & info)
{
    HRESULT  hr     = S_OK;
    bool     opens  = (info.parsed.directiveToken == Directive::Loop);
    bool     closes = (info.parsed.directiveToken == Directive::LoopEnd);



    if (closes && (m_loopNesting == 0))
    {
        ExpandCollectedLoop();
    }
    else
    {
        if (opens)
        {
            m_loopNesting++;
        }

        if (closes)
        {
            m_loopNesting--;
        }

        m_loopBody.push_back (current);
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ExpandCollectedLoop
//
//  The collected body, requeued once per iteration.
//
//  Lines go to the FRONT of the pending queue in reverse, which is how macro
//  expansion already puts generated lines back in source order -- the queue is
//  consumed from the front. Each copy carries the line number and file its
//  original was written in, so a diagnostic raised in the last iteration points
//  at the one line the author actually wrote.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::ExpandCollectedLoop()
{
    m_pass1State = Pass1State::Normal;

    for (int iteration = 0; iteration < m_loopCount; iteration++)
    {
        for (size_t index = m_loopBody.size(); index > 0; index--)
        {
            m_pendingLines.push_front (m_loopBody[index - 1]);
        }
    }

    m_loopBody.clear();

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1LoopEnd
//
//  A loop terminator that closes nothing.
//
//  Reachable only OUTSIDE a repeat block: while one is open the collecting
//  state claims the line before dispatch ever runs. So there is exactly one
//  thing this can mean, and saying it is the whole job -- a terminator with no
//  opening directive was previously dropped without a word, which is the
//  silently-ignored shape this dialect's directives were built to avoid.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1LoopEnd (const PendingLine & current, LineInfo & info)
{
    RecordError (current.sourceLineNumber,
                 info.parsed.directive + " has no loop to close");

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1DummySection
//
//  Opens a section that assigns addresses and emits nothing.
//
//  This is how a source describes a memory layout it does not own -- a page of
//  zero-page variables, a block of scratch -- and gets names for every field
//  without a byte of it appearing in the object. Every line inside is sized
//  exactly as it would be anywhere else, so a label lands where the layout says;
//  what changes is only that the output cursor stays where it was.
//
//  A section inside a section is refused rather than nested. There is one place
//  to return to, and a second entry would overwrite it -- so the first section's
//  end would restore the second's origin and every byte after it would be placed
//  somewhere nobody asked for, silently.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1DummySection (const PendingLine & current, LineInfo & info)
{
    HRESULT     hr       = S_OK;
    bool        isNested = m_inDummySection;
    ExprResult  er;



    if (isNested)
    {
        RecordError (current.sourceLineNumber,
                     info.parsed.directive + " is already open, from line " + std::to_string (m_dummyLine));
    }

    BAIL_OUT_IF (isNested, S_OK);

    m_pass1Ctx.currentPC = (int32_t) m_pc;
    er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass1Ctx);

    if (!er.success)
    {
        RecordErrorAt (current.sourceLineNumber, info.parsed.operandColumn,
                       info.parsed.directive + " expression must be resolvable: " + er.error);
    }

    BAIL_OUT_IF (!er.success, S_OK);

    m_dummyReturnPC     = m_pc;
    m_dummyReturnOutput = m_outputPos;
    m_dummyLine         = current.sourceLineNumber;
    m_dummyColumn       = m_diagnosticColumn;
    m_dummyFile         = current.sourceFile;
    m_dummyDirective    = info.parsed.directive;
    m_inDummySection    = true;

    m_pc                = (Word) er.value;
    info.pc             = m_pc;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1DummySectionEnd
//
//  Closes a dummy section, putting both cursors back where the section was
//  entered from.
//
//  A terminator with no section open is reported rather than ignored: it means
//  either an opening directive that never assembled -- inside a conditional the
//  author did not expect to be false, typically -- or a stray line, and both
//  leave the reader with a file whose addresses are not what they look like.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1DummySectionEnd (const PendingLine & current, LineInfo & info)
{
    HRESULT  hr     = S_OK;
    bool     isOpen = m_inDummySection;



    if (!isOpen)
    {
        RecordError (current.sourceLineNumber,
                     info.parsed.directive + " has no dummy section to close");
    }

    BAIL_OUT_IF (!isOpen, S_OK);

    m_pc             = m_dummyReturnPC;
    m_outputPos      = m_dummyReturnOutput;
    m_inDummySection = false;

    info.pc          = m_pc;
    info.outputPos   = m_outputPos;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1CpuSelect
//
//  A dialect that selects its CPU in the source, doing it.
//
//  From here to the end of the assembly the wider instruction table answers
//  every lookup. Recorded per line as it goes -- see LineInfo::usedExtendedSet
//  -- rather than replayed by pass 2, because conditional assembly can put this
//  line inside a block the two passes need not agree about.
//
//  ASKING FIRST is obligatory, and this is the caller InstructionSetProvider's
//  comment names. A provider with nothing to switch to answers with the base
//  table, so switching against it would leave the source told it had reached a
//  wider processor and the assembler quietly on the narrow one. Saying so is
//  the only honest answer available: the tables are injected, so this layer
//  cannot go and find one.
//
//  AN OPERAND IS ACCEPTED AND IGNORED, and the reasoning that once refused one
//  is kept here because it was right for as long as the answer was unknown.
//  Whether this directive has a form that puts the CPU BACK was an open question
//  about the language, and refusing the operand was the only reading that
//  answered nothing: ignoring it would have said "there is no such form, and
//  writing one selects the wider processor anyway", silently.
//
//  It is no longer open. Assembled under the real assembler, an operand draws no
//  diagnostic and selects nothing -- the wider set stays selected, proven by an
//  instruction only the wider processor has still assembling on the line after
//  it. So the transition really is one-way, and the refusal became the thing
//  rejecting source the real assembler accepts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1CpuSelect (const PendingLine & current, LineInfo & info)
{
    HRESULT  hr           = S_OK;
    bool     canSelect    = m_instructionSets.HasExtended();



    if (!canSelect)
    {
        RecordError (current.sourceLineNumber,
                     info.parsed.directive + " selects a wider instruction set, and this assembly was"
                     " given only one instruction set to work from");
    }

    BAIL_OUT_IF (!canSelect, S_OK);

    m_extendedActive = true;
    m_opcodeTable    = &m_instructionSets.GetExtended();

    // Reported so the caller can say WHICH instruction set the assembly ran on
    // and what chose it. Nothing outside the source knows: the directive may sit
    // inside a conditional, so an invocation that passed no CPU flag cannot tell
    // "the default stood" from "the source selected the wider set" without being
    // told, and those two read identically in an empty report.
    m_result.extendedSetSelectedInSource = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1SaveObject
//
//  The save directive, in the pass that only sizes lines.
//
//  It places no bytes and cuts nothing here; both of those happen in pass 2,
//  where the byte stream exists. What pass 1 owes it is the check that it names
//  something, so a source missing the name is told at the line that is missing
//  it rather than at whatever the assembly did afterwards.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1SaveObject (const PendingLine & current, LineInfo & info)
{
    HRESULT      hr     = S_OK;
    std::string  name   = StripCommentAndTrim (info.parsed.directiveArg);
    bool         hasArg = !name.empty();



    //  A NAME IS REQUIRED, and is not taken from anywhere else when it is
    //  missing. This directive exists to write a named output, and falling back
    //  to whatever name happened to be in effect is what would let several
    //  saves in one source resolve to one file, each writing over the last
    //  while the assembly reported success. The output-file directive beside it
    //  is already an error with no operand, for the same reason.
    if (!hasArg)
    {
        RecordError (current.sourceLineNumber, info.parsed.directive + " names no output file");
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1FileType
//
//  The source stating the filesystem type its output should take.
//
//  REPORTED RATHER THAN ACTED ON, like the output name beside it. Nothing here
//  knows what a filesystem is; this records the byte the source asked for and
//  leaves deciding whether the target has such a type to whoever writes the
//  file.
//
//  A value that will not fit in a byte is refused rather than truncated: the
//  low byte of a wrong answer is still a wrong answer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1FileType (const PendingLine & current, LineInfo & info)
{
    HRESULT      hr     = S_OK;
    std::string  arg    = StripCommentAndTrim (info.parsed.directiveArg);
    bool         hasArg = !arg.empty();



    //  Only that it names something. WHICH type it names is settled in pass 2,
    //  where the type has to take effect at the point in the byte stream the
    //  line sits at rather than for the whole assembly.
    if (!hasArg)
    {
        RecordError (current.sourceLineNumber, info.parsed.directive + " names no file type");
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitFileType
//
//  The filesystem type, taking effect where the line sits.
//
//  IN PASS 2 BECAUSE THE TYPE BELONGS TO AN OUTPUT rather than to the assembly.
//  Setting it in pass 1 gives every output the last type the source stated,
//  which is wrong the moment a source produces more than one: a type named
//  before the second save would reach back and retype the first as well.
//
//  A value that will not fit in a byte is refused rather than truncated. The
//  low byte of a wrong answer is still a wrong answer, and it would file the
//  object under a type nobody asked for.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitFileType (const LineInfo & info, Word & emitPC)
{
    HRESULT      hr      = S_OK;
    std::string  arg     = StripCommentAndTrim (info.parsed.directiveArg);
    bool         hasArg  = !arg.empty();
    bool         inRange = false;
    ExprResult   er;



    (void) emitPC;

    BAIL_OUT_IF (!hasArg, S_OK);

    m_pass2Ctx.currentPC = (int32_t) info.pc;
    er                   = ExpressionEvaluator::Evaluate (arg, m_pass2Ctx);

    if (!er.success)
    {
        RecordErrorAt (m_lastSourceLine, info.parsed.operandColumn,
                       info.parsed.directive + " expression must be resolvable: " + er.error);
    }

    BAIL_OUT_IF (!er.success, S_OK);

    inRange = (er.value >= 0) && (er.value <= 0xFF);

    if (!inRange)
    {
        RecordErrorAt (m_lastSourceLine, info.parsed.operandColumn,
                       info.parsed.directive + " takes a one-byte file type");
    }

    BAIL_OUT_IF (!inRange, S_OK);

    m_fileType    = (Byte) er.value;
    m_hasFileType = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1ObjectFile
//
//  The source naming its own output.
//
//  THE CALLER WINS. A name supplied from outside the source is honored and this
//  directive is then recorded only as what the source would have asked for, so
//  a build script can point one source at several outputs without editing it.
//  The precedence is settled in Initialize, which seeds the result with the
//  caller's name; all this has to do is not overwrite one.
//
//  A later directive replaces an earlier one, the way a later origin does. The
//  name in effect is the last one the source stated, which is the only reading
//  that does not require knowing how many are above it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1ObjectFile (const PendingLine & current, LineInfo & info)
{
    HRESULT      hr        = S_OK;
    std::string  name      = StripCommentAndTrim (info.parsed.directiveArg);
    bool         hasName   = !name.empty();
    bool         callerSet = !m_options.outputFileName.empty();



    if (!hasName)
    {
        RecordError (current.sourceLineNumber, info.parsed.directive + " names no output file");
    }

    BAIL_OUT_IF (!hasName, S_OK);
    BAIL_OUT_IF (callerSet, S_OK);

    m_result.outputFileName = name;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::BindPositionalParameters
//
//  One parameter-binding line, in whichever pass is asking.
//
//  The values are split on the MACRO ARGUMENT separator, because these are the
//  same parameters an invocation supplies and a source writing them two ways
//  would be two rules for one form. The name each binds under is the profile's,
//  so a binding and a reference to it cannot disagree about what the symbol is
//  called.
//
//  They bind as REASSIGNABLE, which is the whole point: a fragment may be
//  included twice under different bindings, and an immutable symbol would report
//  the second as a duplicate of the first.
//
//  EACH DIAGNOSTIC BELONGS TO EXACTLY ONE PASS, because this line is visited by
//  both and anything said in both is said twice. How the line is WRITTEN is
//  settled in pass 1, which is the first pass that can answer it. Whether an
//  expression resolves is settled in pass 2, which is the first pass that can:
//  the vendor's own use binds a label defined BELOW the fragment it
//  parameterizes, so refusing a forward reference in pass 1 would reject the one
//  shape the directive exists for.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::BindPositionalParameters (int                 lineNumber,
                                                   const ParsedLine &  parsed,
                                                   ExprContext      &  ctx,
                                                   bool                isFinalPass)
{
    HRESULT                   hr        = S_OK;
    MacroSyntax               syntax    = m_dialect.GetMacroSyntax();
    std::vector<std::string>  values    = Parser::SplitOnSeparator (parsed.directiveArg, syntax.argumentSeparator);
    size_t                    count     = values.size();
    bool                      hasValues = (count > 0);
    bool                      fitsForm  = (count <= (size_t) kMaxPositionalParams);



    if (!hasValues && !isFinalPass)
    {
        RecordError (lineNumber, parsed.directive + " binds no parameters. Values are separated by '"
                                 + std::string (1, syntax.argumentSeparator) + "'");
    }

    BAIL_OUT_IF (!hasValues, S_OK);

    // More values than the positional form can name. The extras would bind to
    // symbols no source can spell, which is silently assembling a program whose
    // author believed every value was in use.
    if (!fitsForm && !isFinalPass)
    {
        RecordError (lineNumber, parsed.directive + " binds " + std::to_string (count)
                                 + " parameters, and only " + std::to_string (kMaxPositionalParams)
                                 + " can be referred to");
    }

    BAIL_OUT_IF (!fitsForm, S_OK);

    for (size_t i = 0; i < count; i++)
    {
        std::string  name    = m_dialect.GetPositionalParameterSymbol ((int) i + 1);
        ExprResult   er      = {};
        bool         isNamed = !name.empty();

        // A profile claiming this directive and naming no symbol for a parameter
        // would bind nothing while reporting success, so it fails loudly here
        // rather than leaving every reference undefined for no stated reason.
        CBRA (isNamed);

        er = ExpressionEvaluator::Evaluate (values[i], ctx);

        if (er.success)
        {
            m_symbols[name]     = (Word) er.value;
            m_symbolKinds[name] = SymbolKind::Set;
            m_exprSymbols[name] = er.value;

            if (isFinalPass)
            {
                m_fullSymbols[name] = er.value;
            }
        }
        else if (isFinalPass)
        {
            RecordErrorAt (lineNumber, parsed.operandColumn,
                           "Cannot evaluate parameter binding: " + er.error);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1ParameterBinding
//
//  Pass 1 binds so the lines below are SIZED against the values in effect --
//  a parameter naming a zero-page address makes the instruction beside it two
//  bytes rather than three, and getting that wrong moves every label after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1ParameterBinding (const PendingLine & current, LineInfo & info)
{
    HRESULT  hr = S_OK;



    m_pass1Ctx.currentPC = (int32_t) m_pc;

    hr = BindPositionalParameters (current.sourceLineNumber, info.parsed, m_pass1Ctx, false);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RebindParameters
//
//  And pass 2 binds again, where the line is reached, for the reason
//  RebindMutableConstant does: pass 2 reads one table built after pass 1
//  finished, so without this every reference resolves to the LAST binding the
//  file made rather than to the one above it.
//
//  It emits nothing, which is why the cursor is untouched. The pass-2 column is
//  the only hook that runs in source order, and the assembly-time assertion
//  already uses it for an action rather than for bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RebindParameters (const LineInfo & info, Word & /*emitPC*/)
{
    HRESULT  hr = S_OK;



    m_pass2Ctx.currentPC = (int32_t) info.pc;

    hr = BindPositionalParameters (info.parsed.lineNumber, info.parsed, m_pass2Ctx, true);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::IgnorePass1Directive
//
//  Recognized and deliberately does nothing: .OPT_NOOP is accepted only for
//  as65 source compatibility, and .PAGE acts at listing time. They still need
//  a non-null row so the dispatch reports them as handled rather than as an
//  unknown directive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::IgnorePass1Directive (const PendingLine & /*current*/, LineInfo & /*info*/)
{
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1OptimizeControl
//
//  OPT turns the emitted-code optimizations on, NOOPT turns them off, and the
//  command line's own switch beats both.
//
//  ONE handler for the two spellings, reading the token the line already
//  carries. Two would be one piece of state written from two places, and the
//  interesting rule -- that the switch is permanent -- would then have to be
//  repeated in the one that can break it.
//
//  Acting in PASS 1 is the whole point: the substitution it governs decides how
//  many bytes a line occupies, so the state has to be settled while lines are
//  being sized rather than while they are being emitted.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1OptimizeControl (const PendingLine & /*current*/, LineInfo & info)
{
    bool  turningOn = (info.parsed.directiveToken == Directive::Optimize);



    // A source asking for optimization gets it only if the command line did not
    // already refuse. as65 documents -n as winning even over an OPT in the
    // source, so this is where that is enforced.
    m_optimizeEnabled = turningOn && !m_options.disableOpt;

    return S_OK;
}





//  claimed by an earlier phase (.ORG, the segments, the conditionals) or it
//  has nothing to do until pass 2 (.MULTINOP).
//
//  Each row carries its own token purely so the static_assert below can prove
//  the array is still in enum order. Add a Directive without adding its row
//  and the build fails, which is the property the old if/else chain could not
//  offer -- a new directive could be wired into one dispatch site and silently
//  missed by the others.
//
////////////////////////////////////////////////////////////////////////////////

const AssemblySession::DirectiveRow * AssemblySession::GetDirectiveRows()
{
    static constexpr DirectiveRow  s_kRows[] =
{
    { Directive::None,        nullptr,                                      nullptr                                  },
    { Directive::Align,       &AssemblySession::HandlePass1Align,                 &AssemblySession::EmitAlignDirective     },
    { Directive::Byte,        &AssemblySession::HandlePass1DataDirectives,  &AssemblySession::EmitByteDirective      },
    { Directive::Cmap,        &AssemblySession::HandleCmapDirective,        nullptr                                  },
    { Directive::Dd,          &AssemblySession::HandlePass1Dd,                    &AssemblySession::EmitDdDirective        },
    { Directive::Ds,          &AssemblySession::HandlePass1Ds,                    &AssemblySession::EmitDsDirective        },
    { Directive::Else,        nullptr,                                      nullptr                                  },
    { Directive::End,         &AssemblySession::HandlePass1End,                   nullptr                                  },
    { Directive::Endif,       nullptr,                                      nullptr                                  },
    { Directive::Error,       &AssemblySession::HandlePass1Error,                 nullptr                                  },
    { Directive::If,          nullptr,                                      nullptr                                  },
    { Directive::Ifdef,       nullptr,                                      nullptr                                  },
    { Directive::Ifndef,      nullptr,                                      nullptr                                  },
    { Directive::Include,     &AssemblySession::HandleIncludeDirective,     nullptr                                  },
    { Directive::List,        &AssemblySession::HandlePass1List,                  nullptr                                  },
    { Directive::MultiNop,    nullptr,                                      &AssemblySession::EmitMultiNopDirective  },
    { Directive::Nolist,      &AssemblySession::HandlePass1Nolist,                nullptr                                  },
    { Directive::NoOptimize,  &AssemblySession::HandlePass1OptimizeControl, nullptr                                  },
    { Directive::Optimize,    &AssemblySession::HandlePass1OptimizeControl, nullptr                                  },
    { Directive::OptNoop,     &AssemblySession::IgnorePass1Directive,               nullptr                                  },
    { Directive::Org,         nullptr,                                      nullptr                                  },
    { Directive::Page,        &AssemblySession::IgnorePass1Directive,               nullptr                                  },
    { Directive::SegmentBss,  nullptr,                                      nullptr                                  },
    { Directive::SegmentCode, nullptr,                                      nullptr                                  },
    { Directive::SegmentData, nullptr,                                      nullptr                                  },
    { Directive::Struct,      &AssemblySession::StartStructDefinition,      nullptr                                  },
    { Directive::Text,        &AssemblySession::HandlePass1Text,                  &AssemblySession::EmitTextDirective      },
    { Directive::Title,       &AssemblySession::HandlePass1Title,                 nullptr                                  },
    { Directive::Word,        &AssemblySession::HandlePass1Word,                  &AssemblySession::EmitWordDirective      },

    //  Merlin's directives. The TOKENS exist so the vocabulary is complete in
    //  one place and this static_assert fires once rather than once per
    //  directive added.
    //
    //  A null pass-1 column means NOT IMPLEMENTED YET, and the line is then
    //  dropped without a word -- which is the silent wrong-bytes failure this
    //  dialect's vocabulary exists to prevent, not a way of saying "emits
    //  nothing". The tokens whose rows are legitimately null say so at their
    //  own rows below: an earlier phase claims the line, or the boundary
    //  refuses it by name.
    //
    //  WordHighFirst shares its pass-1 handler with the ordinary word directive
    //  because the two reserve identically -- two bytes per argument, whatever
    //  order pass 2 writes them in. Only the emitter differs, and a second
    //  sizing function would be a copy that could drift from the one the
    //  neighboring row uses.
    { Directive::StringData,      &AssemblySession::HandlePass1String,      &AssemblySession::EmitStringDirective    },
    { Directive::HexData,         &AssemblySession::HandlePass1Hex,         &AssemblySession::EmitHexDirective       },
    { Directive::WordHighFirst,   &AssemblySession::HandlePass1Word,        &AssemblySession::EmitWordHighFirstDirective },

    //  ERR acts entirely in pass 2, where every symbol is known. Its pass-1 row
    //  is the recognizer rather than a no-op: a directive with no pass-1 handler
    //  is not marked as one, and an unmarked line never reaches pass-2 dispatch.
    { Directive::ErrorIf,         &AssemblySession::IgnorePass1Directive,    &AssemblySession::EmitErrorIfDirective   },
    { Directive::Loop,            &AssemblySession::HandlePass1Loop,        nullptr                                  },
    { Directive::LoopEnd,         &AssemblySession::HandlePass1LoopEnd,     nullptr                                  },
    { Directive::DummySection,    &AssemblySession::HandlePass1DummySection,    nullptr                              },
    { Directive::DummySectionEnd, &AssemblySession::HandlePass1DummySectionEnd, nullptr                              },
    { Directive::MacroDef,        nullptr,                                  nullptr                                  },
    { Directive::MacroEnd,        nullptr,                                  nullptr                                  },
    { Directive::CpuSelect,       &AssemblySession::HandlePass1CpuSelect,   nullptr                                  },
    //  Both columns, and the pass-2 one is where the outputs are actually cut.
    //  A second occurrence closes the file the first opened, which can only be
    //  done where the byte stream is.
    { Directive::ObjectFile,      &AssemblySession::HandlePass1ObjectFile,  &AssemblySession::EmitObjectFile         },

    //  KBD acts entirely in the prelude, before a label can bind, so both rows
    //  are null for the same reason ORG's are rather than because it is
    //  unimplemented.
    { Directive::KeyboardInput,   nullptr,                                  nullptr                                  },

    //  VAR binds in BOTH passes. Pass 1 so the lines below it are sized against
    //  the values in effect, and pass 2 so a reference resolves against the
    //  binding above it rather than the last one the file made. The pass-2 row
    //  emits no bytes -- the same use of that column the assembly-time assertion
    //  already makes.
    { Directive::ParameterBinding, &AssemblySession::HandlePass1ParameterBinding, &AssemblySession::RebindParameters },

    //  Refused by name rather than handled. The refusal is a table of its own,
    //  consulted before dispatch, so these rows stay null by design.
    //
    //  THE FILE-TYPE ROW IS NO LONGER ONE OF THEM. Its boundary row is gone, so
    //  it reaches dispatch like any other directive and needs a handler here --
    //  without one it would become a word the dialect defines and the assembler
    //  cannot size, which fails the assembly rather than setting a type. Its
    //  POSITION is what matters as much as its presence: this table is indexed
    //  by the directive token, so a row in the wrong place hands one directive's
    //  line to another's handler.
    { Directive::Relocatable,     nullptr,                                  nullptr                                  },
    { Directive::EntrySymbol,     nullptr,                                  nullptr                                  },
    { Directive::ExternalSymbol,  nullptr,                                  nullptr                                  },
    { Directive::FileType,        &AssemblySession::HandlePass1FileType,    &AssemblySession::EmitFileType           },
    { Directive::SaveObject,      &AssemblySession::HandlePass1SaveObject,  &AssemblySession::EmitSaveObject         },
    };



    // Adding a Directive without adding its row fails the build here. Row
    // ORDER is checked at lookup instead -- a static_assert cannot see a
    // function-local array, and every row is exercised by the assembler suite.
    static_assert (std::size (s_kRows) == (size_t) Directive::Count,
                   "s_kRows must have one row per Directive");

    return s_kRows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Directives
//
//  Pass-1 dispatch, the mirror of EmitDirectiveBytes: token indexes the
//  directive table and the pass1 column says who handles it.
//
//  Unlike pass 2, a null column here means NOT HANDLED rather than "handled,
//  emits nothing" -- a directive an earlier phase already claimed -- so
//  `handled` is false and the line falls through to whatever comes next. Same
//  order-assert as pass 2, protecting the same index-by-token assumption.
//
//  A spelling that resolved to NO token is the other case, and it fails the
//  assembly. The line looks like a directive and names nothing the dialect
//  defines, so nothing can size it: it produces no bytes, every address below
//  it moves up, and the run would otherwise succeed while quietly building
//  something other than what was written. It is a SOURCE error rather than a
//  subset boundary -- a word the assembler never recognized is not a construct
//  it understood and declined.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Directives (const PendingLine & current, LineInfo & info, bool & handled)
{
    HRESULT           hr      = S_OK;
    Directive         token   = info.parsed.directiveToken;
    Pass1DirectiveFn  handler = nullptr;



    if (token == Directive::None)
    {
        RecordError (current.sourceLineNumber, DescribeUnknownDirective (info.parsed));
    }

    if (token > Directive::None && token < Directive::Count)
    {
        const DirectiveRow &  row = GetDirectiveRows()[(size_t) token];

        ASSERT (row.token == token);   // the table drifted out of enum order
        handler = row.pass1;
    }

    // A directive with no pass-1 row is not ours: an unrecognized spelling,
    // reported just above, or one an earlier phase already claimed.
    handled          = (handler != nullptr);
    info.isDirective = handled;

    BAIL_OUT_IF (!handled, S_OK);

    hr = (this->*handler) (current, info);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1DataDirectives
//
//  Sizes a byte-producing directive in pass 1. Only the COUNT matters here --
//  pass 2 computes the values -- so this advances m_pc and nothing else.
//
//  Evaluation is attempted first because it is the reliable count. When it
//  fails (a forward reference, typically) the fallback counts comma-separated
//  arguments instead, which is right for one-byte-per-argument data and keeps
//  every later label at the correct address even though no value is known yet.
//
//  Errors go into a THROWAWAY list: any genuine problem will be reported again
//  by pass 2 against the complete symbol table, and reporting here would
//  duplicate every diagnostic and blame forward references that were about to
//  resolve.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1DataDirectives (const PendingLine & current, LineInfo & info)
{
    HRESULT                     hr         = S_OK;
    std::vector<int32_t>        values;
    std::vector<AssemblyError>  tempErrors;



    m_pass1Ctx.currentPC = (int32_t) m_pc;


    TryEvaluateDirectiveArgs (info.parsed.directiveArg, m_pass1Ctx, values, current.sourceLineNumber, tempErrors);

    // If evaluation fails, try counting comma-separated items
    if (values.empty() && !info.parsed.directiveArg.empty())
    {
        auto args = Parser::SplitArgList (info.parsed.directiveArg);
        ReserveBytes ((Word) args.size());
    }
    else
    {
        ReserveBytes ((Word) values.size());
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleIncludeDirective
//
//  Splices an included file into the pending queue rather than recursing, so
//  arbitrarily deep includes cost queue entries instead of C++ stack. Lines
//  are pushed to the FRONT in reverse order, which lands them in source order
//  immediately after this directive -- the queue is consumed from the front.
//
//  The extension decides the treatment. A .bin / .s19 / .s28 / .s37 / .hex
//  payload is converted to synthesized .BYTE lines by GenerateByteDirectives,
//  so binary data re-enters through the ordinary assembly path and needs no
//  special case downstream. Anything else is included as source text.
//
//  Binary lines keep the INCLUDING line's number, because they have no source
//  lines of their own -- an error in them should point at the .include.
//  Included source keeps its own numbering, so errors point into that file.
//
//  Every failure here is a user-facing diagnostic, not an infrastructure
//  fault: missing reader, depth exceeded, unreadable file all record an error
//  and return S_OK so the rest of the assembly still runs and reports.
//  kMaxIncludeDepth is what stops a file that includes itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleIncludeDirective (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    // A user-facing diagnostic, not an infrastructure failure: the error goes
    // into the result and assembly carries on, so hr stays S_OK.
    CBRFEx (m_options.fileReader != nullptr, S_OK,
            RecordError (current.sourceLineNumber, "No file reader configured for include"));

    CBRFEx (current.includeDepth < kMaxIncludeDepth, S_OK,
            RecordError (current.sourceLineNumber,
                "Include nesting depth exceeded (max " + std::to_string (kMaxIncludeDepth) + ")"));

    {
        std::string               filename   = Parser::ParseQuotedString (info.parsed.directiveArg);
        std::string               ext;
        std::vector<std::string>  synthLines;
        FileReadResult            fr         = {};

        if (filename.empty())
        {
            filename = info.parsed.directiveArg;
            size_t fs = filename.find_first_not_of (" \t");
            size_t fe = filename.find_last_not_of (" \t");

            if (fs != std::string::npos)
            {
                filename = filename.substr (fs, fe - fs + 1);
            }
        }

        fr = m_options.fileReader->ReadFile (filename, m_options.baseDir);

        CBRFEx (fr.success, S_OK, RecordError (current.sourceLineNumber, fr.error));

        ext = GetLowerExtension (filename);

        if (ext == ".bin")
        {
            std::vector<Byte> raw (fr.contents.begin(), fr.contents.end());
            synthLines = GenerateByteDirectives (raw);
        }
        else if (ext == ".s19" || ext == ".s28" || ext == ".s37")
        {
            synthLines = GenerateByteDirectives (ParseSRecord (fr.contents));
        }
        else if (ext == ".hex")
        {
            synthLines = GenerateByteDirectives (ParseIntelHex (fr.contents));
        }

        if (!synthLines.empty())
        {
            for (int il = (int) synthLines.size() - 1; il >= 0; il--)
            {
                PendingLine pl   = {};
                pl.text          = synthLines[il];
                pl.sourceLineNumber = current.sourceLineNumber;
                pl.macroDepth    = current.macroDepth;
                pl.includeDepth  = current.includeDepth + 1;
                pl.sourceFile    = filename;
                m_pendingLines.push_front (pl);
            }
        }
        else if (ext != ".bin" && ext != ".s19" && ext != ".s28"
              && ext != ".s37" && ext != ".hex")
        {
            auto includeLines = Parser::SplitLines (fr.contents);

            for (int il = (int) includeLines.size() - 1; il >= 0; il--)
            {
                PendingLine pl   = {};
                pl.text          = includeLines[il];
                pl.sourceLineNumber = il + 1;
                pl.macroDepth    = current.macroDepth;
                pl.includeDepth  = current.includeDepth + 1;
                pl.sourceFile    = filename;
                m_pendingLines.push_front (pl);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::StartStructDefinition
//
//  Opens a STRUCT block: pass 1 switches to CollectingStruct, and every member
//  line after this is measured rather than assembled until END STRUCT.
//
//  A struct emits nothing. It defines OFFSETS -- each member becomes a symbol
//  whose value is its position within the struct -- which is why it tracks its
//  own currentOffset instead of touching m_pc.
//
//  The optional second argument is a starting offset, so a struct can be laid
//  over an existing memory map rather than starting at zero. A base expression
//  that fails to evaluate falls back to zero rather than abandoning the
//  struct, so the members are still defined and their relative offsets stay
//  correct.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::StartStructDefinition (const PendingLine & current, LineInfo & info)
{
    HRESULT hr      = S_OK;
    bool    hasName = false;



    {
        auto args = Parser::SplitArgList (info.parsed.directiveArg);

        hasName = !args.empty();
        CBRFEx (hasName, S_OK, RecordError (current.sourceLineNumber, "struct requires a name"));

        m_currentStruct             = {};
        m_currentStruct.name        = args[0];
        m_currentStruct.startOffset = 0;

        if (args.size() >= 2)
        {
            ExprResult  er;

            m_pass1Ctx.currentPC = (int32_t) m_pc;
            er = ExpressionEvaluator::Evaluate (args[1], m_pass1Ctx);

            if (er.success)
            {
                m_currentStruct.startOffset = er.value;
            }
        }

        m_currentStruct.currentOffset = m_currentStruct.startOffset;
        m_pass1State = Pass1State::CollectingStruct;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleCmapDirective
//
//  .CMAP installs a character translation applied to string literals in
//  .BYTE, which is how source written in ASCII emits text in the target's
//  encoding -- Apple II high-bit ASCII, or a custom game font where 'A' is
//  tile 0.
//
//  `.cmap 0` resets to identity, the escape hatch for turning a mapping off
//  partway through a file. Anything starting with a quote is a mapping and
//  goes to ParseCmapMapping; anything else is ignored rather than reported,
//  since the table is only consulted for string literals and a malformed
//  directive costs nothing but its own line.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleCmapDirective (const PendingLine & /*current*/, LineInfo & info)
{
    HRESULT hr = S_OK;



    std::string arg = info.parsed.directiveArg;



    {
        size_t as = arg.find_first_not_of (" \t");

        if (as != std::string::npos)
        {
            arg = arg.substr (as);
        }

        size_t ae = arg.find_last_not_of (" \t");

        if (ae != std::string::npos)
        {
            arg = arg.substr (0, ae + 1);
        }
    }

    if (arg == "0")
    {
        for (int ci = 0; ci < 256; ci++)
        {
            m_charMap.table[ci] = (Byte) ci;
        }
    }
    else if (arg.size() >= 5 && arg[0] == '\'')
    {
        hr = ParseCmapMapping (arg);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ParseCmapMapping
//
//  One .CMAP entry, in either of two forms:
//
//      'A' = $C1          a single character
//      'A'-'Z' = $C1      a range, mapped consecutively from the right side
//
//  The range form is what makes a whole alphabet one line instead of 26, and
//  it maps sequentially -- 'B' lands on $C2 and so on -- so a contiguous
//  target encoding needs only its first value.
//
//  The dash is searched from index 1, never 0, so a quoted '-' character is
//  not mistaken for the range separator. It must also fall before the '=' to
//  count, which keeps a minus sign in the right-hand expression from being
//  read as one.
//
//  A malformed entry is skipped silently, matching the directive above: the
//  table simply keeps its previous value for those characters.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ParseCmapMapping (const std::string & arg)
{
    HRESULT hr = S_OK;



    size_t eqPos   = arg.find ('=');
    size_t dashPos = arg.find ('-', 1);



    // No '=' means this is not a constant definition after all.
    BAIL_OUT_IF (eqPos == std::string::npos, S_OK);

    {
        std::string lhs = arg.substr (0, eqPos);
        std::string rhs = arg.substr (eqPos + 1);
        ExprResult  rhsVal;

        {
            size_t ls = lhs.find_last_not_of (" \t");

            if (ls != std::string::npos)
            {
                lhs = lhs.substr (0, ls + 1);
            }

            size_t rs = rhs.find_first_not_of (" \t");

            if (rs != std::string::npos)
            {
                rhs = rhs.substr (rs);
            }
        }

        m_pass1Ctx.currentPC = (int32_t) m_pc;
        rhsVal = ExpressionEvaluator::Evaluate (rhs, m_pass1Ctx);

        if (rhsVal.success)
        {
            if (dashPos != std::string::npos && dashPos < eqPos &&
                lhs.size() >= 7 && lhs[0] == '\'' && lhs[2] == '\'')
            {
                char         startChar = lhs[1];
                std::string  afterDash = lhs.substr (dashPos + 1);
                size_t ads = afterDash.find_first_not_of (" \t");

                if (ads != std::string::npos)
                {
                    afterDash = afterDash.substr (ads);
                }

                if (afterDash.size() >= 3 && afterDash[0] == '\'' && afterDash[2] == '\'')
                {
                    char endChar = afterDash[1];

                    for (int ci = (unsigned char) startChar; ci <= (unsigned char) endChar; ci++)
                    {
                        m_charMap.table[ci] = (Byte) (rhsVal.value + (ci - (unsigned char) startChar));
                    }
                }
            }
            else if (lhs.size() >= 3 && lhs[0] == '\'' && lhs[2] == '\'')
            {
                m_charMap.table[(unsigned char) lhs[1]] = (Byte) rhsVal.value;
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ExpandMacro
//
//  Replaces a macro call with its substituted body, spliced to the FRONT of
//  the pending queue in reverse so it lands in source order right where the
//  call was. Same mechanism as .include, and for the same reason: expansion is
//  iterative through the queue, not recursive, so nesting costs queue entries
//  rather than C++ stack. A macro that expands to another macro simply
//  arrives at this function again with macroDepth one higher.
//
//  m_macroUniqueCounter is bumped per expansion and its suffix passed down, so
//  LOCAL labels get a distinct name per invocation -- without it, calling the
//  same macro twice would define the same label twice.
//
//  Note the two exits differ in `handled`. Not a macro at all leaves it false,
//  so later stages get the line. Depth exceeded sets it TRUE despite failing:
//  the line WAS a macro call, and letting a later stage reinterpret it would
//  report a bogus "unknown mnemonic" on top of the real error.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ExpandMacro (const PendingLine & current, LineInfo & info, bool & handled)
{
    HRESULT                   hr        = S_OK;
    MacroSyntax               syntax    = m_dialect.GetMacroSyntax();
    std::string               name      = info.parsed.mnemonic;
    std::vector<std::string>  args;
    std::vector<std::string>  expandedLines;
    std::string               uniqueSuffix;
    int                       highest   = 0;
    int                       supplied  = 0;
    bool                      isDefined = false;



    handled = false;

    // The argument separator is the dialect's. Merlin's is a semicolon, which is
    // also why the field scanner refuses to treat one inside the operand as a
    // comment: `ADD SUMSTR;DEFLEN;PL` passes three arguments, and a parser
    // stripping from the first semicolon would pass one.
    args      = Parser::SplitOnSeparator (info.parsed.operand, syntax.argumentSeparator);
    isDefined = (m_macros.find (name) != m_macros.end());

    // Not a macro call; the line belongs to a later stage.
    BAIL_OUT_IF (!isDefined, S_OK);

    // Claimed even though it failed: the line was a macro call, so no later
    // stage should try to reinterpret it.
    CBRFEx (current.macroDepth < kMaxMacroDepth, S_OK,
            RecordError (current.sourceLineNumber,
                "Macro nesting depth exceeded (max " + std::to_string (kMaxMacroDepth) + ")");
            handled = true);

    // A parameter the body refers to with no argument to fill it. REJECTED here
    // rather than expanded with the gap left empty, and the difference is not
    // cosmetic: an empty substitution turns `LDA ]2` into `LDA `, and a body
    // whose lines happen to survive that assembles a DIFFERENT program without
    // complaint.
    //
    // The commonest cause is not a typo. What separates one argument from the
    // next is the dialect's -- Merlin uses a semicolon where others use a comma
    // -- so a call written in another assembler's punctuation arrives as ONE
    // argument however many were meant, and every parameter after the first is
    // unfilled. Claimed on the way out for the same reason the depth limit is.
    highest = HighestParameterReferenced (m_macros[name], syntax.parameterSigil);
    supplied = (int) args.size();

    CBRFEx (highest <= supplied, S_OK,
            RecordError (current.sourceLineNumber,
                name + " refers to parameter " + std::string (1, syntax.parameterSigil) +
                std::to_string (highest) + " but the invocation supplies " + std::to_string (supplied) +
                " argument(s). Arguments are separated by '" + std::string (1, syntax.argumentSeparator) + "'.");
            handled = true);

    m_macroUniqueCounter++;
    uniqueSuffix = std::format ("{:04d}", m_macroUniqueCounter);

    hr = SubstituteMacroParams (m_macros[name], args, uniqueSuffix, expandedLines);
    CHR (hr);

    // Insert expanded lines at the FRONT of the queue (reverse order)
    for (int bi = (int) expandedLines.size() - 1; bi >= 0; bi--)
    {
        PendingLine  pl = {};

        pl.text             = expandedLines[bi];
        pl.sourceLineNumber = current.sourceLineNumber;
        pl.sourceFile       = current.sourceFile;
        pl.macroDepth       = current.macroDepth + 1;
        m_pendingLines.push_front (pl);
    }

    handled = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HighestParameterReferenced
//
//  How many arguments a body actually needs.
//
//  Scans the RAW stored body, which is what makes it agree with substitution by
//  construction rather than by two implementations happening to match.
//  ApplyMacroSubstitutions replaces the sigil form wherever it occurs, comments
//  included, so a check that first stripped comments would pass a call whose
//  expansion then substituted an empty string into one.
//
//  A digit is what separates a parameter from a variable symbol -- `]1` is an
//  argument and `]COUNT` a name -- so anything but a digit after the sigil is
//  not a parameter at all. Zero is excluded with it: a dialect that spells the
//  argument COUNT that way is asking a question, not naming a slot.
//
//  0 for a dialect with no positional form, so it can never refuse a call.
//
////////////////////////////////////////////////////////////////////////////////

int AssemblySession::HighestParameterReferenced (const MacroDefinition & macroDef, char sigil)
{
    int  highest = 0;



    if (sigil == 0)
    {
        return highest;
    }

    for (const std::string & line : macroDef.body)
    {
        size_t  pos = 0;

        while ((pos = line.find (sigil, pos)) != std::string::npos)
        {
            pos++;

            if ((pos < line.size()) && (line[pos] >= '1') && (line[pos] <= '9'))
            {
                int  index = line[pos] - '0';

                highest = (index > highest) ? index : highest;
            }
        }
    }

    return highest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::SubstituteMacroParams
//
//  Turns a stored macro body into the lines one invocation actually emits:
//  parameters replaced by arguments, LOCAL labels suffixed unique to this
//  call, and the body truncated at EXITM.
//
//  EXITM stops the expansion mid-body, which means any conditional the body
//  had open is abandoned. CountExitmIfDepth measures how many, and that many
//  synthesized ENDIFs are appended -- otherwise the IF would stay open past
//  the expansion and the file would end with an unclosed-block error pointing
//  at the macro rather than at whatever is wrong.
//
//  Local-label declarations are dropped rather than emitted: uniqueSuffix has
//  already done their work, so passing them through would leave a directive
//  the assembler would have to ignore anyway.
//
//  The keyword is the ACTIVE DIALECT'S, not a literal. It stays out of the
//  spelling tables on purpose -- it is a macro-body keyword, and tokenizing it
//  would cost a lookup on every line of every file to serve lines that appear
//  only here -- but comparing a fixed word instead means a dialect whose source
//  merely CONTAINS that word loses the line. In a field-based dialect the first
//  word of a line is a label, so `LOCAL LDA #1` is a labeled instruction, and
//  dropping it deletes both the label and two bytes with no diagnostic.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::SubstituteMacroParams (const MacroDefinition & macroDef,
                                                 const std::vector<std::string> & args,
                                                 const std::string & uniqueSuffix,
                                                 std::vector<std::string> & expandedLines)
{
    HRESULT       hr           = S_OK;
    std::string   localKeyword = m_dialect.GetMacroSyntax().localKeyword;
    const auto &  body         = macroDef.body;



    for (int bi = 0; bi < (int) body.size(); bi++)
    {
        std::string  expanded  = body[bi];
        std::string  firstWord;

        // Check for exitm
        bool isExitm = false;
        hr = CheckForExitm (expanded, isExitm);
        CHR (hr);

        if (isExitm)
        {
            int          ifDepth = 0;
            std::string  closer  = m_dialect.GetSpellingForDirective (Directive::Endif);

            hr = CountExitmIfDepth (expandedLines, ifDepth);
            CHR (hr);

            // Spelled by the ACTIVE PROFILE. This is the one line the assembler
            // writes for itself, and a fixed word here put another dialect's
            // spelling into this dialect's stream -- where it is an unknown
            // operation on a line the source never wrote and cannot see. Indented
            // because a dialect whose first column is the label field would read
            // it as one.
            for (int ed = 0; ed < ifDepth; ed++)
            {
                expandedLines.push_back ("                " + closer);
            }

            break;
        }

        // The local-label declaration, which uniqueSuffix has already taken care
        // of, so it never reaches the output. Recognized by the active dialect's
        // keyword: a dialect with none answers empty and no line is claimed.
        firstWord = GetLeadingWord (ToUpperCase (expanded));

        if (!localKeyword.empty() && ((firstWord == localKeyword) || (firstWord == "." + localKeyword)))
        {
            continue;
        }

        hr = ApplyMacroSubstitutions (expanded, macroDef, args, uniqueSuffix);
        CHR (hr);

        hr = StripForcedSubstitution (expanded);
        CHR (hr);

        expandedLines.push_back (expanded);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CheckForExitm
//
//  Does this body line abandon the rest of the expansion?
//
//  The keyword stays a string compare rather than joining a spelling table: a
//  table feeds the parser, so a row there would cost a lookup on every line of
//  every file to serve lines that appear only inside a macro body. What it must
//  NOT stay is a fixed word -- the profile supplies it, beside the terminator
//  and the local declaration, and a dialect with no early exit answers empty and
//  claims no line at all. A fixed word truncated a body silently in any dialect
//  whose source happened to contain it.
//
//  The dotted form is admitted the way the local declaration's is, for the same
//  reason: a dialect writing its keywords with a leading dot spells this one
//  that way too, and stating it twice in the profile would be one spelling that
//  could disagree with itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::CheckForExitm (const std::string & line, bool & isExitm)
{
    HRESULT      hr          = S_OK;
    std::string  exitKeyword = m_dialect.GetMacroSyntax().exitKeyword;
    std::string  code        = ToUpperCase (StripCommentAndTrim (line));



    isExitm = !exitKeyword.empty() && ((code == exitKeyword) || (code == "." + exitKeyword));

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CountExitmIfDepth
//
//  How many conditional blocks are still open in the lines expanded so far --
//  which is exactly how many ENDIFs an EXITM has to synthesize to leave the
//  file balanced.
//
//  Counted over the ALREADY-EXPANDED lines rather than the macro body, since
//  substitution can change which directives are present. It is a net depth,
//  so an IF/ENDIF pair inside the abandoned region cancels out and only the
//  genuinely unclosed ones are counted.
//
//  Each line is READ BY THE ACTIVE PROFILE rather than scanned for a leading
//  word here. The spellings were once written out in this loop, then read from
//  a fixed table, and both were wrong in both directions for any dialect but the
//  one the table belonged to: a foreign word that dialect never wrote was
//  counted, and its own conditional was not. An EXITM then synthesized the wrong
//  number of closers, which either leaves a block open past the expansion or
//  closes one the source is still inside.
//
//  Parsing rather than word-splitting also settles a labeled conditional, which
//  no leading-word scan can: the first field of such a line is the label.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::CountExitmIfDepth (const std::vector<std::string> & expandedLines, int & ifDepth)
{
    HRESULT hr = S_OK;



    ifDepth = 0;



    for (const std::string & line : expandedLines)
    {
        Directive  token = TokenForLine (m_dialect.ParseLine (line, 0));

        if (token == Directive::If || token == Directive::Ifdef || token == Directive::Ifndef)
        {
            ifDepth++;
        }
        else if (token == Directive::Endif)
        {
            ifDepth--;
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ApplyMacroSubstitutions
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ApplyMacroSubstitutions (std::string & expanded,
                                                   const MacroDefinition & macroDef,
                                                   const std::vector<std::string> & args,
                                                   const std::string & uniqueSuffix)
{
    HRESULT  hr    = S_OK;
    char     sigil = m_dialect.GetMacroSyntax().parameterSigil;



    // Positional parameters, for a dialect that spells them with a sigil rather
    // than declaring names. Substitution deliberately ignores identifier
    // boundaries: Merlin's own library splices a parameter INTO a symbol, and
    // both directions appear on the distribution disk -- `LDX #A]1-ADRTBL`
    // pastes the argument after a prefix, `LDX #]1END-]1-1` before a suffix. A
    // whole-word rule would leave both untouched and every reference undefined.
    if (sigil != 0)
    {
        for (int ai = 1; ai <= kMaxPositionalParams; ai++)
        {
            std::string  placeholder = std::string (1, sigil) + std::to_string (ai);
            std::string  replacement = (ai <= (int) args.size()) ? args[ai - 1] : "";
            size_t       pos         = 0;

            while ((pos = expanded.find (placeholder, pos)) != std::string::npos)
            {
                expanded.replace (pos, placeholder.size(), replacement);
                pos += replacement.size();
            }
        }
    }

    // Replace \0 with argument count
    {
        std::string  argCountStr = std::to_string ((int) args.size());
        size_t       pos         = 0;

        while ((pos = expanded.find ("\\0", pos)) != std::string::npos)
        {
            expanded.replace (pos, 2, argCountStr);
            pos += argCountStr.size();
        }
    }

    // Replace \1 through \9 with arguments
    for (int ai = 9; ai >= 1; ai--)
    {
        size_t pos = 0;

        std::string placeholder = "\\" + std::to_string (ai);

        while ((pos = expanded.find (placeholder, pos)) != std::string::npos)
        {
            std::string replacement = (ai <= (int) args.size()) ? args[ai - 1] : "";
            expanded.replace (pos, placeholder.size(), replacement);
            pos += replacement.size();
        }
    }

    // Replace named parameters as whole-word matches
    for (int pi = 0; pi < (int) macroDef.paramNames.size(); pi++)
    {
        const std::string  & paramName = macroDef.paramNames[pi];
        size_t               pos       = 0;
        std::string replacement = (pi < (int) args.size()) ? args[pi] : "";

        while ((pos = expanded.find (paramName, pos)) != std::string::npos)
        {
            size_t  endPos = 0;

            bool leftOk = (pos == 0) ||
                           (!isalnum ((unsigned char) expanded[pos - 1]) && expanded[pos - 1] != '_');
            endPos = pos + paramName.size();
            bool rightOk = (endPos >= expanded.size()) ||
                            (!isalnum ((unsigned char) expanded[endPos]) && expanded[endPos] != '_');

            if (leftOk && rightOk)
            {
                expanded.replace (pos, paramName.size(), replacement);
                pos += replacement.size();
            }
            else
            {
                pos += paramName.size();
            }
        }
    }

    // Replace \? with unique suffix
    {
        size_t pos = 0;

        while ((pos = expanded.find ("\\?", pos)) != std::string::npos)
        {
            expanded.replace (pos, 2, uniqueSuffix);
            pos += uniqueSuffix.size();
        }
    }

    // Apply local label suffixing
    for (const auto & localLabel : macroDef.localLabels)
    {
        size_t pos = 0;

        while ((pos = expanded.find (localLabel, pos)) != std::string::npos)
        {
            size_t  endPos = 0;

            bool leftOk = (pos == 0) ||
                           (!isalnum ((unsigned char) expanded[pos - 1]) && expanded[pos - 1] != '_');
            endPos = pos + localLabel.size();
            bool rightOk = (endPos >= expanded.size()) ||
                            (!isalnum ((unsigned char) expanded[endPos]) && expanded[endPos] != '_');

            if (leftOk && rightOk)
            {
                std::string suffixed = localLabel + uniqueSuffix;
                expanded.replace (pos, localLabel.size(), suffixed);
                pos += suffixed.size();
            }
            else
            {
                pos += localLabel.size();
            }
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::StripForcedSubstitution
//
//  Removes the single quotes that delimit forced substitution in a macro body.
//  They exist so a parameter can be pasted flush against surrounding text --
//  `LDA 'PREFIX'_TABLE` substitutes PREFIX and leaves `LDA foo_TABLE`, which
//  bare `PREFIX_TABLE` could not express because that is one identifier.
//
//  Substitution has already happened by this point; only the markers are left,
//  and they must not reach the parser.
//
//  Quotes inside a DOUBLE-quoted string are skipped: an apostrophe in a
//  message is text, not a marker. An unpaired quote is left alone rather than
//  erased, so a lone apostrophe survives as a character literal.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::StripForcedSubstitution (std::string & expanded)
{
    HRESULT hr = S_OK;



    size_t  sq       = 0;
    bool    inDouble = false;



    while (sq < expanded.size())
    {
        if (expanded[sq] == '"')
        {
            inDouble = !inDouble;
            sq++;
        }
        else if (!inDouble && expanded[sq] == '\'')
        {
            size_t sq2 = expanded.find ('\'', sq + 1);

            if (sq2 != std::string::npos)
            {
                expanded.erase (sq2, 1);
                expanded.erase (sq, 1);
            }
            else
            {
                sq++;
            }
        }
        else
        {
            sq++;
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleColonlessLabel
//
//  Handles the traditional 6502 form where a label needs no colon and is
//  identified purely by starting in column 0:
//
//      loop    LDA $00        <- `loop` is a label, LDA is the instruction
//
//  This runs LAST, after opcodes, bit-ops and macros have all had their turn,
//  because "a word in column 0" is otherwise indistinguishable from any of
//  them. Reaching here means the word matched nothing else, so it must be a
//  label -- the elimination is the identification.
//
//  Anything following the label is pushed back onto the queue as its own line
//  rather than assembled here, so it re-enters through the ordinary path and
//  the split needs no second copy of the instruction logic. The mnemonic and
//  operand are then cleared, since this line is now only the label.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleColonlessLabel (const PendingLine & current, LineInfo & info, bool & handled)
{
    HRESULT  hr              = S_OK;
    bool     fLooksLikeLabel = false;



    handled = false;



    // A colon-less label is whatever is left once every real mnemonic form
    // has had its turn: it must start at column 0, carry no explicit label,
    // and not be an opcode, a bit-op, or a macro name.
    fLooksLikeLabel = info.parsed.startsAtColumn0 &&
                      info.parsed.label.empty() &&
                      !m_opcodeTable->IsMnemonic (info.parsed.mnemonic) &&
                      !IsBitOpMnemonic (info.parsed.mnemonic) &&
                      (m_macros.find (info.parsed.mnemonic) == m_macros.end());

    BAIL_OUT_IF (!fLooksLikeLabel, S_OK);

    {
        std::string labelName;
        std::string labelError;
        HRESULT     hrLabel = S_OK;

        hr = ExtractColonlessLabelName (current, labelName);
        CHR (hr);

        hrLabel = Parser::ValidateLabel (labelName, *m_opcodeTable, labelError,
                                         m_dialect.GetExtraSymbolCharacters());

        if (FAILED (hrLabel))
        {
            RecordErrorAt (current.sourceLineNumber, info.parsed.labelColumn, labelError);
        }
        else if (m_symbols.count (labelName) > 0)
        {
            RecordErrorAt (current.sourceLineNumber, info.parsed.labelColumn,
                           "Duplicate label: " + labelName);
        }
        else
        {
            m_symbols[labelName]     = m_pc;
            m_symbolKinds[labelName] = SymbolKind::Label;
            m_exprSymbols[labelName] = (int32_t) m_pc;
        }

        info.parsed.label = labelName;

        if (!info.parsed.operand.empty())
        {
            PendingLine pl   = {};
            pl.text          = "    " + info.parsed.operand;
            pl.sourceLineNumber = current.sourceLineNumber;
            pl.macroDepth    = current.macroDepth;
            m_pendingLines.push_front (pl);
        }

        info.parsed.mnemonic.clear();
        info.parsed.operand.clear();
        info.isInstruction = false;
        handled = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ExtractColonlessLabelName
//
//  Takes the label from the RAW source line rather than from the parsed
//  mnemonic. The parser uppercases mnemonics, and a label's case is
//  significant -- lifting it from info.parsed.mnemonic would define `Loop` as
//  `LOOP` and leave every reference to it unresolved.
//
//  It is the first whitespace-delimited word, with any trailing comment cut,
//  so `loop; entry point` yields `loop` rather than swallowing the comment.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ExtractColonlessLabelName (const PendingLine & current, std::string & labelName)
{
    HRESULT hr = S_OK;



    std::string rawTrimmed = current.text;



    {
        size_t  sc = 0;

        size_t s = rawTrimmed.find_first_not_of (" \t");

        if (s != std::string::npos)
        {
            rawTrimmed = rawTrimmed.substr (s);
        }

        size_t e = rawTrimmed.find_first_of (" \t");

        if (e != std::string::npos)
        {
            labelName = rawTrimmed.substr (0, e);
        }
        else
        {
            labelName = rawTrimmed;
        }

        sc = labelName.find (';');

        if (sc != std::string::npos)
        {
            labelName = labelName.substr (0, sc);
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::NormalizeBitOp
//
//  Folds the as65 spelling of the Rockwell bit operations into the suffixed
//  one, so only a single form reaches the classifier and the opcode table:
//
//      RMB 3,$12        ->  RMB3 $12
//      BBS 0,$12,tgt    ->  BBS0 $12,tgt
//
//  The bit number must be a pass-1 constant 0..7, since it selects the OPCODE
//  rather than being encoded as an operand -- there is no byte to defer it to,
//  and a forward reference could not be widened the way an address can.
//
//  Fewer than two operands is left alone rather than reported here, so the
//  ordinary addressing-mode path produces the diagnostic and this stays a
//  normalization step with one error of its own.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::NormalizeBitOp (const PendingLine & current, LineInfo & info)
{
    const std::string & m = info.parsed.mnemonic;



    // as65 spells the Rockwell bit ops with the bit as a leading operand and a bare
    // mnemonic: RMB/SMB as `<bit>,<zp>`, BBR/BBS as `<bit>,<zp>,<target>`. Fold the
    // bit into the mnemonic (RMB3, BBS0, ...) so the shared classifier and opcode
    // table resolve them exactly like the suffixed form (RMB3 $zp, BBS0 $zp,tgt).
    if (m == "RMB" || m == "SMB" || m == "BBR" || m == "BBS")
    {
        std::vector<std::string> parts = Parser::SplitArgList (info.parsed.operand);

        // Need the bit plus at least the zero-page operand; otherwise leave it and
        // let the normal path report the (invalid) addressing mode.
        if (parts.size() >= 2)
        {
            ExprResult er = ExpressionEvaluator::Evaluate (parts[0], m_pass1Ctx);

            if (!er.success || er.value < 0 || er.value > 7)
            {
                RecordError (current.sourceLineNumber,
                    "Bit number for " + m + " must be a constant 0..7");
                info.hasError = true;
            }
            else
            {
                std::string rest;

                info.parsed.mnemonic = m + std::to_string (er.value);


                for (size_t i = 1; i < parts.size(); ++i)
                {
                    if (i > 1)
                    {
                        rest += ",";
                    }

                    rest += parts[i];
                }

                info.parsed.operand = rest;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ClassifyAndResolve
//
//  Pass-1 handling of an instruction line: normalize the mnemonic, classify
//  the operand syntax, try to resolve its value, and settle the addressing
//  mode and size.
//
//  The value is attempted in pass 1 because SIZE depends on it -- $12 is a
//  two-byte zero-page instruction while $1234 is three -- and every label
//  after this line takes its address from that size.
//
//  A failure splits on hasUnresolved. An undefined symbol is expected here:
//  it is a forward reference, pass 2 will have it, and the mode falls back to
//  the wider form so the size is right either way. Any OTHER failure is a real
//  expression error and is reported now, since waiting for pass 2 would report
//  it against a symbol table that had since changed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ClassifyAndResolve (const PendingLine & current, LineInfo & info)
{
    HRESULT  hr           = S_OK;
    bool     exprResolved = false;
    int32_t  exprValue    = 0;



    NormalizeBitOp (current, info);

    info.classified    = Parser::ClassifyOperand (info.parsed.operand);
    info.isInstruction = true;

    m_pass1Ctx.currentPC = (int32_t) m_pc;


    if (info.classified.syntax != OperandSyntax::None &&
        info.classified.syntax != OperandSyntax::Accumulator &&
        !info.classified.expression.empty())
    {
        ExprResult er = ExpressionEvaluator::Evaluate (info.classified.expression, m_pass1Ctx);

        if (er.success)
        {
            exprResolved = true;
            exprValue    = er.value;
        }
        else if (!er.hasUnresolved)
        {
            RecordErrorAt (current.sourceLineNumber, info.parsed.operandColumn,
                           "Expression error: " + er.error);
            info.hasError = true;
        }
    }

    info.valueResolved = exprResolved;
    info.resolvedValue = exprValue;

    hr = ResolveAddressingAndSize (current, info, exprValue, exprResolved);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ResolveAddressingAndSize
//
//  Picks the addressing mode and advances m_pc by the instruction's size --
//  the single most consequential thing pass 1 does, since every later label's
//  address depends on getting the size right the first time.
//
//  When the chosen mode has no encoding for this mnemonic, the zero-page forms
//  are retried as their absolute equivalents. That is what makes a forward
//  reference work: an unresolved operand looks small, would classify as zero
//  page, and would reserve two bytes for an instruction that turns out to need
//  three. Widening keeps the reservation correct even when the value is not
//  yet known.
//
//  The widening only ever goes zero-page -> absolute, never the reverse.
//  Reserving too much would leave a gap; reserving too little would overlap
//  the next instruction, and every address after it would be wrong.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ResolveAddressingAndSize (const PendingLine & current, LineInfo & info,
                                                    int32_t exprValue, bool exprResolved)
{
    HRESULT hr = S_OK;



    {
        OpcodeEntry entry    = {};
        std::string encoded;

        GlobalAddressingMode::AddressingMode mode = ResolveAddressingMode (
            info.classified.syntax, info.parsed.mnemonic,
            exprValue, exprResolved);
        info.resolvedMode = mode;

        // The one place a line is sized as an instruction other than the one it
        // was written with. Decided here rather than in pass 2 because SIZE is
        // what changes -- three bytes become two -- and every label below this
        // line takes its address from that.
        if (CanReplaceJumpWithBranch (info, mode, exprValue, exprResolved))
        {
            info.jumpOptimizedToBranch = true;
            info.resolvedMode          = GlobalAddressingMode::Relative;
            mode                       = GlobalAddressingMode::Relative;
        }

        encoded = EncodedMnemonic (info);


        if (m_opcodeTable->TryLookup (encoded, mode, entry))
        {
            ReserveBytes ((Word) (1 + entry.operandSize));
        }
        else
        {
            GlobalAddressingMode::AddressingMode altMode = mode;

            if (altMode == GlobalAddressingMode::ZeroPage)
            {
                altMode = GlobalAddressingMode::Absolute;
            }
            else if (altMode == GlobalAddressingMode::ZeroPageX)
            {
                altMode = GlobalAddressingMode::AbsoluteX;
            }
            else if (altMode == GlobalAddressingMode::ZeroPageY)
            {
                altMode = GlobalAddressingMode::AbsoluteY;
            }

            if (altMode != mode && m_opcodeTable->TryLookup (info.parsed.mnemonic, altMode, entry))
            {
                info.resolvedMode = altMode;
                ReserveBytes ((Word) (1 + entry.operandSize));
            }
            else if (!info.hasError)
            {
                if (!m_opcodeTable->NamesAnInstruction (info.parsed.mnemonic))
                {
                    RecordError (current.sourceLineNumber, DescribeUnknownOperation (info.parsed));
                }
                else if (info.classified.syntax == OperandSyntax::None)
                {
                    RecordError (current.sourceLineNumber, "Missing operand for: " + info.parsed.mnemonic);
                }
                else
                {
                    RecordError (current.sourceLineNumber, "Invalid addressing mode for: " + info.parsed.mnemonic);
                }

                info.hasError = true;
                ReserveBytes (EstimateErrorRecoverySize (info.classified.syntax, info.parsed.mnemonic));
            }
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ValidateAssemblyCompletion
//
//  End-of-pass-1 balance check: the block openers that were never closed.
//  Their closers self-report as they are encountered (a stray ENDIF or ENDM
//  errors on the spot), but an opener that is never closed leaves nothing
//  behind to notice it -- the source simply ends -- so it has to be caught
//  from the leftover state here.
//
//  Both are recorded rather than thrown: pass 1 has already finished, so
//  reporting every imbalance beats stopping at the first.
//
//  Both point at the line the block OPENED on, which is where the fix goes --
//  never at EOF, which is merely where the pass ran out of source and noticed.
//  The macro case has MacroDefinition::lineNumber; the conditional case has
//  ConditionalState::openLineNumber, carried for exactly this.
//
//  Unclosed macros and unclosed conditionals alike get one error per open level
//  rather than a single "3 level(s) open" summary: each one is separately
//  missing its own closer, so each is separately somewhere to go. Emitted in
//  ascending line order, the order errors are read in -- which falls out of
//  walking each stack forward, since both are outermost-first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ValidateAssemblyCompletion()
{
    HRESULT hr = S_OK;



    ReportSubsetBoundaryRefusals();

    // These diagnostics are DEFERRED: the construct opened arbitrarily far back,
    // and by now the ambient source file is whichever was processed last. So the
    // file is restored from where the construct OPENED, exactly as the line
    // number already was. Without this an IF opened inside an included file is
    // reported with the right line and the wrong file, which is a stronger
    // version of blaming the end of the file.
    // One error per open definition, each at its own opening line, for the same
    // reason the conditional stack below gets one per level: several can be open
    // at once now that a definition may share the terminator of the one after
    // it, and each is separately missing one. Walking the stack forward gives
    // ascending line order, which is the order they are read in.
    for (const MacroDefinition & pending : m_pendingMacros)
    {
        m_currentSourceFile = pending.sourceFile;
        m_diagnosticColumn  = pending.openColumn;

        RecordError (pending.lineNumber, "Unclosed macro definition: " + pending.name);
    }

    if (m_pass1State == Pass1State::CollectingLoop)
    {
        m_currentSourceFile = m_loopFile;
        m_diagnosticColumn  = m_loopColumn;

        RecordError (m_loopLine, "Unclosed " + m_loopDirective + " block (no matching terminator)");
    }

    if (m_inDummySection)
    {
        m_currentSourceFile = m_dummyFile;
        m_diagnosticColumn  = m_dummyColumn;

        RecordError (m_dummyLine, "Unclosed " + m_dummyDirective + " section (no matching terminator)");
    }

    // One error per unclosed level, each at the line its own IF opened on --
    // every one of them is missing an ENDIF, so every one is a place to go.
    //
    // Forward through the stack, which is outermost-first and so ascending by
    // line: errors are read in source order, like every other diagnostic here.
    for (const ConditionalState & open : m_condStack)
    {
        m_currentSourceFile = open.openFile;
        m_diagnosticColumn  = open.openColumn;

        RecordError (open.openLineNumber, "Unclosed if block (no matching endif)");
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleMultiNop
//
//  as65's `NOP <count>`, which emits `count` NOPs rather than one. Rewritten
//  here into a synthetic .MULTINOP directive so pass 2 emits it through the
//  ordinary directive path instead of needing an instruction special case.
//
//  `handled` is set even when the count does not resolve or is not positive:
//  the line WAS a multi-NOP, and letting the instruction path re-read it would
//  try to encode `NOP` with an operand and report a bogus addressing-mode
//  error on top of the real one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleMultiNop (const PendingLine & current, LineInfo & info, bool & handled)
{
    HRESULT      hr       = S_OK;
    std::string  multiNop = m_dialect.GetMultiNopMnemonic();



    handled = false;



    // Only the repeat form is a multi-NOP; the bare mnemonic is an ordinary
    // opcode, and a dialect with no such form claims no line at all.
    //
    // This is the second dual-purpose spelling, the other being the RMB branch
    // in the AS65 profile. They stay apart rather than sharing a table because
    // the table could not hold what separates them: RMB splits on the operand's
    // *shape* (a comma means the Rockwell instruction), which the parser can
    // see, while this one splits on the operand's *value*, which needs the
    // expression evaluator and the pass-1 symbol table. Both are dialect facts,
    // which is why the SPELLINGS come from the profile and only the decision
    // stays here.
    BAIL_OUT_IF (multiNop.empty() || ToUpperCase (info.parsed.mnemonic) != multiNop ||
                 info.parsed.operand.empty(), S_OK);

    {
        ExprResult  er;

        m_pass1Ctx.currentPC = (int32_t) m_pc;
        er = ExpressionEvaluator::Evaluate (info.parsed.operand, m_pass1Ctx);

        if (er.success && er.value > 0)
        {
            info.isDirective           = true;
            info.parsed.isDirective    = true;
            info.parsed.directive      = m_dialect.GetSpellingForDirective (Directive::MultiNop);
            info.parsed.directiveToken = Directive::MultiNop;
            info.parsed.directiveArg   = info.parsed.operand;
            ReserveBytes ((Word) er.value);
        }

        handled = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RunPass2
//
//  Emits bytes, now that pass 1 has fixed every address. Walks m_lineInfos --
//  the record pass 1 built -- rather than the source, so macro expansion and
//  includes are already flattened and nothing is re-parsed.
//
//  The image is a full 64 KB pre-filled with m_options.fillByte, so gaps left
//  by .org jumps carry a defined value rather than whatever was there;
//  ExtractImage trims it to the range actually written.
//
//  Order matters at the top: every pass-1 symbol is copied into the pass-2
//  context, THEN .EQU values are resolved, THEN unresolved ones are reported.
//  Resolution needs the labels present, and reporting has to come after
//  resolution or it would blame constants that were about to resolve.
//
//  A line already carrying an error emits nothing but still gets a listing
//  entry, so the listing stays line-for-line with the source.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RunPass2()
{
    HRESULT hr = S_OK;



    m_image.assign (65536, m_options.fillByte);

    // Build full expression context with all symbols
    for (const auto & sym : m_symbols)
    {
        m_fullSymbols[sym.first] = (int32_t) sym.second;
    }

    hr = ResolveEquConstants();
    CHR (hr);

    hr = ReportUnresolvedEqus();
    CHR (hr);

    for (const auto & info : m_lineInfos)
    {
        // Bytes go where pass 1 said they would land, which is NOT the address
        // the line runs at once a relocating origin has moved the two apart.
        // Expressions on the line still see info.pc, because that is what a
        // label on it bound to and what a branch from it is computed against.
        Word emitPCStart    = info.outputPos;
        Word emitPC         = info.outputPos;
        bool lineHasAddress = false;

        // Pass 2 walks the recorded lines rather than the pending ones, so the
        // originating file has to be re-established here or every diagnostic
        // raised while emitting would be attributed to the top-level input. The
        // column travels with it, for the same reason and from the same record.
        m_currentSourceFile = info.sourceFile;
        m_diagnosticColumn  = PrimaryColumn (info.parsed);

        // Same reasoning for the instruction set: REPLAY what pass 1 recorded
        // for this line rather than re-deriving it. Emitting against a
        // different table than the one that sized the line is how an operand
        // ends up the wrong width.
        m_opcodeTable = info.usedExtendedSet ? &m_instructionSets.GetExtended()
                                             : &m_instructionSets.GetBase();

        // A reassignable symbol takes its value again here, so a reference sees
        // what was assigned most recently BEFORE it rather than what the file
        // assigned last.
        hr = RebindMutableConstant (info);
        CHR (hr);

        if (info.hasError)
        {
            // Nothing to emit
        }
        else if (info.emitsNoBytes)
        {
            // A dummy section's lines were sized and addressed and must produce
            // no bytes. The output cursor never advanced across them, so every
            // one of them would emit on top of whatever the previous real line
            // placed -- which is silent wrong output rather than a diagnostic.
            // They still carry an address, because that is the whole point of
            // having assembled them.
            lineHasAddress = info.isDirective || info.isInstruction || !info.parsed.label.empty();
        }
        else if (info.isDirective)
        {
            lineHasAddress = true;
            m_pass2Ctx.currentPC = (int32_t) info.pc;
            hr = EmitDirectiveBytes (info, emitPC);
            CHR (hr);
        }
        else if (info.isInstruction)
        {
            lineHasAddress = true;
            m_pass2Ctx.currentPC = (int32_t) info.pc;
            hr = EmitInstruction (info, emitPC);
            CHR (hr);
        }
        else if (!info.parsed.label.empty())
        {
            lineHasAddress = true;
        }

        NoteSpanEmission (info, emitPCStart, emitPC);

        hr = BuildListingEntry (info, emitPCStart, emitPC, lineHasAddress);
        CHR (hr);
    }

    // Whatever is still accumulating becomes the last output. A source that
    // never cut a span reaches here with all of its bytes in one, which is why
    // an ordinary assembly needs no separate path.
    CloseSpan (std::string());

    ReportDuplicateOutputNames();

    hr = ExtractImage();
    CHR (hr);

    m_result.symbols     = m_symbols;
    m_result.symbolKinds = m_symbolKinds;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ResolveEquConstants
//
//  Settles .EQU values that pass 1 could not, by sweeping repeatedly until a
//  full sweep resolves nothing new.
//
//  Iteration is what allows one .EQU to be defined in terms of another
//  declared later: each pass resolves whatever now has all its inputs, which
//  makes more inputs available to the next. A single ordered pass would only
//  work if constants were declared in dependency order.
//
//  Already-resolved names are skipped, so a value is computed once and a
//  resolved constant cannot be recomputed against a changed context.
//
//  The 100-iteration cap bounds a circular definition (A equ B / B equ A),
//  which makes no progress and would otherwise spin. Nothing is reported
//  here: whatever is still unresolved falls to ReportUnresolvedEqus, which
//  cannot tell a cycle from a genuinely undefined symbol and does not need to
//  -- both are "this never resolved" to the author.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ResolveEquConstants()
{
    HRESULT hr = S_OK;



    bool madeProgress = true;
    int  iterations   = 0;



    while (madeProgress && iterations < 100)
    {
        madeProgress = false;
        iterations++;

        for (const auto & info : m_lineInfos)
        {
            const std::string & expr = info.parsed.constantExpr;

            if (!info.isConstant || !info.parsed.isConstant)
            {
                continue;
            }

            if (info.parsed.constantKind != SymbolKind::Equ)
            {
                continue;
            }

            if (m_fullSymbols.find (info.parsed.constantName) != m_fullSymbols.end())
            {
                continue;
            }

            if (expr.size() >= 2 && expr.front() == '"' && expr.back() == '"')
            {
                int32_t len = (int32_t) (expr.size() - 2);
                m_symbols[info.parsed.constantName]     = (Word) len;
                m_fullSymbols[info.parsed.constantName] = len;
                madeProgress = true;
            }
            else
            {
                ExprResult  er;

                m_pass2Ctx.currentPC = (int32_t) info.pc;
                er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, m_pass2Ctx);

                if (er.success)
                {
                    m_symbols[info.parsed.constantName]     = (Word) er.value;
                    m_fullSymbols[info.parsed.constantName] = er.value;
                    madeProgress = true;
                }
            }
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RebindMutableConstant
//
//  Gives a reassignable symbol its value again at the point pass 2 reaches its
//  definition, so every reference resolves against the assignment most recently
//  before it.
//
//  Without this the two passes disagree about what the symbol holds. Pass 1
//  walks the file in order and sees each assignment in turn -- which is exactly
//  what sizes the lines between them -- while pass 2 reads one table built after
//  pass 1 finished, so every reference resolves to the LAST value the file ever
//  assigned. An immutable symbol cannot show the difference, which is why this
//  went unnoticed; a reassignable one shows it as wrong bytes and no diagnostic.
//
//  A reassignable LABEL is the same problem with the program counter for an
//  expression, and it is the more damaging half: every one of CLOCK.S's eight
//  `]LOOP` branches would otherwise be computed against the last of them, which
//  assembles cleanly into eight wrong branch offsets.
//
//  An expression that will not evaluate leaves the previous value standing
//  rather than clearing the symbol. The failure is reported where the constant
//  is defined; blanking it here would add a second, stranger complaint at every
//  reference.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RebindMutableConstant (const LineInfo & info)
{
    HRESULT     hr        = S_OK;
    bool        isMutable = info.isConstant && info.parsed.isConstant &&
                            (info.parsed.constantKind == SymbolKind::Set);
    bool        isLabel   = !info.parsed.label.empty() &&
                            (info.parsed.labelKind == SymbolKind::Set);
    ExprResult  er        = {};



    if (isLabel)
    {
        m_symbols[info.parsed.label]     = info.pc;
        m_fullSymbols[info.parsed.label] = (int32_t) info.pc;
    }

    BAIL_OUT_IF (!isMutable, S_OK);

    m_pass2Ctx.currentPC = (int32_t) info.pc;
    er                   = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, m_pass2Ctx);

    if (er.success)
    {
        m_symbols[info.parsed.constantName]     = (Word) er.value;
        m_fullSymbols[info.parsed.constantName] = er.value;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ReportUnresolvedEqus
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ReportUnresolvedEqus()
{
    HRESULT hr = S_OK;



    for (const auto & info : m_lineInfos)
    {
        if (!info.isConstant || !info.parsed.isConstant)
        {
            continue;
        }

        m_currentSourceFile = info.sourceFile;
        m_diagnosticColumn  = PrimaryColumn (info.parsed);

        if (info.parsed.constantKind == SymbolKind::Equ &&
            m_fullSymbols.find (info.parsed.constantName) == m_fullSymbols.end())
        {
            // The expression is the subject, so the diagnostic points at it
            // rather than at the sign that assigned it.
            RecordErrorAt (info.parsed.lineNumber, info.parsed.operandColumn,
                "Cannot resolve equ expression: " + info.parsed.constantExpr);
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitDirectiveBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitTextDirective (const LineInfo & info, Word & emitPC)
{
    std::string  text = Parser::ParseQuotedString (info.parsed.directiveArg);



    for (char c : text)
    {
        EmitByte (m_charMap.table[(unsigned char) c], emitPC);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitStringDirective
//
//  The bytes an encoded-string directive produces, from exactly the encoder pass
//  1 sized the line with.
//
//  A failure here is silent on purpose: pass 1 already reported it against the
//  same operand text, and reporting again would print every malformed string
//  twice.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitStringDirective (const LineInfo & info, Word & emitPC)
{
    std::vector<Byte>  bytes;
    std::string        error;
    bool               encoded = TryEncodeStringOperand (info.parsed, bytes, error);



    IGNORE_RETURN_VALUE (encoded, false);

    for (Byte value : bytes)
    {
        EmitByte (value, emitPC);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitErrorIfDirective
//
//  Merlin's assembly-time assertion: the expression is evaluated, and a NON-ZERO
//  result fails the assembly.
//
//  In pass 2 rather than pass 1, because the expressions worth asserting on are
//  about what was assembled -- `ERR END-LABTBL-1/$700` bounds a table by the
//  distance between its own two labels -- and one of those labels is a forward
//  reference at the point the directive is read.
//
//  It emits no bytes. It rides the pass-2 emitter table anyway because that is
//  the pass in which every symbol is known, and the table is what says which
//  pass a directive acts in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitErrorIfDirective (const LineInfo & info, Word & /*emitPC*/)
{
    ExprResult  er      = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass2Ctx);
    bool        asserts = false;



    if (!er.success)
    {
        RecordErrorAt (info.parsed.lineNumber, info.parsed.operandColumn,
                       info.parsed.directive + " expression must be resolvable: " + er.error);
    }
    else
    {
        asserts = (er.value != 0);

        if (asserts)
        {
            RecordError (info.parsed.lineNumber,
                         "Assembly-time assertion failed: " + info.parsed.directiveArg
                         + " evaluated to " + std::to_string (er.value));
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitMultiNopDirective
//
//  `nop <count>` collapses to that many $EA bytes. The count is re-evaluated
//  in pass 2 because it may reference a label only resolved by then.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitMultiNopDirective (const LineInfo & info, Word & emitPC)
{
    ExprResult  er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass2Ctx);
    int32_t     j  = 0;



    if (er.success && er.value > 0)
    {
        for (j = 0; j < er.value; j++)
        {
            EmitByte (0xEA, emitPC);
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitDirectiveBytes
//
//  Pass-2 dispatch for directives: looks the token up in the directive table
//  and calls whatever member function sits in its pass2 column.
//
//  A null pass2 column is not an error -- it means the directive emits no
//  bytes, either because it did all its work in pass 1 (.ORG, .LIST, .STRUCT)
//  or because it never produces output. Bailing on null is what lets those
//  share the same dispatch as the emitters.
//
//  The assert catches the table drifting out of enum order, which would
//  silently dispatch one directive to another's emitter -- indexing by token
//  is only sound while row.token == token holds for every row.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitDirectiveBytes (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;



    Directive         token   = info.parsed.directiveToken;
    Pass2DirectiveFn  emitter = nullptr;



    if (token > Directive::None && token < Directive::Count)
    {
        const DirectiveRow &  row = GetDirectiveRows()[(size_t) token];

        ASSERT (row.token == token);   // the table drifted out of enum order
        emitter = row.pass2;
    }

    // A null pass-2 column means the directive emits nothing: it either did
    // all its work in pass 1 (.ORG, .LIST, .STRUCT) or never produces bytes.
    BAIL_OUT_IF (emitter == nullptr, S_OK);

    hr = (this->*emitter) (info, emitPC);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitByteDirective
//
//  .BYTE and friends, where each argument is a string, an escape run, or an
//  expression. Four cases rather than two because the argument splitter is
//  comma-based and strings can contain commas or trailing bytes.
//
//  Strings go through m_charMap -- the .CMAP translation table -- so source
//  text can be emitted in the target's encoding. Anything NOT a string does
//  not: an escape run or an expression result is already a byte value, and
//  translating it would corrupt data that was never text. That asymmetry is
//  the point of the separate branches.
//
//  A `"str"suffix` form emits the quoted part translated and the suffix
//  untranslated, which is how a string with a terminator or high-bit marker
//  is written on one line. An opening quote with no closing one falls through
//  to expression evaluation rather than erroring, so `"` as a character
//  literal still works.
//
//  A QUOTED RUN IS NOT TEXT IN EVERY DIALECT, and which it is comes off the
//  evaluation context rather than being decided here. A dialect that spells a
//  character constant with the quotation mark has already given the evaluator
//  that delimiter, so `"A"` is one character with bit 7 set and not a
//  one-character string -- Merlin's byte directive takes an EXPRESSION, and
//  reading it as text emits the wrong byte while looking entirely reasonable.
//  A dialect with no such spelling answers 0, which matches no argument, so the
//  text branches stand exactly as they were.
//
//  A failed evaluation records the error and keeps going, so one bad argument
//  in a long table reports itself without hiding the rest. m_result.success is
//  cleared once at the end rather than per-failure.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitByteDirective (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;



    auto args      = Parser::SplitArgList (info.parsed.directiveArg);
    char charQuote = m_pass2Ctx.highAsciiCharDelimiter;
    bool ok        = true;



    for (const auto & arg : args)
    {
        bool isText = (arg.size() >= 2) && (arg.front() == '"') && (arg.front() != charQuote);

        if (isText && arg.back() == '"')
        {
            std::string raw       = arg.substr (1, arg.size() - 2);
            std::string processed = ProcessEscapeSequences (raw);

            for (char c : processed)
            {
                EmitByte (m_charMap.table[(unsigned char) c], emitPC);
            }
        }
        else if (isText)
        {
            size_t closeQuote = arg.find ('"', 1);

            if (closeQuote != std::string::npos)
            {
                std::string  raw             = arg.substr (1, closeQuote - 1);
                std::string  processed       = ProcessEscapeSequences (raw);
                std::string  suffix;
                std::string  suffixProcessed;

                for (char c : processed)
                {
                    EmitByte (m_charMap.table[(unsigned char) c], emitPC);
                }

                suffix = arg.substr (closeQuote + 1);
                suffixProcessed = ProcessEscapeSequences (suffix);

                for (char c : suffixProcessed)
                {
                    EmitByte ((Byte) (unsigned char) c, emitPC);
                }
            }
            else
            {
                ExprResult er = ExpressionEvaluator::Evaluate (arg, m_pass2Ctx);

                if (!er.success)
                {
                    RecordError (info.parsed.lineNumber,
                        "Cannot evaluate expression: " + arg + " (" + er.error + ")");
                    ok = false;
                }
                else
                {
                    EmitByte ((Byte) (er.value & 0xFF), emitPC);
                }
            }
        }
        else if (arg.size() >= 2 && arg[0] == '\\')
        {
            std::string processed = ProcessEscapeSequences (arg);

            for (char c : processed)
            {
                EmitByte ((Byte) (unsigned char) c, emitPC);
            }
        }
        else
        {
            ExprResult er = ExpressionEvaluator::Evaluate (arg, m_pass2Ctx);

            if (!er.success)
            {
                RecordError (info.parsed.lineNumber,
                    "Cannot evaluate expression: " + arg + " (" + er.error + ")");
                ok = false;
            }
            else
            {
                EmitByte ((Byte) (er.value & 0xFF), emitPC);
            }
        }
    }

    if (!ok)
    {
        m_result.success = false;
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitWordDirective
//
//  Two bytes per value, little-endian low byte first -- the 6502's byte order,
//  so a .WORD holding an address can be read directly by the target.
//
//  The `values.size() != 0 || arg.empty()` guard distinguishes "no arguments"
//  (legal, emits nothing) from "arguments that all failed to evaluate"
//  (TryEvaluateDirectiveArgs already recorded why; this only has to fail the
//  assembly). Without it an unevaluable .WORD would silently emit nothing and
//  every following address would be two bytes early.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitWordDirective (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;



    std::vector<int32_t> values;



    TryEvaluateDirectiveArgs (info.parsed.directiveArg, m_pass2Ctx, values, info.parsed.lineNumber, m_result.errors);

    if (values.size() != 0 || info.parsed.directiveArg.empty())
    {
        for (int32_t v : values)
        {
            EmitByte ((Byte) (v & 0xFF), emitPC);
            EmitByte ((Byte) ((v >> 8) & 0xFF), emitPC);
        }
    }
    else
    {
        m_result.success = false;
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitWordHighFirstDirective
//
//  Two bytes per value, HIGH byte first -- the reverse of the processor's own
//  order, and the entire reason the directive exists beside the ordinary one.
//
//  It is not a 6502 address layout. The order is what a jump table read by
//  pushing an address onto the stack wants, and what data shared with a
//  big-endian format wants, so a dialect offering both spellings is offering two
//  different data layouts rather than a synonym.
//
//  Everything else matches the ordinary word directive exactly, including the
//  "no arguments" versus "arguments that all failed" distinction: without it an
//  unevaluable line would silently emit nothing and every following address
//  would be two bytes early.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitWordHighFirstDirective (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;



    std::vector<int32_t> values;



    TryEvaluateDirectiveArgs (info.parsed.directiveArg, m_pass2Ctx, values, info.parsed.lineNumber, m_result.errors);

    if (values.size() != 0 || info.parsed.directiveArg.empty())
    {
        for (int32_t v : values)
        {
            EmitByte ((Byte) ((v >> 8) & 0xFF), emitPC);
            EmitByte ((Byte) (v & 0xFF), emitPC);
        }
    }
    else
    {
        m_result.success = false;
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitDdDirective
//
//  Four bytes per value, little-endian, same order and same empty-versus-
//  failed distinction as EmitWordDirective above.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitDdDirective (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;



    std::vector<int32_t> values;



    TryEvaluateDirectiveArgs (info.parsed.directiveArg, m_pass2Ctx, values, info.parsed.lineNumber, m_result.errors);

    if (values.size() != 0 || info.parsed.directiveArg.empty())
    {
        for (int32_t v : values)
        {
            EmitByte ((Byte) (v & 0xFF), emitPC);
            EmitByte ((Byte) ((v >> 8) & 0xFF), emitPC);
            EmitByte ((Byte) ((v >> 16) & 0xFF), emitPC);
            EmitByte ((Byte) ((v >> 24) & 0xFF), emitPC);
        }
    }
    else
    {
        m_result.success = false;
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitDsDirective
//
//  .DS reserves space by emitting `size` copies of a fill byte -- it does not
//  merely advance the PC. That matters because the reserved run then holds a
//  known value in the image rather than whatever the pre-fill left, so a
//  buffer declared with .DS 256, $FF really is $FF in the output.
//
//  The optional second argument is the fill; absent, it is zero. A fill
//  expression that fails to evaluate leaves the default rather than abandoning
//  the reservation, because the SIZE is what the following addresses depend
//  on -- emitting the right number of wrong bytes keeps every later label
//  correct, while emitting none would shift all of them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitDsDirective (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;



    auto args = Parser::SplitArgList (info.parsed.directiveArg);



    if (!args.empty())
    {
        ExprResult sizeEr = ExpressionEvaluator::Evaluate (args[0], m_pass2Ctx);

        if (sizeEr.success)
        {
            Byte fillVal = 0;

            if (args.size() >= 2)
            {
                ExprResult fillEr = ExpressionEvaluator::Evaluate (args[1], m_pass2Ctx);

                if (fillEr.success)
                {
                    fillVal = (Byte) (fillEr.value & 0xFF);
                }
            }

            for (int32_t j = 0; j < sizeEr.value; j++)
            {
                EmitByte (fillVal, emitPC);
            }
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitAlignDirective
//
//  Pads forward to the next multiple of `alignment`, defaulting to 2. The
//  padding is emitted as m_options.fillByte rather than skipped, for the same
//  reason as .DS: the gap ends up with a defined value in the image.
//
//  Pass 1 already advanced the PC by the same amount, so this has to agree
//  with it exactly -- both compute alignment - (pc % alignment). If the two
//  ever disagreed, every label after the .ALIGN would be at an address the
//  emitted bytes do not occupy.
//
//  A bad or absent expression falls back to 2 rather than erroring, and
//  alignment <= 0 pads nothing (guarding the modulo, which would be undefined
//  at zero).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitAlignDirective (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;



    int alignment = 2;



    if (!info.parsed.directiveArg.empty())
    {
        ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass2Ctx);

        if (er.success)
        {
            alignment = er.value;
        }
    }

    if (alignment > 0)
    {
        int remainder2 = info.pc % alignment;

        if (remainder2 != 0)
        {
            int  padding = alignment - remainder2;
            Byte fillVal = m_options.fillByte;

            for (int j = 0; j < padding; j++)
            {
                EmitByte (fillVal, emitPC);
            }
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitInstruction
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitInstruction (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;



    int32_t value = 0;
    bool    emit  = true;



    hr = ResolveInstructionValue (info, value, emit);
    CHR (hr);

    if (emit)
    {
        hr = EmitInstructionBytes (info, value, emitPC);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ResolveInstructionValue
//
//  Produces the operand value an instruction will encode, from whichever
//  source has it: pass 1 already resolved it, or the expression is evaluated
//  now against the full symbol table.
//
//  Pass 1 resolving it does not make the expression uninteresting -- the
//  symbols it names still count as referenced, which is what feeds
//  DetectUnusedLabels. Skipping that on the already-resolved path would report
//  every label used only by an early-resolved instruction as unused.
//
//  Accumulator and no-operand forms are excluded from evaluation because they
//  have no expression to evaluate; asking would produce an error for a
//  perfectly valid `ASL A`.
//
//  `emit` is what a failure clears, so the caller skips the byte emission
//  without treating it as an infrastructure fault -- the error is already
//  recorded and the assembly continues.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ResolveInstructionValue (const LineInfo & info, int32_t & value, bool & emit)
{
    HRESULT hr = S_OK;



    GlobalAddressingMode::AddressingMode mode = info.resolvedMode;

    value = 0;
    emit  = true;



    if (info.valueResolved)
    {
        value = info.resolvedValue;

        if (!info.classified.expression.empty())
        {
            for (const auto & sym : m_symbols)
            {
                if (info.classified.expression.find (sym.first) != std::string::npos)
                {
                    m_referencedLabels[sym.first] = info.parsed.lineNumber;
                }
            }
        }
    }
    else if (info.classified.syntax != OperandSyntax::None &&
             info.classified.syntax != OperandSyntax::Accumulator &&
             !info.classified.expression.empty())
    {
        ExprResult er = ExpressionEvaluator::Evaluate (info.classified.expression, m_pass2Ctx);

        if (!er.success)
        {
            OpcodeEntry entry = {};

            RecordError (info.parsed.lineNumber,
                "Undefined symbol in: " + info.classified.expression);
            emit = false;


            if (m_opcodeTable->TryLookup (info.parsed.mnemonic, mode, entry))
            {
                // Emit placeholder bytes handled by caller
            }
        }
        else
        {
            value = er.value;

            for (const auto & sym : m_symbols)
            {
                if (info.classified.expression.find (sym.first) != std::string::npos)
                {
                    m_referencedLabels[sym.first] = info.parsed.lineNumber;
                }
            }
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitInstructionBytes
//
//  Opcode plus operand, with the two addressing modes that need arithmetic
//  handled before the general case.
//
//  Relative: a branch encodes a signed displacement from the address AFTER
//  the instruction, hence pc + 2 rather than pc. Out of range is reported but
//  the truncated byte is still emitted, so the image keeps the size pass 1
//  reserved and every later label stays at its computed address -- one bad
//  branch reports itself instead of shifting the rest of the file.
//
//  ZeroPageRelative (65C02 BBRn/BBSn) carries TWO operands: `value` is the
//  already-resolved zero-page address, while the branch target is a second
//  expression evaluated here. Its displacement is from pc + 3, since the
//  instruction is three bytes. All three bytes are emitted even when the
//  target fails to resolve, for the same size-stability reason.
//
//  Everything else takes its operand width from the opcode table rather than
//  from the mode, and writes little-endian for the two-byte forms.
//
//  Symbols named in the second expression are marked referenced here, because
//  ResolveInstructionValue only ever sees the first one -- without this, a
//  label used solely as a BBRn branch target would be reported unused.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitInstructionBytes (const LineInfo & info, int32_t value, Word & emitPC)
{
    HRESULT hr = S_OK;



    GlobalAddressingMode::AddressingMode mode = info.resolvedMode;



    if (mode == GlobalAddressingMode::Relative)
    {
        Word  pcAfterInstruction = info.pc + 2;
        int   offset             = value - (int) pcAfterInstruction;

        if (offset < -128 || offset > 127)
        {
            RecordError (info.parsed.lineNumber, "Branch target out of range");
        }

        value = offset & 0xFF;
    }

    if (mode == GlobalAddressingMode::ZeroPageRelative)
    {
        // 65C02 BBRn/BBSn: three bytes — opcode, a zero-page address byte, then a
        // relative offset to the branch target. `value` already holds the resolved
        // zero-page address (first operand); the branch target is the second
        // operand, evaluated here against the fully-populated pass-2 symbol table.
        // Always emit exactly three bytes so the image stays aligned with the size
        // reserved in pass 1, even when an operand fails to resolve.
        OpcodeEntry entry       = {};
        Byte        offsetByte  = 0;
        bool        hasEncoding = false;

        hasEncoding = m_opcodeTable->TryLookup (info.parsed.mnemonic, mode, entry);

        if (!hasEncoding)
        {
            RecordError (info.parsed.lineNumber, "Cannot encode: " + info.parsed.mnemonic);
        }
        else
        {
            ExprResult er = ExpressionEvaluator::Evaluate (info.classified.secondExpression, m_pass2Ctx);

            if (!er.success)
            {
                RecordError (info.parsed.lineNumber,
                    "Undefined symbol in: " + info.classified.secondExpression);
            }
            else
            {
                int offset = er.value - (int) (info.pc + 3);

                if (offset < -128 || offset > 127)
                {
                    RecordError (info.parsed.lineNumber, "Branch target out of range");
                }

                offsetByte = (Byte) (offset & 0xFF);

                for (const auto & sym : m_symbols)
                {
                    if (info.classified.secondExpression.find (sym.first) != std::string::npos)
                    {
                        m_referencedLabels[sym.first] = info.parsed.lineNumber;
                    }
                }
            }

            EmitByte (entry.opcode, emitPC);
            EmitByte ((Byte) (value & 0xFF), emitPC);   // zero-page address
            EmitByte (offsetByte, emitPC);              // relative branch offset
        }
    }
    else
    {
        OpcodeEntry entry   = {};
        std::string encoded = EncodedMnemonic (info);

        if (!m_opcodeTable->TryLookup (encoded, mode, entry))
        {
            RecordError (info.parsed.lineNumber, "Cannot encode: " + info.parsed.mnemonic);
        }
        else
        {
            EmitByte (entry.opcode, emitPC);

            if (entry.operandSize == 1)
            {
                EmitByte ((Byte) (value & 0xFF), emitPC);
            }
            else if (entry.operandSize == 2)
            {
                EmitByte ((Byte) (value & 0xFF), emitPC);
                EmitByte ((Byte) ((value >> 8) & 0xFF), emitPC);
            }
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::BuildListingEntry
//
//  One listing row per source line, called for EVERY line pass 2 walks --
//  including lines that emitted nothing -- so the listing stays line-for-line
//  with the source rather than only showing lines that produced bytes.
//
//  The emitted bytes are read back out of m_image between the PC before and
//  after, rather than accumulated as they are written. That way the listing
//  shows what actually landed in the image, which is the thing being
//  verified; anything that wrote by another route still appears.
//
//  Cycle counts are attached only to instructions that both succeeded and
//  emitted, since a failed or zero-byte line has no timing to report.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::BuildListingEntry (const LineInfo & info, Word emitPCStart, Word emitPC,
                                             bool lineHasAddress)
{
    HRESULT hr = S_OK;



    // Counted BEFORE both bails below, because this is how many lines were
    // assembled rather than how many were listed. The two differ whenever no
    // listing was asked for -- which is the ordinary case -- and again inside
    // a suppressed region, whose lines are assembled and deliberately not
    // shown.
    m_result.linesAssembled++;

    BAIL_OUT_IF (!m_options.generateListing, S_OK);

    // A suppressed line still lists when it was skipped by a conditional, so
    // the listing shows which branch was taken.
    BAIL_OUT_IF (info.listingSuppressed && !info.conditionalSkip, S_OK);

    {
        AssemblyLine listLine = {};
        listLine.lineNumber = info.parsed.lineNumber;

        if (info.parsed.lineNumber >= 1 && info.parsed.lineNumber <= (int) m_lines.size())
        {
            listLine.sourceText = m_lines[info.parsed.lineNumber - 1];
        }

        listLine.hasAddress        = lineHasAddress;
        listLine.address           = info.pc;
        listLine.isMacroExpansion  = (info.macroDepth > 0);
        listLine.isConditionalSkip = info.conditionalSkip;

        if (info.isInstruction && !info.hasError && emitPC > emitPCStart)
        {
            OpcodeEntry cycleEntry = {};
            std::string encoded    = EncodedMnemonic (info);

            // The instruction the image CONTAINS, which is not the one the
            // source wrote when pass 1 replaced a jump with a branch. Reporting
            // the written one would put a jump's timing beside a branch's bytes.
            if (m_opcodeTable->TryLookup (encoded, info.resolvedMode, cycleEntry))
            {
                listLine.cycleCounts = cycleEntry.cycleCounts;
            }
        }

        for (Word j = emitPCStart; j < emitPC; j++)
        {
            listLine.bytes.push_back (m_image[j]);
        }

        m_result.listing.push_back (listLine);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::NoteSpanEmission
//
//  Tells the span being accumulated that a line placed bytes in it.
//
//  THE ADDRESS IS TAKEN FROM THE FIRST LINE THAT EMITS, not from wherever the
//  span opened. A span may open on a save and then meet an origin before any
//  byte lands, and it is where the bytes go that a later load cares about.
//
//  It is the program counter that is recorded rather than the output cursor.
//  The two agree until a relocating origin separates them, after which the
//  bytes sit at the cursor and belong at the counter.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::NoteSpanEmission (const LineInfo & info, Word emitPCStart, Word emitPC)
{
    bool  placedBytes = emitPC > emitPCStart;



    if (placedBytes)
    {
        if (!m_spanHasBytes)
        {
            m_spanOutputStart = emitPCStart;
            m_spanLoadAddress = info.pc;
            m_spanHasBytes    = true;
        }

        m_spanOutputEnd = emitPC;
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CloseSpan
//
//  Ends the span being accumulated and appends it as one output.
//
//  A span that placed no bytes yields nothing rather than an empty output. Two
//  saves in a row, or a save with nothing between it and the one above, would
//  otherwise each produce a zero-length file that no source asked for.
//
//  The bytes are cut from the OUTPUT and the address comes from the program
//  counter, which are the same number until a relocating origin separates them.
//  After one, a span sits at the output cursor and loads where its origin said,
//  and it is the latter that a loader needs.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::CloseSpan (const std::string & name)
{
    SavePoint    span;
    size_t       first     = m_spanOutputStart;
    size_t       last      = m_spanOutputEnd;
    bool         hasBytes  = m_spanHasBytes && (last > first);
    bool         isFirst   = m_result.savePoints.empty();
    std::string  effective = name.empty() ? m_objectFileInEffect : name;
    bool         named     = !effective.empty();



    //  An unnamed span AFTER something else was already saved is dropped, and
    //  said so. That is the period assembler's behavior, measured: it assembles
    //  such bytes and counts them and writes no file for them. Dropping them
    //  silently is the part that would be wrong, so the warning carries what the
    //  missing file would have.
    if (hasBytes && !named && !isFirst)
    {
        RecordWarning (m_lastSourceLine,
                       "bytes were assembled after the last save and no output names them, so they were not written");
    }

    if (hasBytes && (named || isFirst))
    {
        span.bytes.assign (m_image.begin() + first, m_image.begin() + last);
        span.loadAddress    = m_spanLoadAddress;
        span.hasLoadAddress = true;
        span.name           = effective;
        span.fileType       = m_fileType;
        span.hasFileType    = m_hasFileType;

        m_result.savePoints.push_back (span);
    }

    m_spanHasBytes = false;

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ReportDuplicateOutputNames
//
//  Two outputs of one assembly under one name, which is refused.
//
//  DELIBERATELY NOT THE SAME CASE as a name already on the target from an
//  earlier run. Replacing across runs is what a build loop needs and happens
//  every time after the first; replacing within one run throws away an output
//  the source just asked for, and the assembly would report success having
//  written one file where the source named two.
//
//  Checked once the outputs are known rather than as each is cut, so the
//  diagnostic can name the file rather than the position, and so nothing has
//  been written by the time it is raised.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::ReportDuplicateOutputNames()
{
    size_t  outer = 0;
    size_t  inner = 0;



    //  A direct scan rather than a set. An assembly produces a handful of
    //  outputs at most, so the cost is nothing and the alternative would widen
    //  the precompiled header for one comparison.
    for (outer = 0; outer < m_result.savePoints.size(); outer++)
    {
        const std::string &  name = m_result.savePoints[outer].name;

        if (name.empty())
        {
            continue;
        }

        for (inner = 0; inner < outer; inner++)
        {
            if (m_result.savePoints[inner].name == name)
            {
                RecordError (m_lastSourceLine,
                             "two outputs of this assembly are both called " + name +
                             ", so one would be written over the other");
                break;
            }
        }
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitObjectFile
//
//  The output-file directive, in the pass that places bytes.
//
//  A SECOND ONE CLOSES THE FIRST OUTPUT AND BEGINS ANOTHER. The directive
//  assembles the code that FOLLOWS it into the file it names, so meeting a
//  second one ends the file the first opened -- a source carrying two of them
//  produces two files with no save directive anywhere. Treating a later one as
//  merely renaming the single output is indistinguishable from this for one
//  occurrence and wrong for two.
//
//  It emits no bytes. The pass-2 column is used here the way the assembly-time
//  assertion uses it: to act at the point in the byte stream where the line
//  sits, which is the whole reason this cannot be done in pass 1.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitObjectFile (const LineInfo & info, Word & emitPC)
{
    HRESULT      hr   = S_OK;
    std::string  name = StripCommentAndTrim (info.parsed.directiveArg);



    (void) emitPC;

    CloseSpan (std::string());

    m_objectFileInEffect = name;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitSaveObject
//
//  The save directive, which writes what has accumulated and carries on.
//
//  THE ACCUMULATION IS EMPTIED, so the next output holds only what follows.
//  That is the period assembler's own behavior rather than a choice here: its
//  manual says the object area is empty after a save, and a two-save source
//  measured against it produces two files the size of their own halves.
//
//  The name it gives applies to the span it ends and to no other. An
//  output-file directive still in effect governs the next one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitSaveObject (const LineInfo & info, Word & emitPC)
{
    HRESULT      hr   = S_OK;
    std::string  name = StripCommentAndTrim (info.parsed.directiveArg);



    (void) emitPC;

    CloseSpan (name);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ExtractImage
//
//  Trims the 64 KB working image down to the span actually written, tracked
//  as m_lowestAddr / m_highestAddr by EmitByte. That span is what makes the
//  output a loadable image rather than a 64 KB blob of fill byte.
//
//  It also OVERWRITES m_result.startAddress, which .org set earlier: a source
//  whose first .org is followed by writes below it should load at the lowest
//  byte it actually produced, not at where it declared it would start.
//
//  lowest > highest means nothing was ever emitted -- a source of only
//  comments, equates or skipped conditionals. That yields an empty image
//  rather than a reversed range, and end is pinned to start so the empty
//  result still describes a valid (zero-length) span.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ExtractImage()
{
    HRESULT hr = S_OK;



    if (m_lowestAddr <= m_highestAddr)
    {
        m_result.bytes.assign (m_image.begin() + m_lowestAddr, m_image.begin() + m_highestAddr + 1);
        m_result.startAddress = m_lowestAddr;
        m_result.endAddress   = (Word) (m_highestAddr + 1);
    }
    else
    {
        m_result.bytes.clear();
        m_result.endAddress = m_result.startAddress;
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::DetectUnusedLabels
//
//  Warns about labels nothing refers to -- usually dead code or a rename that
//  missed one site.
//
//  Instruction operands were marked referenced as pass 2 encoded them; this
//  sweeps directive arguments, which pass 2 never had reason to attribute.
//  Both use substring matching rather than re-parsing, so a symbol whose name
//  appears inside a longer identifier counts as referenced. Deliberately
//  lenient: this only drives a warning, and a false "unused" on a label that
//  IS used is far more annoying than missing one that is not.
//
//  Only SymbolKind::Label is considered. An unreferenced .EQU or .SET is
//  ordinary -- headers define constants a given source may not use -- so
//  warning about those would make the whole check noise.
//
//  Runs after pass 2 because that is when the reference set is complete, and
//  warns rather than errors: an unused label assembles perfectly well.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::DetectUnusedLabels()
{
    HRESULT hr = S_OK;



    // Track references in directive expressions
    for (const auto & info : m_lineInfos)
    {
        if (info.isDirective)
        {
            for (const auto & sym : m_symbols)
            {
                if (info.parsed.directiveArg.find (sym.first) != std::string::npos)
                {
                    m_referencedLabels[sym.first] = info.parsed.lineNumber;
                }
            }
        }
    }

    for (const auto & sym : m_symbols)
    {
        auto kindIt = m_symbolKinds.find (sym.first);

        if (kindIt == m_symbolKinds.end() || kindIt->second != SymbolKind::Label)
        {
            continue;
        }

        if (m_referencedLabels.find (sym.first) == m_referencedLabels.end())
        {
            int          defLine   = 0;
            int          defColumn = 0;
            std::string  defFile;

            for (const auto & info : m_lineInfos)
            {
                if (info.parsed.label == sym.first)
                {
                    defLine   = info.parsed.lineNumber;
                    defColumn = info.parsed.labelColumn;
                    defFile   = info.sourceFile;
                    break;
                }
            }

            // The warning belongs to where the label was DEFINED, so the file
            // and the column both come from that line rather than from wherever
            // the sweep happens to be. The label itself is the subject, so the
            // column is its own rather than the line's.
            m_currentSourceFile = defFile;
            m_diagnosticColumn  = defColumn;

            RecordWarning (defLine, "Unused label: " + sym.first);
        }
    }

// Error:
    return hr;
}
