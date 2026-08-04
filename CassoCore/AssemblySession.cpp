#include "Pch.h"

#include "AssemblySession.h"
#include "ExpressionEvaluator.h"
#include "Parser.h"





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
    size_t       start   = code.find_first_not_of (" \t");
    size_t       comment = 0;
    size_t       end     = 0;



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
    size_t       start = text.find_first_not_of (" \t");
    size_t       end   = 0;



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
    std::vector<Byte> data;
    std::istringstream stream (content);
    std::string line;



    while (std::getline (stream, line))
    {
        // Trim trailing CR
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.size() < 2 || line[0] != 'S')
        {
            continue;
        }

        char recType = line[1];

        // S1: 2-byte address, S2: 3-byte address, S3: 4-byte address
        int addrBytes = 0;

        if (recType == '1')      addrBytes = 2;
        else if (recType == '2') addrBytes = 3;
        else if (recType == '3') addrBytes = 4;
        else                     continue;

        if (line.size() < 4)
        {
            continue;
        }

        int byteCount = HexByte (line, 2);

        if (byteCount < 0)
        {
            continue;
        }

        // Data bytes = byteCount - address bytes - 1 checksum byte
        int dataBytes = byteCount - addrBytes - 1;

        if (dataBytes <= 0)
        {
            continue;
        }

        // Data starts after "Sn" + 2-char count + address hex chars
        size_t dataOffset = 4 + (size_t) addrBytes * 2;

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
    std::vector<Byte> data;
    std::istringstream stream (content);
    std::string line;



    while (std::getline (stream, line))
    {
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

        int byteCount  = HexByte (line, 1);
        int recordType = HexByte (line, 7);

        if (byteCount < 0 || recordType < 0)
        {
            continue;
        }

        // Only process data records (type 00)
        if (recordType != 0x00)
        {
            continue;
        }

        size_t dataOffset = 9;

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
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> AssemblySession::GenerateByteDirectives (const std::vector<Byte> & data)
{
    std::vector<std::string> lines;



    static const int kBytesPerLine = 16;



    for (size_t i = 0; i < data.size(); i += kBytesPerLine)
    {
        std::string line = "    .byte ";

        size_t end = std::min (i + kBytesPerLine, data.size());

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
    return m_opcodeTable.HasMode (mnemonic, GlobalAddressingMode::Relative);
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
    // A dialect fact, not a CPU one, which is why it cannot be answered from
    // the opcode table the way IsBranchMnemonic now is: the table holds
    // RMB0..RMB7, and these bare names exist only because as65 spells the bit
    // as an operand. A second dialect supplies a different list here.
    static constexpr std::string_view  s_kBareBitOps[] = { "RMB", "SMB", "BBR", "BBS" };

    bool  found = false;



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
//  And each case built an OpcodeEntry it never read, because Lookup was being
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

        if (m_opcodeTable.HasMode (mnemonic, candidate.mode))
        {
            mode = candidate.mode;
            break;
        }
    }

    return mode;
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
            size = m_opcodeTable.HasMode (mnemonic, GlobalAddressingMode::JumpAbsolute) ? 3 : 2;
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
//  TryEvaluateDirectiveArgs — evaluate comma-separated expression list
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
//  TryEvaluateDirectiveArgs — evaluate comma-separated expression list
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
        // Check for quoted string — emit each character as a value
        if (arg.size() >= 2 && arg.front() == '"' && arg.back() == '"')
        {
            std::string raw       = arg.substr (1, arg.size() - 2);
            std::string processed = ProcessEscapeSequences (raw);

            for (char c : processed)
            {
                values.push_back ((int32_t) (unsigned char) c);
            }

            continue;
        }

        ExprResult er = ExpressionEvaluator::Evaluate (arg, ctx);

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

AssemblySession::AssemblySession (const OpcodeTable & opcodeTable, const AssemblerOptions & options) :
    m_opcodeTable  (opcodeTable),
    m_options      (options),
    m_listingLevel (options.generateListing ? 1 : 0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordError
//
////////////////////////////////////////////////////////////////////////////////

void AssemblySession::RecordError (int lineNumber, const std::string & message)
{
    AssemblyError error = {};
    error.lineNumber = lineNumber;
    error.message    = message;
    m_result.errors.push_back (error);
    m_result.success = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordWarning
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
            m_result.warnings.push_back (warning);
            break;
        }

        case WarningMode::FatalWarnings:
        {
            AssemblyError error = {};
            error.lineNumber = lineNumber;
            error.message    = message;
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
//  AssemblySession::Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::Initialize (const std::string & sourceText)
{
    HRESULT hr = S_OK;



    m_result             = {};
    m_result.success     = true;
    m_result.startAddress = 0;
    m_pc                 = m_result.startAddress;

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
//  AssemblySession::Run
//
////////////////////////////////////////////////////////////////////////////////

AssemblyResult AssemblySession::Run (const std::string & sourceText)
{
    HRESULT hr = S_OK;



    hr = Initialize (sourceText);
    CHR (hr);

    hr = RunPass1();
    CHR (hr);

    hr = RunPass2();
    CHR (hr);

    hr = DetectUnusedLabels();
    CHR (hr);

Error:
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ProcessPass1Line (const PendingLine & current)
{
    HRESULT   hr   = S_OK;
    LineInfo  info = {};



    info.parsed            = Parser::ParseLine (current.text, current.sourceLineNumber);
    info.pc                = m_pc;
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
//  "NAME macro [params]" -- the operand, upper-cased, is MACRO followed by
//  end-of-operand or whitespace.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsMacroDefinitionStart (const ParsedLine & parsed, const std::string & operandUpper)
{
    bool  looksLikeMacro = (operandUpper.substr (0, 5) == "MACRO") &&
                           (operandUpper.size() <= 5 ||
                            operandUpper[5] == ' '  ||
                            operandUpper[5] == '\t');

    return !parsed.mnemonic.empty() && !parsed.isEmpty && looksLikeMacro;
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
//  AssemblySession::IsConditionalLine
//
//  Both spellings are accepted: the dotted directive form, and the bare
//  mnemonic form as65 also allows. The bare form never takes the parser's
//  directive path and so carries no token, which is why it is resolved from
//  the spelling table here rather than read off ParsedLine.
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsConditionalLine (const ParsedLine & parsed)
{
    Directive  token = parsed.isDirective
                           ? parsed.directiveToken
                           : DirectiveTable::FromSpelling (parsed.mnemonic);



    return IsConditionalDirective (token);
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
    Pass1Prelude  kind = Pass1Prelude::None;



    if (IsAssembling() && IsMacroDefinitionStart (info.parsed, operandUpper))
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
    else if (info.parsed.isDirective && info.parsed.directive == ".ORG")
    {
        kind = Pass1Prelude::Org;
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RunPreludeDirectives (const PendingLine & current, LineInfo & info, bool & outClaimed)
{
    HRESULT       hr           = S_OK;
    std::string   operandUpper = ToUpperCase (info.parsed.operand);
    Pass1Prelude  kind         = ClassifyPrelude (info, operandUpper);



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
        info.isDirective = true;
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
    hr = RecordLabel (current, info);
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



    // `.END STRUCT` reaches this as a directive with an argument; bare
    // `end struct` reaches it as a mnemonic with an operand. Both name what
    // they close in the first word of what follows.
    if (info.parsed.isDirective && info.parsed.directive == ".END")
    {
        endsWhat = info.parsed.directiveArg;
    }
    else if (info.parsed.mnemonic == "END")
    {
        endsWhat = info.parsed.operand;
    }

    isEnd = (GetLeadingWord (ToUpperCase (endsWhat)) == "STRUCT");

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
//  FromStorageSpelling rather than FromSpelling: inside a .STRUCT body there is
//  no instruction to be ambiguous with, so `count rmb 4` is unambiguously four
//  reserved bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::GetStructMemberSize (const std::string & operand, int32_t & outSize)
{
    HRESULT                   hr        = S_OK;
    size_t                    split     = operand.find_first_of (" \t");
    Directive                 token     = DirectiveTable::FromStorageSpelling (ToUpperCase (operand.substr (0, split)));
    std::string               countExpr = (split == std::string::npos) ? "" : operand.substr (split);
    size_t                    exprStart = countExpr.find_first_not_of (" \t");
    const StructMemberType *  match     = nullptr;



    outSize   = 0;
    countExpr = (exprStart == std::string::npos) ? countExpr : countExpr.substr (exprStart);



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
            // `.DS <count>` -- the operand carries the width. An expression that
            // does not evaluate falls back to one byte rather than dropping the
            // member, so offsets after it stay plausible for the rest of pass 1.
            m_pass1Ctx.currentPC = (int32_t) m_pc;

            ExprResult  er = ExpressionEvaluator::Evaluate (countExpr, m_pass1Ctx);

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
//  LOCAL is the one directive read on the way past, because its names have to
//  be known before expansion can rename them per-invocation -- otherwise two
//  invocations in the same file would define the same label twice. It is still
//  pushed into the body as well, since expansion re-reads it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::CollectMacroBody (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    std::string mnUpper = info.parsed.mnemonic;



    if (mnUpper == "ENDM" || (info.parsed.isDirective && info.parsed.directive == ".ENDM"))
    {
        MacroDefinition def = {};
        def.name       = m_currentMacroName;
        def.body       = m_currentMacroBody;
        def.paramNames = m_currentMacroParams;
        def.localLabels = m_currentMacroLocals;
        def.lineNumber = m_currentMacroLine;
        m_macros[m_currentMacroName] = def;
        m_pass1State = Pass1State::Normal;
    }
    else
    {
        std::string bodyMn = info.parsed.mnemonic;

        if (bodyMn == "LOCAL" || (info.parsed.isDirective && info.parsed.directive == ".LOCAL"))
        {
            std::string localArg = bodyMn == "LOCAL" ? info.parsed.operand : info.parsed.directiveArg;
            auto localNames = Parser::SplitArgList (localArg);

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
                    m_currentMacroLocals.push_back (name);
                }
            }
        }

        m_currentMacroBody.push_back (current.text);
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
//  A name that collides with a real mnemonic is recorded as an error but the
//  definition still proceeds -- reporting one clear "conflicts with mnemonic"
//  beats abandoning the definition and then emitting an "unknown macro" at
//  every call site, which buries the actual cause.
//
//  `handled` reports whether this line opened a definition; the HRESULT
//  reports only whether the attempt itself ran.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::DetectMacroDefinition (const PendingLine & current, LineInfo & info,
                                                 const std::string & operandUpper, bool & handled)
{
    HRESULT hr = S_OK;

    handled = false;



    // Same predicate ClassifyPass1Line used to route here, so the two cannot
    // disagree about what opens a definition.
    BAIL_OUT_IF (!IsAssembling(), S_OK);
    BAIL_OUT_IF (!IsMacroDefinitionStart (info.parsed, operandUpper), S_OK);

    // Name collision check
    if (m_opcodeTable.IsMnemonic (info.parsed.mnemonic))
    {
        RecordError (current.sourceLineNumber, "Macro name conflicts with mnemonic: " + info.parsed.mnemonic);
    }

    m_pass1State = Pass1State::CollectingMacro;
    m_currentMacroName = info.parsed.mnemonic;
    m_currentMacroLine = current.sourceLineNumber;
    m_currentMacroBody.clear();
    m_currentMacroParams.clear();
    m_currentMacroLocals.clear();

    // Parse parameter names (after "macro" keyword)
    if (operandUpper.size() > 5)
    {
        std::string paramStr = info.parsed.operand.substr (6);

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
                    m_currentMacroParams.push_back (name);
                }
            }
        }
    }

    handled = true;

Error:
    return hr;
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



    // The dotted form carries its token from the parser; the bare mnemonic
    // form does not take the directive path, so it resolves here. Either way
    // the argument comes from wherever that spelling puts it.
    if (info.parsed.isDirective)
    {
        token   = info.parsed.directiveToken;
        condArg = info.parsed.directiveArg;
    }
    else if (!info.parsed.mnemonic.empty())
    {
        token   = DirectiveTable::FromSpelling (info.parsed.mnemonic);
        condArg = info.parsed.operand;
    }

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
    state.seenElse = false;

    if (state.parentAssembling)
    {
        m_pass1Ctx.currentPC = (int32_t) m_pc;
        ExprResult er = ExpressionEvaluator::Evaluate (condArg, m_pass1Ctx);

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
    state.seenElse = false;

    if (state.parentAssembling)
    {
        std::string symName = condArg;
        size_t s = symName.find_first_not_of (" \t");
        size_t e = symName.find_last_not_of (" \t");

        if (s != std::string::npos)
        {
            symName = symName.substr (s, e - s + 1);
        }

        bool defined = (m_exprSymbols.find (symName) != m_exprSymbols.end());
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleOrgDirective (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    m_pass1Ctx.currentPC = (int32_t) m_pc;
    ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass1Ctx);

    if (!er.success)
    {
        RecordError (current.sourceLineNumber, ".org expression must be resolvable: " + er.error);
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

        if (!m_originSet)
        {
            m_result.startAddress = newAddr;
            m_originSet = true;
        }
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleSegmentSwitch
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleSegmentSwitch (LineInfo & info, bool & handled)
{
    HRESULT    hr    = S_OK;
    Directive  token = info.parsed.directiveToken;



    handled = false;

    BAIL_OUT_IF (!IsSegmentDirective (token), S_OK);

    // Save current PC to current segment
    m_segmentPC[(int) m_currentSegment] = m_pc;

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
    m_pc    = m_segmentPC[(int) m_currentSegment];
    info.pc = m_pc;

    info.isDirective = true;
    handled = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RecordLabel
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RecordLabel (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    // Most lines carry no label; that is not a failure, just nothing to record.
    BAIL_OUT_IF (info.parsed.label.empty(), S_OK);

    {
        std::string labelError;

        if (!Parser::ValidateLabel (info.parsed.label, m_opcodeTable, labelError))
        {
            RecordError (current.sourceLineNumber, labelError);
        }
        else if (m_symbols.count (info.parsed.label) > 0)
        {
            RecordError (current.sourceLineNumber, "Duplicate label: " + info.parsed.label);
        }
        else
        {
            m_symbols[info.parsed.label]     = m_pc;
            m_symbolKinds[info.parsed.label] = SymbolKind::Label;
            m_exprSymbols[info.parsed.label] = (int32_t) m_pc;

            // Warn if label resembles mnemonic by case
            std::string  upper = ToUpperCase (info.parsed.label);

            if (upper != info.parsed.label && m_opcodeTable.IsMnemonic (upper))
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleSetConstant (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    {
        ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, m_pass1Ctx);

        if (!er.success)
        {
            RecordError (current.sourceLineNumber, "Cannot evaluate constant expression: " + er.error);
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
            m_symbolKinds[info.parsed.constantName] = SymbolKind::Equ;

            const std::string & expr = info.parsed.constantExpr;

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



    m_pc += (Word) (args.size() * 2);

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



    m_pc += (Word) text.size();

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



    m_pc += (Word) (args.size() * 4);

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
            RecordError (current.sourceLineNumber, ".ds size must be resolvable: " + er.error);
        }
        else
        {
            m_pc += (Word) er.value;
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
            m_pc += (Word) (alignment - overshoot);
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Directives (const PendingLine & current, LineInfo & info, bool & handled)
{
    HRESULT           hr      = S_OK;
    Directive         token   = info.parsed.directiveToken;
    Pass1DirectiveFn  handler = nullptr;



    if (token > Directive::None && token < Directive::Count)
    {
        const DirectiveRow &  row = GetDirectiveRows()[(size_t) token];

        ASSERT (row.token == token);   // the table drifted out of enum order
        handler = row.pass1;
    }

    // A directive with no pass-1 row is not ours: an unknown dotted spelling,
    // or one an earlier phase already claimed.
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1DataDirectives (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    m_pass1Ctx.currentPC = (int32_t) m_pc;

    std::vector<int32_t>     values;
    std::vector<AssemblyError> tempErrors;

    TryEvaluateDirectiveArgs (info.parsed.directiveArg, m_pass1Ctx, values, current.sourceLineNumber, tempErrors);

    // If evaluation fails, try counting comma-separated items
    if (values.empty() && !info.parsed.directiveArg.empty())
    {
        auto args = Parser::SplitArgList (info.parsed.directiveArg);
        m_pc += (Word) args.size();
    }
    else
    {
        m_pc += (Word) values.size();
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleIncludeDirective
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
        std::string filename = Parser::ParseQuotedString (info.parsed.directiveArg);

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

        FileReadResult fr = m_options.fileReader->ReadFile (filename, m_options.baseDir);

        CBRFEx (fr.success, S_OK, RecordError (current.sourceLineNumber, fr.error));

        std::string ext = GetLowerExtension (filename);
        std::vector<std::string> synthLines;

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
            m_pass1Ctx.currentPC = (int32_t) m_pc;
            ExprResult er = ExpressionEvaluator::Evaluate (args[1], m_pass1Ctx);

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
        ExprResult rhsVal = ExpressionEvaluator::Evaluate (rhs, m_pass1Ctx);

        if (rhsVal.success)
        {
            if (dashPos != std::string::npos && dashPos < eqPos &&
                lhs.size() >= 7 && lhs[0] == '\'' && lhs[2] == '\'')
            {
                char startChar = lhs[1];
                std::string afterDash = lhs.substr (dashPos + 1);
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ExpandMacro (const PendingLine & current, LineInfo & info, bool & handled)
{
    HRESULT hr = S_OK;



    handled = false;

    auto macroIt = m_macros.find (info.parsed.mnemonic);



    // Not a macro call; the line belongs to a later stage.
    BAIL_OUT_IF (macroIt == m_macros.end(), S_OK);

    // Claimed even though it failed: the line was a macro call, so no later
    // stage should try to reinterpret it.
    CBRFEx (current.macroDepth < kMaxMacroDepth, S_OK,
            RecordError (current.sourceLineNumber,
                "Macro nesting depth exceeded (max " + std::to_string (kMaxMacroDepth) + ")");
            handled = true);

    {
        std::vector<std::string> args;

        if (!info.parsed.operand.empty())
        {
            args = Parser::SplitArgList (info.parsed.operand);
        }

        m_macroUniqueCounter++;
        std::string uniqueSuffix = std::format ("{:04d}", m_macroUniqueCounter);

        std::vector<std::string> expandedLines;
        hr = SubstituteMacroParams (macroIt->second, args, uniqueSuffix, expandedLines);
        CHR (hr);

        // Insert expanded lines at the FRONT of the queue (reverse order)
        for (int bi = (int) expandedLines.size() - 1; bi >= 0; bi--)
        {
            PendingLine pl   = {};
            pl.text          = expandedLines[bi];
            pl.sourceLineNumber = current.sourceLineNumber;
            pl.macroDepth    = current.macroDepth + 1;
            m_pendingLines.push_front (pl);
        }

        handled = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::SubstituteMacroParams
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::SubstituteMacroParams (const MacroDefinition & macroDef,
                                                 const std::vector<std::string> & args,
                                                 const std::string & uniqueSuffix,
                                                 std::vector<std::string> & expandedLines)
{
    HRESULT hr = S_OK;

    const auto & body = macroDef.body;



    for (int bi = 0; bi < (int) body.size(); bi++)
    {
        std::string expanded = body[bi];

        // Check for exitm
        bool isExitm = false;
        hr = CheckForExitm (expanded, isExitm);
        CHR (hr);

        if (isExitm)
        {
            int ifDepth = 0;
            hr = CountExitmIfDepth (expandedLines, ifDepth);
            CHR (hr);

            for (int ed = 0; ed < ifDepth; ed++)
            {
                expandedLines.push_back ("                ENDIF");
            }

            break;
        }

        // `local` declares macro-local labels, which uniqueSuffix has already
        // taken care of, so the declaration itself never reaches the output.
        // Left as a string compare for the same reason as EXITM: it is a
        // macro-body keyword, and putting it in DirectiveTable would tokenize
        // it on every line in the file.
        std::string  firstWord = GetLeadingWord (ToUpperCase (expanded));

        if (firstWord == "LOCAL" || firstWord == ".LOCAL")
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::CheckForExitm (const std::string & line, bool & isExitm)
{
    HRESULT      hr   = S_OK;
    std::string  code = ToUpperCase (StripCommentAndTrim (line));



    // EXITM stays a string compare rather than joining DirectiveTable: the
    // table feeds Parser::ParseLine, so adding it there would tokenize EXITM on
    // every line in the file, not just inside a macro body being expanded.
    isExitm = (code == "EXITM" || code == ".EXITM");

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CountExitmIfDepth
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::CountExitmIfDepth (const std::vector<std::string> & expandedLines, int & ifDepth)
{
    HRESULT hr = S_OK;



    ifDepth = 0;



    // The spellings were written out here -- IF/.IF/IFDEF/.IFDEF/IFNDEF/
    // .IFNDEF and ENDIF/.ENDIF -- which made this the third place in the
    // assembler holding a copy of the vocabulary. DirectiveTable owns all
    // eight, so this only has to know which tokens open a block and which
    // closes one.
    for (const std::string & line : expandedLines)
    {
        Directive  token = DirectiveTable::FromSpelling (
                               GetLeadingWord (ToUpperCase (StripCommentAndTrim (line))));

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






HRESULT AssemblySession::ApplyMacroSubstitutions (std::string & expanded,
                                                   const MacroDefinition & macroDef,
                                                   const std::vector<std::string> & args,
                                                   const std::string & uniqueSuffix)
{
    HRESULT hr = S_OK;



    // Replace \0 with argument count
    {
        std::string argCountStr = std::to_string ((int) args.size());
        size_t pos = 0;

        while ((pos = expanded.find ("\\0", pos)) != std::string::npos)
        {
            expanded.replace (pos, 2, argCountStr);
            pos += argCountStr.size();
        }
    }

    // Replace \1 through \9 with arguments
    for (int ai = 9; ai >= 1; ai--)
    {
        std::string placeholder = "\\" + std::to_string (ai);
        size_t pos = 0;

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
        const std::string & paramName = macroDef.paramNames[pi];
        std::string replacement = (pi < (int) args.size()) ? args[pi] : "";
        size_t pos = 0;

        while ((pos = expanded.find (paramName, pos)) != std::string::npos)
        {
            bool leftOk = (pos == 0) ||
                           (!isalnum ((unsigned char) expanded[pos - 1]) && expanded[pos - 1] != '_');
            size_t endPos = pos + paramName.size();
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
            bool leftOk = (pos == 0) ||
                           (!isalnum ((unsigned char) expanded[pos - 1]) && expanded[pos - 1] != '_');
            size_t endPos = pos + localLabel.size();
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::StripForcedSubstitution (std::string & expanded)
{
    HRESULT hr = S_OK;



    size_t sq = 0;
    bool inDouble = false;



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
                      !m_opcodeTable.IsMnemonic (info.parsed.mnemonic) &&
                      !IsBitOpMnemonic (info.parsed.mnemonic) &&
                      (m_macros.find (info.parsed.mnemonic) == m_macros.end());

    BAIL_OUT_IF (!fLooksLikeLabel, S_OK);

    {
        std::string labelName;
        std::string labelError;

        hr = ExtractColonlessLabelName (current, labelName);
        CHR (hr);

        if (!Parser::ValidateLabel (labelName, m_opcodeTable, labelError))
        {
            RecordError (current.sourceLineNumber, labelError);
        }
        else if (m_symbols.count (labelName) > 0)
        {
            RecordError (current.sourceLineNumber, "Duplicate label: " + labelName);
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ExtractColonlessLabelName (const PendingLine & current, std::string & labelName)
{
    HRESULT hr = S_OK;



    std::string rawTrimmed = current.text;



    {
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

        size_t sc = labelName.find (';');

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
                info.parsed.mnemonic = m + std::to_string (er.value);

                std::string rest;

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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ClassifyAndResolve (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    NormalizeBitOp (current, info);

    info.classified    = Parser::ClassifyOperand (info.parsed.operand);
    info.isInstruction = true;

    m_pass1Ctx.currentPC = (int32_t) m_pc;

    bool    exprResolved = false;
    int32_t exprValue    = 0;

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
            RecordError (current.sourceLineNumber, "Expression error: " + er.error);
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ResolveAddressingAndSize (const PendingLine & current, LineInfo & info,
                                                    int32_t exprValue, bool exprResolved)
{
    HRESULT hr = S_OK;



    {
        GlobalAddressingMode::AddressingMode mode = ResolveAddressingMode (
            info.classified.syntax, info.parsed.mnemonic,
            exprValue, exprResolved);
        info.resolvedMode = mode;

        OpcodeEntry entry = {};

        if (m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
        {
            m_pc += 1 + entry.operandSize;
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

            if (altMode != mode && m_opcodeTable.Lookup (info.parsed.mnemonic, altMode, entry))
            {
                info.resolvedMode = altMode;
                m_pc += 1 + entry.operandSize;
            }
            else if (!info.hasError)
            {
                if (!m_opcodeTable.IsMnemonic (info.parsed.mnemonic))
                {
                    RecordError (current.sourceLineNumber, "Invalid mnemonic: " + info.parsed.mnemonic);
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
                m_pc += EstimateErrorRecoverySize (info.classified.syntax, info.parsed.mnemonic);
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ValidateAssemblyCompletion()
{
    HRESULT hr = S_OK;



    if (m_pass1State == Pass1State::CollectingMacro)
    {
        RecordError (m_currentMacroLine, "Unclosed macro definition: " + m_currentMacroName);
    }

    if (!m_condStack.empty())
    {
        RecordError ((int) m_lines.size(),
            "Unclosed if block (" + std::to_string (m_condStack.size()) + " level(s) open)");
    }

// Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleMultiNop
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleMultiNop (const PendingLine & current, LineInfo & info, bool & handled)
{
    HRESULT hr = S_OK;



    handled = false;



    // Only "nop <count>" is a multi-NOP; a bare NOP is an ordinary opcode.
    //
    // This is the second dual-purpose as65 mnemonic, the other being the RMB
    // branch in Parser::ParseLine. They stay apart rather than sharing a table
    // because the table could not hold what separates them: RMB splits on the
    // operand's *shape* (a comma means the Rockwell instruction), which the
    // parser can see, while NOP splits on the operand's *value*, which needs the
    // expression evaluator and the pass-1 symbol table. Both spellings are
    // dialect facts -- a second dialect replaces this pair.
    BAIL_OUT_IF (info.parsed.mnemonic != "NOP" || info.parsed.operand.empty(), S_OK);

    {
        m_pass1Ctx.currentPC = (int32_t) m_pc;
        ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.operand, m_pass1Ctx);

        if (er.success && er.value > 0)
        {
            info.isDirective           = true;
            info.parsed.isDirective    = true;
            info.parsed.directive      = ".MULTINOP";
            info.parsed.directiveToken = Directive::MultiNop;
            info.parsed.directiveArg   = info.parsed.operand;
            m_pc += (Word) er.value;
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
        Word emitPCStart    = info.pc;
        Word emitPC         = info.pc;
        bool lineHasAddress = false;

        if (info.hasError)
        {
            // Nothing to emit
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

        hr = BuildListingEntry (info, emitPCStart, emitPC, lineHasAddress);
        CHR (hr);
    }

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

            const std::string & expr = info.parsed.constantExpr;

            if (expr.size() >= 2 && expr.front() == '"' && expr.back() == '"')
            {
                int32_t len = (int32_t) (expr.size() - 2);
                m_symbols[info.parsed.constantName]     = (Word) len;
                m_fullSymbols[info.parsed.constantName] = len;
                madeProgress = true;
            }
            else
            {
                m_pass2Ctx.currentPC = (int32_t) info.pc;
                ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, m_pass2Ctx);

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

        if (info.parsed.constantKind == SymbolKind::Equ &&
            m_fullSymbols.find (info.parsed.constantName) == m_fullSymbols.end())
        {
            RecordError (info.parsed.lineNumber,
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitByteDirective (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;



    auto args = Parser::SplitArgList (info.parsed.directiveArg);
    bool ok   = true;



    for (const auto & arg : args)
    {
        if (arg.size() >= 2 && arg.front() == '"' && arg.back() == '"')
        {
            std::string raw       = arg.substr (1, arg.size() - 2);
            std::string processed = ProcessEscapeSequences (raw);

            for (char c : processed)
            {
                EmitByte (m_charMap.table[(unsigned char) c], emitPC);
            }
        }
        else if (arg.size() >= 2 && arg.front() == '"')
        {
            size_t closeQuote = arg.find ('"', 1);

            if (closeQuote != std::string::npos)
            {
                std::string raw       = arg.substr (1, closeQuote - 1);
                std::string processed = ProcessEscapeSequences (raw);

                for (char c : processed)
                {
                    EmitByte (m_charMap.table[(unsigned char) c], emitPC);
                }

                std::string suffix          = arg.substr (closeQuote + 1);
                std::string suffixProcessed = ProcessEscapeSequences (suffix);

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
//  AssemblySession::EmitDdDirective
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
            RecordError (info.parsed.lineNumber,
                "Undefined symbol in: " + info.classified.expression);
            emit = false;

            OpcodeEntry entry = {};

            if (m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitInstructionBytes (const LineInfo & info, int32_t value, Word & emitPC)
{
    HRESULT hr = S_OK;



    GlobalAddressingMode::AddressingMode mode = info.resolvedMode;



    if (mode == GlobalAddressingMode::Relative)
    {
        Word pcAfterInstruction = info.pc + 2;
        int  offset = value - (int) pcAfterInstruction;

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

        hasEncoding = m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry);

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
        OpcodeEntry entry = {};

        if (!m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::BuildListingEntry (const LineInfo & info, Word emitPCStart, Word emitPC,
                                             bool lineHasAddress)
{
    HRESULT hr = S_OK;



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

            if (m_opcodeTable.Lookup (info.parsed.mnemonic, info.resolvedMode, cycleEntry))
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
//  AssemblySession::ExtractImage
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
            int defLine = 0;

            for (const auto & info : m_lineInfos)
            {
                if (info.parsed.label == sym.first)
                {
                    defLine = info.parsed.lineNumber;
                    break;
                }
            }

            RecordWarning (defLine, "Unused label: " + sym.first);
        }
    }

// Error:
    return hr;
}
