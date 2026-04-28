#include "Pch.h"

#include "Assembler.h"
#include "Ehm.h"
#include "ExpressionEvaluator.h"
#include "Parser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DefaultFileReader::ReadFile
//
////////////////////////////////////////////////////////////////////////////////

FileReadResult DefaultFileReader::ReadFile (const std::string & filename, const std::string & baseDir)
{
    FileReadResult result = {};

    std::string fullPath = baseDir.empty () ? filename : baseDir + "/" + filename;
    std::ifstream file (fullPath);

    if (!file.is_open ())
    {
        result.success = false;
        result.error   = "Cannot open file: " + fullPath;
        return result;
    }

    std::ostringstream ss;
    ss << file.rdbuf ();
    result.success  = true;
    result.contents = ss.str ();
    return result;
}
enum class Segment { Code, Data, Bss };



struct LineInfo
{
    ParsedLine        parsed;
    ClassifiedOperand classified;
    Word              pc;
    bool              isInstruction;
    bool              isDirective;
    bool              isConstant;
    bool              hasError;

    GlobalAddressingMode::AddressingMode resolvedMode;
    int32_t                              resolvedValue;
    bool                                 valueResolved;

    int              macroDepth;
    bool             conditionalSkip;
    bool             listingSuppressed;
};



struct PendingLine
{
    std::string text;
    int         sourceLineNumber;
    int         macroDepth;
    int         includeDepth;
    std::string sourceFile;
};






////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession
//
////////////////////////////////////////////////////////////////////////////////

class AssemblySession
{
public:
    AssemblySession (const OpcodeTable & opcodeTable, const AssemblerOptions & options);

    AssemblyResult Run (const std::string & sourceText);

private:
    HRESULT Initialize         (const std::string & sourceText);
    HRESULT RunPass1           ();
    HRESULT RunPass2           ();
    HRESULT DetectUnusedLabels ();

    HRESULT HandleStructCollection     (const PendingLine & current, LineInfo & info);
    HRESULT CollectMacroBody           (const PendingLine & current, LineInfo & info);
    HRESULT DetectMacroDefinition      (const PendingLine & current, LineInfo & info,
                                        const std::string & operandUpper, bool & handled);
    HRESULT HandleConditionalDirective (const PendingLine & current, LineInfo & info, bool & handled);
    HRESULT HandleOrgDirective         (const PendingLine & current, LineInfo & info);
    HRESULT HandleSegmentSwitch        (LineInfo & info, bool & handled);
    HRESULT RecordLabel                (const PendingLine & current, LineInfo & info);
    HRESULT HandleConstantDefinition   (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Directives      (const PendingLine & current, LineInfo & info, bool & handled);
    HRESULT HandlePass1DataDirectives  (const PendingLine & current, LineInfo & info);
    HRESULT HandleIncludeDirective     (const PendingLine & current, LineInfo & info);
    HRESULT StartStructDefinition      (const PendingLine & current, LineInfo & info);
    HRESULT HandleCmapDirective        (LineInfo & info);
    HRESULT ExpandMacro                (const PendingLine & current, LineInfo & info, bool & handled);
    HRESULT SubstituteMacroParams      (const MacroDefinition & macroDef,
                                        const std::vector<std::string> & args,
                                        const std::string & uniqueSuffix,
                                        std::vector<std::string> & expandedLines);
    HRESULT HandleColonlessLabel       (const PendingLine & current, LineInfo & info, bool & handled);
    HRESULT ClassifyAndResolve         (const PendingLine & current, LineInfo & info);
    HRESULT ValidateAssemblyCompletion ();

    HRESULT ResolveEquConstants  ();
    HRESULT ReportUnresolvedEqus ();
    HRESULT EmitDirectiveBytes   (const LineInfo & info, Word & emitPC);
    HRESULT EmitInstruction      (const LineInfo & info, Word & emitPC);
    HRESULT BuildListingEntry    (const LineInfo & info, Word emitPCStart, Word emitPC, bool lineHasAddress);
    HRESULT ExtractImage         ();


    HRESULT ProcessPass1Line           (const PendingLine & current);
    HRESULT HandleMultiNop             (const PendingLine & current, LineInfo & info, bool & handled);
    HRESULT CountExitmIfDepth          (const std::vector<std::string> & expandedLines, int & ifDepth);

    // Phase 5 sub-helpers
    HRESULT CheckEndStruct            (const PendingLine & current, LineInfo & info, bool & isEnd);
    HRESULT ParseStructMember         (const PendingLine & current, LineInfo & info);
    HRESULT HandleIfDirective         (const PendingLine & current, const std::string & condArg);
    HRESULT HandleIfdefDirective      (const PendingLine & current, const std::string & condDirective,
                                       const std::string & condArg);
    HRESULT HandleElseDirective       (const PendingLine & current);
    HRESULT HandleEndifDirective      (const PendingLine & current);
    HRESULT CheckForExitm             (const std::string & line, bool & isExitm);
    HRESULT ApplyMacroSubstitutions   (std::string & expanded, const MacroDefinition & macroDef,
                                       const std::vector<std::string> & args,
                                       const std::string & uniqueSuffix);
    HRESULT StripForcedSubstitution   (std::string & expanded);
    HRESULT EmitByteDirective         (const LineInfo & info, Word & emitPC);
    HRESULT EmitWordDirective         (const LineInfo & info, Word & emitPC);
    HRESULT EmitDdDirective           (const LineInfo & info, Word & emitPC);
    HRESULT EmitDsDirective           (const LineInfo & info, Word & emitPC);
    HRESULT EmitAlignDirective        (const LineInfo & info, Word & emitPC);
    HRESULT ResolveInstructionValue   (const LineInfo & info, int32_t & value, bool & emit);
    HRESULT EmitInstructionBytes      (const LineInfo & info, int32_t value, Word & emitPC);
    HRESULT HandleSetConstant         (const PendingLine & current, LineInfo & info);
    HRESULT HandleEquConstant         (const PendingLine & current, LineInfo & info);
    HRESULT ResolveAddressingAndSize  (const PendingLine & current, LineInfo & info,
                                       int32_t exprValue, bool exprResolved);
    HRESULT ExtractColonlessLabelName (const PendingLine & current, std::string & labelName);
    HRESULT ParseCmapMapping          (const std::string & arg);

    // Helpers moved from file-scope statics
    static std::string              GetLowerExtension       (const std::string & filename);
    static int                      HexCharToNibble         (char c);
    static int                      HexByte                 (const std::string & s, size_t offset);
    static std::vector<Byte>        ParseSRecord            (const std::string & content);
    static std::vector<Byte>        ParseIntelHex           (const std::string & content);
    static std::vector<std::string> GenerateByteDirectives  (const std::vector<Byte> & data);
    static bool                     IsBranchMnemonic        (const std::string & mnemonic);
    static Byte                     EstimateInstructionSize (OperandSyntax syntax, const std::string & mnemonic);
    static std::string              ProcessEscapeSequences  (const std::string & str);
    static bool                     EvaluateDirectiveArgs   (const std::string & argText,
                                                             const ExprContext & ctx,
                                                             std::vector<int32_t> & values,
                                                             int lineNumber,
                                                             std::vector<AssemblyError> & errors);
    GlobalAddressingMode::AddressingMode ResolveAddressingMode (OperandSyntax syntax,
                                                                 const std::string & mnemonic,
                                                                 int32_t value, bool resolved);

    void RecordError   (int lineNumber, const std::string & message);
    void RecordWarning (int lineNumber, const std::string & message);
    bool IsAssembling  () const;
    void InjectBuiltin (const std::string & name, int32_t value);
    void EmitByte      (Byte b, Word & emitPC);

    const OpcodeTable      & m_opcodeTable;
    const AssemblerOptions & m_options;
    AssemblyResult           m_result             = {};

    std::vector<std::string>                           m_lines;
    std::vector<LineInfo>                               m_lineInfos;
    std::unordered_map<std::string, Word>               m_symbols;
    std::unordered_map<std::string, SymbolKind>         m_symbolKinds;
    std::unordered_map<std::string, int32_t>            m_exprSymbols;
    ExprContext                                          m_pass1Ctx           = { &m_exprSymbols, 0 };
    Word                                                m_pc                 = 0;
    bool                                                m_originSet          = false;
    bool                                                m_endAssembly        = false;
    Segment                                             m_currentSegment     = Segment::Code;
    Word                                                m_segmentPC[3]       = { 0, 0, 0 };
    std::vector<ConditionalState>                       m_condStack;
    std::unordered_map<std::string, MacroDefinition>    m_macros;
    bool                                                m_collectingMacro    = false;
    std::string                                         m_currentMacroName;
    int                                                 m_currentMacroLine   = 0;
    std::vector<std::string>                            m_currentMacroBody;
    std::vector<std::string>                            m_currentMacroParams;
    std::vector<std::string>                            m_currentMacroLocals;
    int                                                 m_macroUniqueCounter = 0;
    int                                                 m_listingLevel;
    std::unordered_map<std::string, StructDefinition>   m_structs;
    bool                                                m_collectingStruct   = false;
    StructDefinition                                    m_currentStruct      = {};
    CharacterMap                                        m_charMap;
    std::deque<PendingLine>                             m_pendingLines;
    std::vector<Byte>                                   m_image;
    Word                                                m_lowestAddr         = 0xFFFF;
    Word                                                m_highestAddr        = 0x0000;
    std::unordered_map<std::string, int>                m_referencedLabels;
    std::unordered_map<std::string, int32_t>            m_fullSymbols;
    ExprContext                                          m_pass2Ctx           = { &m_fullSymbols, 0 };

    static const int kMaxMacroDepth   = 15;
    static const int kMaxIncludeDepth = 16;
};





////////////////////////////////////////////////////////////////////////////////
//
//  GetLowerExtension
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::GetLowerExtension (const std::string & filename)
{
    size_t dot = filename.rfind ('.');

    if (dot == std::string::npos)
    {
        return "";
    }

    std::string ext = filename.substr (dot);

    for (auto & c : ext)
    {
        c = (char) std::tolower ((unsigned char) c);
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
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HexByte
//
////////////////////////////////////////////////////////////////////////////////

int AssemblySession::HexByte (const std::string & s, size_t offset)
{
    if (offset + 1 >= s.size ())
    {
        return -1;
    }

    int hi = HexCharToNibble (s[offset]);
    int lo = HexCharToNibble (s[offset + 1]);

    if (hi < 0 || lo < 0)
    {
        return -1;
    }

    return (hi << 4) | lo;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseSRecord
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
        if (!line.empty () && line.back () == '\r')
        {
            line.pop_back ();
        }

        if (line.size () < 2 || line[0] != 'S')
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

        if (line.size () < 4)
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
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> AssemblySession::ParseIntelHex (const std::string & content)
{
    std::vector<Byte> data;
    std::istringstream stream (content);
    std::string line;



    while (std::getline (stream, line))
    {
        // Trim trailing CR
        if (!line.empty () && line.back () == '\r')
        {
            line.pop_back ();
        }

        if (line.empty () || line[0] != ':')
        {
            continue;
        }

        if (line.size () < 11)
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



    for (size_t i = 0; i < data.size (); i += kBytesPerLine)
    {
        std::string line = "    .byte ";

        size_t end = std::min (i + kBytesPerLine, data.size ());

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
//  Assembler
//
////////////////////////////////////////////////////////////////////////////////

Assembler::Assembler (const Microcode instructionSet[256], AssemblerOptions options) :
    m_opcodeTable (instructionSet),
    m_options     (options)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordWarning
//
////////////////////////////////////////////////////////////////////////////////

void Assembler::RecordWarning (AssemblyResult & result, int lineNumber, const std::string & message)
{
    switch (m_options.warningMode)
    {
        case WarningMode::Warn:
        {
            AssemblyError warning = {};
            warning.lineNumber = lineNumber;
            warning.message    = message;
            result.warnings.push_back (warning);
            break;
        }

        case WarningMode::FatalWarnings:
        {
            AssemblyError error = {};
            error.lineNumber = lineNumber;
            error.message    = message;
            result.errors.push_back (error);
            result.success = false;
            break;
        }

        case WarningMode::NoWarn:
            // Discard silently
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBranchMnemonic
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::IsBranchMnemonic (const std::string & mnemonic)
{
    return mnemonic == "BPL" || mnemonic == "BMI" ||
           mnemonic == "BVC" || mnemonic == "BVS" ||
           mnemonic == "BCC" || mnemonic == "BCS" ||
           mnemonic == "BNE" || mnemonic == "BEQ";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveAddressingMode — derive final 6502 addressing mode from syntax,
//  mnemonic, and resolved value
//
////////////////////////////////////////////////////////////////////////////////

GlobalAddressingMode::AddressingMode AssemblySession::ResolveAddressingMode (
    OperandSyntax    syntax,
    const std::string & mnemonic,
    int32_t          value,
    bool             resolved)
{
    using AM = GlobalAddressingMode::AddressingMode;



    switch (syntax)
    {
        case OperandSyntax::None:
            return AM::SingleByteNoOperand;

        case OperandSyntax::Accumulator:
            return AM::Accumulator;

        case OperandSyntax::Immediate:
            return AM::Immediate;

        case OperandSyntax::IndirectX:
            return AM::ZeroPageXIndirect;

        case OperandSyntax::IndirectY:
            return AM::ZeroPageIndirectY;

        case OperandSyntax::Indirect:
            return AM::JumpIndirect;

        case OperandSyntax::IndexedX:
        {
            if (resolved && value >= 0 && value <= 0xFF)
            {
                OpcodeEntry entry = {};

                if (m_opcodeTable.Lookup (mnemonic, AM::ZeroPageX, entry))
                {
                    return AM::ZeroPageX;
                }
            }

            return AM::AbsoluteX;
        }

        case OperandSyntax::IndexedY:
        {
            if (resolved && value >= 0 && value <= 0xFF)
            {
                OpcodeEntry entry = {};

                if (m_opcodeTable.Lookup (mnemonic, AM::ZeroPageY, entry))
                {
                    return AM::ZeroPageY;
                }
            }

            return AM::AbsoluteY;
        }

        case OperandSyntax::Bare:
        {
            if (IsBranchMnemonic (mnemonic))
            {
                return AM::Relative;
            }

            if (mnemonic == "JMP" || mnemonic == "JSR")
            {
                return AM::JumpAbsolute;
            }

            if (resolved && value >= 0 && value <= 0xFF)
            {
                OpcodeEntry entry = {};

                if (m_opcodeTable.Lookup (mnemonic, AM::ZeroPage, entry))
                {
                    return AM::ZeroPage;
                }
            }

            return AM::Absolute;
        }
    }

    return AM::SingleByteNoOperand;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EstimateInstructionSize — conservative size for unresolved expressions
//
////////////////////////////////////////////////////////////////////////////////

Byte AssemblySession::EstimateInstructionSize (OperandSyntax syntax, const std::string & mnemonic)
{
    switch (syntax)
    {
        case OperandSyntax::None:
        case OperandSyntax::Accumulator:
            return 1;

        case OperandSyntax::Immediate:
        case OperandSyntax::IndirectX:
        case OperandSyntax::IndirectY:
            return 2;

        case OperandSyntax::Indirect:
            return 3;

        case OperandSyntax::IndexedX:
        case OperandSyntax::IndexedY:
        case OperandSyntax::Bare:
        {
            if (IsBranchMnemonic (mnemonic))
            {
                return 2;
            }

            return 3;
        }
    }

    return 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluateDirectiveArgs — evaluate comma-separated expression list
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblySession::ProcessEscapeSequences (const std::string & str)
{
    std::string result;
    result.reserve (str.size ());



    for (size_t i = 0; i < str.size (); i++)
    {
        if (str[i] == '\\' && i + 1 < str.size ())
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
//  EvaluateDirectiveArgs — evaluate comma-separated expression list
//
////////////////////////////////////////////////////////////////////////////////

bool AssemblySession::EvaluateDirectiveArgs (
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
        if (arg.size () >= 2 && arg.front () == '"' && arg.back () == '"')
        {
            std::string raw       = arg.substr (1, arg.size () - 2);
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

bool AssemblySession::IsAssembling () const
{
    return m_condStack.empty () || m_condStack.back ().assembling;
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

    for (int i = 0; i < (int) m_lines.size (); i++)
    {
        PendingLine pl = {};
        pl.text             = m_lines[i];
        pl.sourceLineNumber = i + 1;
        pl.macroDepth       = 0;
        m_pendingLines.push_back (pl);
    }

Error:
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



    CHR (Initialize (sourceText));
    CHR (RunPass1 ());
    CHR (RunPass2 ());
    CHR (DetectUnusedLabels ());

Error:
    return m_result;
}






////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::RunPass1
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::RunPass1 ()
{
    HRESULT hr = S_OK;



    while (!m_pendingLines.empty ())
    {
        PendingLine current = m_pendingLines.front ();
        m_pendingLines.pop_front ();

        if (!m_endAssembly)
        {
            CHR (ProcessPass1Line (current));
        }
    }

    CHR (ValidateAssemblyCompletion ());

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
    HRESULT hr = S_OK;

    LineInfo info          = {};



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

    // Struct definition collection
    if (m_collectingStruct)
    {
        CHR (HandleStructCollection (current, info));
        m_lineInfos.push_back (info);
        goto Error;
    }

    // Macro definition collection
    if (m_collectingMacro)
    {
        CHR (CollectMacroBody (current, info));
        m_lineInfos.push_back (info);
        goto Error;
    }

    // Check for macro definition start
    {
        std::string operandUpper;

        if (!info.parsed.operand.empty ())
        {
            operandUpper = info.parsed.operand;

            for (auto & c : operandUpper)
            {
                c = (char) toupper ((unsigned char) c);
            }
        }

        bool macroDefHandled = false;
        CHR (DetectMacroDefinition (current, info, operandUpper, macroDefHandled));

        if (macroDefHandled)
        {
            m_lineInfos.push_back (info);
            goto Error;
        }
    }

    // Conditional assembly directives
    {
        bool condHandled = false;
        CHR (HandleConditionalDirective (current, info, condHandled));

        if (condHandled)
        {
            m_lineInfos.push_back (info);
            goto Error;
        }
    }

    // Skip lines in non-assembling conditional blocks
    if (!IsAssembling ())
    {
        info.conditionalSkip = true;
        m_lineInfos.push_back (info);
        goto Error;
    }

    // Handle .ORG before recording label
    if (info.parsed.isDirective && info.parsed.directive == ".ORG")
    {
        CHR (HandleOrgDirective (current, info));
        info.isDirective = true;
        m_lineInfos.push_back (info);
        goto Error;
    }

    // Handle segment switches before recording label
    if (info.parsed.isDirective)
    {
        bool segHandled = false;
        CHR (HandleSegmentSwitch (info, segHandled));

        if (segHandled)
        {
            m_lineInfos.push_back (info);
            goto Error;
        }
    }

    // Record label
    CHR (RecordLabel (current, info));

    // Handle constant definitions
    if (info.parsed.isConstant)
    {
        CHR (HandleConstantDefinition (current, info));
        m_lineInfos.push_back (info);
        goto Error;
    }

    // Handle directives
    if (info.parsed.isDirective)
    {
        bool dirHandled = false;
        CHR (HandlePass1Directives (current, info, dirHandled));

        if (dirHandled)
        {
            m_lineInfos.push_back (info);
            goto Error;
        }
    }

    // Skip empty lines
    if (info.parsed.mnemonic.empty ())
    {
        m_lineInfos.push_back (info);
        goto Error;
    }

    // Multi-NOP
    {
        bool nopHandled = false;
        CHR (HandleMultiNop (current, info, nopHandled));

        if (nopHandled)
        {
            m_lineInfos.push_back (info);
            goto Error;
        }
    }

    // Macro invocation
    {
        bool macroHandled = false;
        CHR (ExpandMacro (current, info, macroHandled));

        if (macroHandled)
        {
            m_lineInfos.push_back (info);
            goto Error;
        }
    }

    // Colon-less label detection
    {
        bool colonlessHandled = false;
        CHR (HandleColonlessLabel (current, info, colonlessHandled));

        if (colonlessHandled)
        {
            m_lineInfos.push_back (info);
            goto Error;
        }
    }

    // Classify operand and resolve addressing mode
    CHR (ClassifyAndResolve (current, info));
    m_lineInfos.push_back (info);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleStructCollection
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleStructCollection (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;

    bool isEndStruct = false;



    CHR (CheckEndStruct (current, info, isEndStruct));

    if (isEndStruct)
    {
        int32_t structSize = m_currentStruct.currentOffset - m_currentStruct.startOffset;
        m_symbols[m_currentStruct.name]     = (Word) structSize;
        m_symbolKinds[m_currentStruct.name] = SymbolKind::Equ;
        m_exprSymbols[m_currentStruct.name] = structSize;
        m_structs[m_currentStruct.name]     = m_currentStruct;
        m_collectingStruct = false;
    }
    else if (!info.parsed.isEmpty)
    {
        CHR (ParseStructMember (current, info));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CheckEndStruct
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::CheckEndStruct (const PendingLine & current, LineInfo & info, bool & isEnd)
{
    HRESULT hr = S_OK;

    std::string mnUpper = info.parsed.mnemonic;

    isEnd = false;



    if (info.parsed.isDirective && info.parsed.directive == ".END")
    {
        std::string endArgUpper = info.parsed.directiveArg;

        for (auto & c : endArgUpper)
        {
            c = (char) toupper ((unsigned char) c);
        }

        size_t sp = endArgUpper.find_first_not_of (" \t");

        if (sp != std::string::npos)
        {
            endArgUpper = endArgUpper.substr (sp);
        }

        size_t ep = endArgUpper.find_first_of (" \t");
        std::string firstWord = (ep == std::string::npos) ? endArgUpper : endArgUpper.substr (0, ep);

        if (firstWord == "STRUCT")
        {
            isEnd = true;
        }
    }
    else if (mnUpper == "END" && !info.parsed.operand.empty ())
    {
        std::string opUpper = info.parsed.operand;

        for (auto & c : opUpper)
        {
            c = (char) toupper ((unsigned char) c);
        }

        size_t sp = opUpper.find_first_of (" \t");
        std::string first = (sp == std::string::npos) ? opUpper : opUpper.substr (0, sp);

        if (first == "STRUCT")
        {
            isEnd = true;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ParseStructMember
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ParseStructMember (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;

    std::string mnUpper    = info.parsed.mnemonic;
    std::string memberName;
    int32_t     memberSize = 0;



    if (!mnUpper.empty () && !info.parsed.operand.empty ())
    {
        std::string opStr   = info.parsed.operand;
        std::string opUpper = opStr;

        for (auto & c : opUpper)
        {
            c = (char) toupper ((unsigned char) c);
        }

        size_t sp = opUpper.find_first_of (" \t");
        std::string directive = (sp == std::string::npos) ? opUpper : opUpper.substr (0, sp);
        std::string sizeExpr;

        if (sp != std::string::npos)
        {
            sizeExpr = opStr.substr (sp);
            size_t ss = sizeExpr.find_first_not_of (" \t");

            if (ss != std::string::npos)
            {
                sizeExpr = sizeExpr.substr (ss);
            }
        }

        // Recover original-case member name from raw text
        std::string rawTrimmed = current.text;
        size_t rs = rawTrimmed.find_first_not_of (" \t");

        if (rs != std::string::npos)
        {
            rawTrimmed = rawTrimmed.substr (rs);
        }

        size_t re = rawTrimmed.find_first_of (" \t");

        if (re != std::string::npos)
        {
            memberName = rawTrimmed.substr (0, re);
        }
        else
        {
            memberName = rawTrimmed;
        }

        if (directive == "DS" || directive == "DSB" || directive == "RMB")
        {
            m_pass1Ctx.currentPC = (int32_t) m_pc;
            ExprResult er = ExpressionEvaluator::Evaluate (sizeExpr, m_pass1Ctx);
            memberSize = er.success ? er.value : 1;
        }
        else if (directive == "DB" || directive == "BYT" || directive == "BYTE" || directive == "FCB")
        {
            memberSize = 1;
        }
        else if (directive == "DW" || directive == "WORD" || directive == "FCW" || directive == "FDB")
        {
            memberSize = 2;
        }
        else if (directive == "DD")
        {
            memberSize = 4;
        }
    }

    if (!memberName.empty () && memberSize > 0)
    {
        StructMember member = {};
        member.name   = memberName;
        member.offset = m_currentStruct.currentOffset;
        member.size   = memberSize;
        m_currentStruct.members.push_back (member);

        std::string symName = m_currentStruct.name + "." + memberName;
        m_symbols[symName]     = (Word) m_currentStruct.currentOffset;
        m_symbolKinds[symName] = SymbolKind::Equ;
        m_exprSymbols[symName] = m_currentStruct.currentOffset;

        m_currentStruct.currentOffset += memberSize;
    }

Error:
    return hr;
}






////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::CollectMacroBody
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
        m_collectingMacro = false;
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

                if (!name.empty ())
                {
                    m_currentMacroLocals.push_back (name);
                }
            }
        }

        m_currentMacroBody.push_back (current.text);
    }

Error:
    return hr;
}






////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::DetectMacroDefinition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::DetectMacroDefinition (const PendingLine & current, LineInfo & info,
                                                 const std::string & operandUpper, bool & handled)
{
    HRESULT hr = S_OK;

    handled = false;



    if (info.parsed.mnemonic.empty () || info.parsed.isEmpty || !IsAssembling ())
    {
        goto Error;
    }

    // "NAME macro [params]" -- operand starts with "macro"
    if (operandUpper.substr (0, 5) != "MACRO" ||
        (operandUpper.size () > 5 && operandUpper[5] != ' ' && operandUpper[5] != '\t'))
    {
        goto Error;
    }

    // Name collision check
    if (m_opcodeTable.IsMnemonic (info.parsed.mnemonic))
    {
        RecordError (current.sourceLineNumber, "Macro name conflicts with mnemonic: " + info.parsed.mnemonic);
    }

    m_collectingMacro  = true;
    m_currentMacroName = info.parsed.mnemonic;
    m_currentMacroLine = current.sourceLineNumber;
    m_currentMacroBody.clear ();
    m_currentMacroParams.clear ();
    m_currentMacroLocals.clear ();

    // Parse parameter names (after "macro" keyword)
    if (operandUpper.size () > 5)
    {
        std::string paramStr = info.parsed.operand.substr (6);

        if (!paramStr.empty ())
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

                if (!name.empty ())
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleConditionalDirective (const PendingLine & current, LineInfo & info,
                                                      bool & handled)
{
    HRESULT hr = S_OK;

    std::string condDirective;
    std::string condArg;

    handled = false;



    if (info.parsed.isDirective)
    {
        const std::string & dir = info.parsed.directive;

        if (dir == ".IF")          { condDirective = "IF";     condArg = info.parsed.directiveArg; }
        else if (dir == ".IFDEF")  { condDirective = "IFDEF";  condArg = info.parsed.directiveArg; }
        else if (dir == ".IFNDEF") { condDirective = "IFNDEF"; condArg = info.parsed.directiveArg; }
        else if (dir == ".ELSE")   { condDirective = "ELSE"; }
        else if (dir == ".ENDIF")  { condDirective = "ENDIF"; }
    }
    else if (!info.parsed.mnemonic.empty ())
    {
        if (info.parsed.mnemonic == "IF")          { condDirective = "IF";     condArg = info.parsed.operand; }
        else if (info.parsed.mnemonic == "IFDEF")  { condDirective = "IFDEF";  condArg = info.parsed.operand; }
        else if (info.parsed.mnemonic == "IFNDEF") { condDirective = "IFNDEF"; condArg = info.parsed.operand; }
        else if (info.parsed.mnemonic == "ELSE")   { condDirective = "ELSE"; }
        else if (info.parsed.mnemonic == "ENDIF")  { condDirective = "ENDIF"; }
    }

    if (condDirective.empty ())
    {
        goto Error;
    }

    if (condDirective == "IF")
    {
        CHR (HandleIfDirective (current, condArg));
    }
    else if (condDirective == "IFDEF" || condDirective == "IFNDEF")
    {
        CHR (HandleIfdefDirective (current, condDirective, condArg));
    }
    else if (condDirective == "ELSE")
    {
        CHR (HandleElseDirective (current));
    }
    else if (condDirective == "ENDIF")
    {
        CHR (HandleEndifDirective (current));
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
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleIfDirective (const PendingLine & current, const std::string & condArg)
{
    HRESULT hr = S_OK;

    ConditionalState state = {};



    state.parentAssembling = IsAssembling ();
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

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleIfdefDirective
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleIfdefDirective (const PendingLine & current,
                                                const std::string & condDirective,
                                                const std::string & condArg)
{
    HRESULT hr = S_OK;

    ConditionalState state = {};



    state.parentAssembling = IsAssembling ();
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

        bool defined = (m_exprSymbols.find (symName) != m_exprSymbols.end ());
        state.assembling = (condDirective == "IFDEF") ? defined : !defined;
    }
    else
    {
        state.assembling = false;
    }

    m_condStack.push_back (state);

Error:
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



    if (m_condStack.empty ())
    {
        RecordError (current.sourceLineNumber, "else without matching if");
    }
    else if (m_condStack.back ().seenElse)
    {
        RecordError (current.sourceLineNumber, "Duplicate else");
    }
    else
    {
        m_condStack.back ().seenElse = true;

        if (m_condStack.back ().parentAssembling)
        {
            m_condStack.back ().assembling = !m_condStack.back ().assembling;
        }
    }

Error:
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



    if (m_condStack.empty ())
    {
        RecordError (current.sourceLineNumber, "endif without matching if");
    }
    else
    {
        m_condStack.pop_back ();
    }

Error:
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

Error:
    return hr;
}






////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleSegmentSwitch
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleSegmentSwitch (LineInfo & info, bool & handled)
{
    HRESULT hr = S_OK;

    handled = false;

    const std::string & dir = info.parsed.directive;



    if (dir != ".SEGMENT_CODE" && dir != ".SEGMENT_DATA" && dir != ".SEGMENT_BSS" &&
        dir != ".CODE"         && dir != ".DATA"         && dir != ".BSS")
    {
        goto Error;
    }

    // Save current PC to current segment
    m_segmentPC[(int) m_currentSegment] = m_pc;

    // Switch segment
    if (dir == ".SEGMENT_CODE" || dir == ".CODE")
    {
        m_currentSegment = Segment::Code;
    }
    else if (dir == ".SEGMENT_DATA" || dir == ".DATA")
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



    if (info.parsed.label.empty ())
    {
        goto Error;
    }

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
            std::string upper = info.parsed.label;

            for (auto & c : upper)
            {
                c = (char) toupper ((unsigned char) c);
            }

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
        CHR (HandleSetConstant (current, info));
    }
    else
    {
        CHR (HandleEquConstant (current, info));
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

            if (kindIt != m_symbolKinds.end () && kindIt->second != SymbolKind::Set)
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

Error:
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

        if (kindIt != m_symbolKinds.end ())
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

            if (expr.size () >= 2 && expr.front () == '"' && expr.back () == '"')
            {
                int32_t len = (int32_t) (expr.size () - 2);
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

Error:
    return hr;
}






////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandlePass1Directives
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandlePass1Directives (const PendingLine & current, LineInfo & info, bool & handled)
{
    HRESULT hr = S_OK;

    handled = true;
    info.isDirective = true;

    const std::string & dir = info.parsed.directive;



    if (dir == ".BYTE")
    {
        CHR (HandlePass1DataDirectives (current, info));
    }
    else if (dir == ".WORD")
    {
        auto args = Parser::SplitArgList (info.parsed.directiveArg);
        m_pc += (Word) (args.size () * 2);
    }
    else if (dir == ".TEXT")
    {
        std::string text = Parser::ParseQuotedString (info.parsed.directiveArg);
        m_pc += (Word) text.size ();
    }
    else if (dir == ".DD")
    {
        auto args = Parser::SplitArgList (info.parsed.directiveArg);
        m_pc += (Word) (args.size () * 4);
    }
    else if (dir == ".DS")
    {
        m_pass1Ctx.currentPC = (int32_t) m_pc;
        auto args = Parser::SplitArgList (info.parsed.directiveArg);

        if (!args.empty ())
        {
            ExprResult er = ExpressionEvaluator::Evaluate (args[0], m_pass1Ctx);

            if (!er.success)
            {
                RecordError (current.sourceLineNumber, ".ds size must be resolvable: " + er.error);
            }
            else
            {
                m_pc += (Word) er.value;
            }
        }
    }
    else if (dir == ".ALIGN")
    {
        m_pass1Ctx.currentPC = (int32_t) m_pc;
        int alignment = 2;

        if (!info.parsed.directiveArg.empty ())
        {
            ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass1Ctx);

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
            int remainder2 = m_pc % alignment;

            if (remainder2 != 0)
            {
                m_pc += (Word) (alignment - remainder2);
            }
        }
    }
    else if (dir == ".END")
    {
        m_endAssembly = true;
    }
    else if (dir == ".ERROR")
    {
        std::string msg = Parser::ParseQuotedString (info.parsed.directiveArg);

        if (msg.empty () && !info.parsed.directiveArg.empty ())
        {
            msg = info.parsed.directiveArg;
        }

        RecordError (current.sourceLineNumber, msg.empty () ? "User error directive" : msg);
    }
    else if (dir == ".OPT_NOOP")
    {
        // Recognized but intentionally no-op
    }
    else if (dir == ".LIST")
    {
        m_listingLevel++;
    }
    else if (dir == ".NOLIST")
    {
        m_listingLevel--;
    }
    else if (dir == ".PAGE")
    {
        // Page break -- handled at listing output time
    }
    else if (dir == ".TITLE")
    {
        m_result.listingTitle = Parser::ParseQuotedString (info.parsed.directiveArg);

        if (m_result.listingTitle.empty () && !info.parsed.directiveArg.empty ())
        {
            m_result.listingTitle = info.parsed.directiveArg;
        }
    }
    else if (dir == ".INCLUDE")
    {
        CHR (HandleIncludeDirective (current, info));
    }
    else if (dir == ".STRUCT")
    {
        CHR (StartStructDefinition (current, info));
    }
    else if (dir == ".CMAP")
    {
        CHR (HandleCmapDirective (info));
    }
    else
    {
        // Not a recognized directive that we handle here
        info.isDirective = false;
        handled = false;
    }

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

    EvaluateDirectiveArgs (info.parsed.directiveArg, m_pass1Ctx, values, current.sourceLineNumber, tempErrors);

    // If evaluation fails, try counting comma-separated items
    if (values.empty () && !info.parsed.directiveArg.empty ())
    {
        auto args = Parser::SplitArgList (info.parsed.directiveArg);
        m_pc += (Word) args.size ();
    }
    else
    {
        m_pc += (Word) values.size ();
    }

Error:
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



    if (m_options.fileReader == nullptr)
    {
        RecordError (current.sourceLineNumber, "No file reader configured for include");
        goto Error;
    }

    if (current.includeDepth >= kMaxIncludeDepth)
    {
        RecordError (current.sourceLineNumber,
            "Include nesting depth exceeded (max " + std::to_string (kMaxIncludeDepth) + ")");
        goto Error;
    }

    {
        std::string filename = Parser::ParseQuotedString (info.parsed.directiveArg);

        if (filename.empty ())
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

        if (!fr.success)
        {
            RecordError (current.sourceLineNumber, fr.error);
            goto Error;
        }

        std::string ext = GetLowerExtension (filename);
        std::vector<std::string> synthLines;

        if (ext == ".bin")
        {
            std::vector<Byte> raw (fr.contents.begin (), fr.contents.end ());
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

        if (!synthLines.empty ())
        {
            for (int il = (int) synthLines.size () - 1; il >= 0; il--)
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

            for (int il = (int) includeLines.size () - 1; il >= 0; il--)
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
    HRESULT hr = S_OK;



    {
        auto args = Parser::SplitArgList (info.parsed.directiveArg);

        if (args.empty ())
        {
            RecordError (current.sourceLineNumber, "struct requires a name");
            goto Error;
        }

        m_currentStruct             = {};
        m_currentStruct.name        = args[0];
        m_currentStruct.startOffset = 0;

        if (args.size () >= 2)
        {
            m_pass1Ctx.currentPC = (int32_t) m_pc;
            ExprResult er = ExpressionEvaluator::Evaluate (args[1], m_pass1Ctx);

            if (er.success)
            {
                m_currentStruct.startOffset = er.value;
            }
        }

        m_currentStruct.currentOffset = m_currentStruct.startOffset;
        m_collectingStruct = true;
    }

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleCmapDirective
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleCmapDirective (LineInfo & info)
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
    else if (arg.size () >= 5 && arg[0] == '\'')
    {
        CHR (ParseCmapMapping (arg));
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



    if (eqPos == std::string::npos)
    {
        goto Error;
    }

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
                lhs.size () >= 7 && lhs[0] == '\'' && lhs[2] == '\'')
            {
                char startChar = lhs[1];
                std::string afterDash = lhs.substr (dashPos + 1);
                size_t ads = afterDash.find_first_not_of (" \t");

                if (ads != std::string::npos)
                {
                    afterDash = afterDash.substr (ads);
                }

                if (afterDash.size () >= 3 && afterDash[0] == '\'' && afterDash[2] == '\'')
                {
                    char endChar = afterDash[1];

                    for (int ci = (unsigned char) startChar; ci <= (unsigned char) endChar; ci++)
                    {
                        m_charMap.table[ci] = (Byte) (rhsVal.value + (ci - (unsigned char) startChar));
                    }
                }
            }
            else if (lhs.size () >= 3 && lhs[0] == '\'' && lhs[2] == '\'')
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



    if (macroIt == m_macros.end ())
    {
        goto Error;
    }

    if (current.macroDepth >= kMaxMacroDepth)
    {
        RecordError (current.sourceLineNumber,
            "Macro nesting depth exceeded (max " + std::to_string (kMaxMacroDepth) + ")");
        handled = true;
        goto Error;
    }

    {
        std::vector<std::string> args;

        if (!info.parsed.operand.empty ())
        {
            args = Parser::SplitArgList (info.parsed.operand);
        }

        m_macroUniqueCounter++;
        std::string uniqueSuffix = std::format ("{:04d}", m_macroUniqueCounter);

        std::vector<std::string> expandedLines;
        CHR (SubstituteMacroParams (macroIt->second, args, uniqueSuffix, expandedLines));

        // Insert expanded lines at the FRONT of the queue (reverse order)
        for (int bi = (int) expandedLines.size () - 1; bi >= 0; bi--)
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



    for (int bi = 0; bi < (int) body.size (); bi++)
    {
        std::string expanded = body[bi];

        // Check for exitm
        bool isExitm = false;
        CHR (CheckForExitm (expanded, isExitm));

        if (isExitm)
        {
            int ifDepth = 0;
            CHR (CountExitmIfDepth (expandedLines, ifDepth));

            for (int ed = 0; ed < ifDepth; ed++)
            {
                expandedLines.push_back ("                ENDIF");
            }

            break;
        }

        // Skip local directive lines
        std::string exUpper = expanded;
        size_t exStart = exUpper.find_first_not_of (" \t");

        if (exStart != std::string::npos)
        {
            exUpper = exUpper.substr (exStart);
        }

        for (auto & ec : exUpper)
        {
            ec = (char) toupper ((unsigned char) ec);
        }

        size_t lsp = exUpper.find_first_of (" \t");
        std::string localFirst = (lsp == std::string::npos) ? exUpper : exUpper.substr (0, lsp);

        if (localFirst == "LOCAL" || localFirst == ".LOCAL")
        {
            continue;
        }

        CHR (ApplyMacroSubstitutions (expanded, macroDef, args, uniqueSuffix));
        CHR (StripForcedSubstitution (expanded));

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
    HRESULT hr = S_OK;

    isExitm = false;

    std::string trimmed = line;



    {
        size_t s = trimmed.find_first_not_of (" \t");

        if (s != std::string::npos)
        {
            trimmed = trimmed.substr (s);
        }

        size_t sc = trimmed.find (';');

        if (sc != std::string::npos)
        {
            trimmed = trimmed.substr (0, sc);
        }

        size_t e = trimmed.find_last_not_of (" \t");

        if (e != std::string::npos)
        {
            trimmed = trimmed.substr (0, e + 1);
        }
    }

    for (auto & c : trimmed)
    {
        c = (char) toupper ((unsigned char) c);
    }

    isExitm = (trimmed == "EXITM" || trimmed == ".EXITM");

Error:
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



    for (const auto & el : expandedLines)
    {
        std::string elTrimmed = el;
        size_t elStart = elTrimmed.find_first_not_of (" \t");

        if (elStart != std::string::npos)
        {
            elTrimmed = elTrimmed.substr (elStart);
        }

        size_t elSemi = elTrimmed.find (';');

        if (elSemi != std::string::npos)
        {
            elTrimmed = elTrimmed.substr (0, elSemi);
        }

        size_t elEnd2 = elTrimmed.find_last_not_of (" \t");

        if (elEnd2 != std::string::npos)
        {
            elTrimmed = elTrimmed.substr (0, elEnd2 + 1);
        }

        std::string elUpper2 = elTrimmed;

        for (auto & c2 : elUpper2)
        {
            c2 = (char) toupper ((unsigned char) c2);
        }

        size_t sp2 = elUpper2.find_first_of (" \t");
        std::string firstWord = (sp2 == std::string::npos) ? elUpper2 : elUpper2.substr (0, sp2);

        if (firstWord == "IF" || firstWord == ".IF" ||
            firstWord == "IFDEF" || firstWord == ".IFDEF" ||
            firstWord == "IFNDEF" || firstWord == ".IFNDEF")
        {
            ifDepth++;
        }
        else if (firstWord == "ENDIF" || firstWord == ".ENDIF")
        {
            ifDepth--;
        }
    }

Error:
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
        std::string argCountStr = std::to_string ((int) args.size ());
        size_t pos = 0;

        while ((pos = expanded.find ("\\0", pos)) != std::string::npos)
        {
            expanded.replace (pos, 2, argCountStr);
            pos += argCountStr.size ();
        }
    }

    // Replace \1 through \9 with arguments
    for (int ai = 9; ai >= 1; ai--)
    {
        std::string placeholder = "\\" + std::to_string (ai);
        size_t pos = 0;

        while ((pos = expanded.find (placeholder, pos)) != std::string::npos)
        {
            std::string replacement = (ai <= (int) args.size ()) ? args[ai - 1] : "";
            expanded.replace (pos, placeholder.size (), replacement);
            pos += replacement.size ();
        }
    }

    // Replace named parameters as whole-word matches
    for (int pi = 0; pi < (int) macroDef.paramNames.size (); pi++)
    {
        const std::string & paramName = macroDef.paramNames[pi];
        std::string replacement = (pi < (int) args.size ()) ? args[pi] : "";
        size_t pos = 0;

        while ((pos = expanded.find (paramName, pos)) != std::string::npos)
        {
            bool leftOk = (pos == 0) ||
                           (!isalnum ((unsigned char) expanded[pos - 1]) && expanded[pos - 1] != '_');
            size_t endPos = pos + paramName.size ();
            bool rightOk = (endPos >= expanded.size ()) ||
                            (!isalnum ((unsigned char) expanded[endPos]) && expanded[endPos] != '_');

            if (leftOk && rightOk)
            {
                expanded.replace (pos, paramName.size (), replacement);
                pos += replacement.size ();
            }
            else
            {
                pos += paramName.size ();
            }
        }
    }

    // Replace \? with unique suffix
    {
        size_t pos = 0;

        while ((pos = expanded.find ("\\?", pos)) != std::string::npos)
        {
            expanded.replace (pos, 2, uniqueSuffix);
            pos += uniqueSuffix.size ();
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
            size_t endPos = pos + localLabel.size ();
            bool rightOk = (endPos >= expanded.size ()) ||
                            (!isalnum ((unsigned char) expanded[endPos]) && expanded[endPos] != '_');

            if (leftOk && rightOk)
            {
                std::string suffixed = localLabel + uniqueSuffix;
                expanded.replace (pos, localLabel.size (), suffixed);
                pos += suffixed.size ();
            }
            else
            {
                pos += localLabel.size ();
            }
        }
    }

Error:
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



    while (sq < expanded.size ())
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

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::HandleColonlessLabel
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::HandleColonlessLabel (const PendingLine & current, LineInfo & info, bool & handled)
{
    HRESULT hr = S_OK;

    handled = false;



    if (!info.parsed.startsAtColumn0 || !info.parsed.label.empty () ||
        m_opcodeTable.IsMnemonic (info.parsed.mnemonic) ||
        m_macros.find (info.parsed.mnemonic) != m_macros.end ())
    {
        goto Error;
    }

    {
        std::string labelName;
        std::string labelError;

        CHR (ExtractColonlessLabelName (current, labelName));

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

        if (!info.parsed.operand.empty ())
        {
            PendingLine pl   = {};
            pl.text          = "    " + info.parsed.operand;
            pl.sourceLineNumber = current.sourceLineNumber;
            pl.macroDepth    = current.macroDepth;
            m_pendingLines.push_front (pl);
        }

        info.parsed.mnemonic.clear ();
        info.parsed.operand.clear ();
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

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ClassifyAndResolve
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ClassifyAndResolve (const PendingLine & current, LineInfo & info)
{
    HRESULT hr = S_OK;



    info.classified    = Parser::ClassifyOperand (info.parsed.operand);
    info.isInstruction = true;

    m_pass1Ctx.currentPC = (int32_t) m_pc;

    bool    exprResolved = false;
    int32_t exprValue    = 0;

    if (info.classified.syntax != OperandSyntax::None &&
        info.classified.syntax != OperandSyntax::Accumulator &&
        !info.classified.expression.empty ())
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

    CHR (ResolveAddressingAndSize (current, info, exprValue, exprResolved));

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
                m_pc += EstimateInstructionSize (info.classified.syntax, info.parsed.mnemonic);
            }
        }
    }

Error:
    return hr;
}






////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ValidateAssemblyCompletion
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ValidateAssemblyCompletion ()
{
    HRESULT hr = S_OK;



    if (m_collectingMacro)
    {
        RecordError (m_currentMacroLine, "Unclosed macro definition: " + m_currentMacroName);
    }

    if (!m_condStack.empty ())
    {
        RecordError ((int) m_lines.size (),
            "Unclosed if block (" + std::to_string (m_condStack.size ()) + " level(s) open)");
    }

Error:
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



    if (info.parsed.mnemonic != "NOP" || info.parsed.operand.empty ())
    {
        goto Error;
    }

    {
        m_pass1Ctx.currentPC = (int32_t) m_pc;
        ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.operand, m_pass1Ctx);

        if (er.success && er.value > 0)
        {
            info.isDirective         = true;
            info.parsed.isDirective  = true;
            info.parsed.directive    = ".MULTINOP";
            info.parsed.directiveArg = info.parsed.operand;
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

HRESULT AssemblySession::RunPass2 ()
{
    HRESULT hr = S_OK;



    m_image.assign (65536, m_options.fillByte);

    // Build full expression context with all symbols
    for (const auto & sym : m_symbols)
    {
        m_fullSymbols[sym.first] = (int32_t) sym.second;
    }

    CHR (ResolveEquConstants ());
    CHR (ReportUnresolvedEqus ());

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
            CHR (EmitDirectiveBytes (info, emitPC));
        }
        else if (info.isInstruction)
        {
            lineHasAddress = true;
            m_pass2Ctx.currentPC = (int32_t) info.pc;
            CHR (EmitInstruction (info, emitPC));
        }
        else if (!info.parsed.label.empty ())
        {
            lineHasAddress = true;
        }

        CHR (BuildListingEntry (info, emitPCStart, emitPC, lineHasAddress));
    }

    CHR (ExtractImage ());

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

HRESULT AssemblySession::ResolveEquConstants ()
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

            if (m_fullSymbols.find (info.parsed.constantName) != m_fullSymbols.end ())
            {
                continue;
            }

            const std::string & expr = info.parsed.constantExpr;

            if (expr.size () >= 2 && expr.front () == '"' && expr.back () == '"')
            {
                int32_t len = (int32_t) (expr.size () - 2);
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

Error:
    return hr;
}






////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::ReportUnresolvedEqus
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::ReportUnresolvedEqus ()
{
    HRESULT hr = S_OK;



    for (const auto & info : m_lineInfos)
    {
        if (!info.isConstant || !info.parsed.isConstant)
        {
            continue;
        }

        if (info.parsed.constantKind == SymbolKind::Equ &&
            m_fullSymbols.find (info.parsed.constantName) == m_fullSymbols.end ())
        {
            RecordError (info.parsed.lineNumber,
                "Cannot resolve equ expression: " + info.parsed.constantExpr);
        }
    }

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::EmitDirectiveBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::EmitDirectiveBytes (const LineInfo & info, Word & emitPC)
{
    HRESULT hr = S_OK;

    const std::string & dir = info.parsed.directive;



    if (dir == ".ORG")
    {
        // Nothing to emit
    }
    else if (dir == ".BYTE")
    {
        CHR (EmitByteDirective (info, emitPC));
    }
    else if (dir == ".WORD")
    {
        CHR (EmitWordDirective (info, emitPC));
    }
    else if (dir == ".TEXT")
    {
        std::string text = Parser::ParseQuotedString (info.parsed.directiveArg);

        for (char c : text)
        {
            EmitByte (m_charMap.table[(unsigned char) c], emitPC);
        }
    }
    else if (dir == ".DD")
    {
        CHR (EmitDdDirective (info, emitPC));
    }
    else if (dir == ".DS")
    {
        CHR (EmitDsDirective (info, emitPC));
    }
    else if (dir == ".ALIGN")
    {
        CHR (EmitAlignDirective (info, emitPC));
    }
    else if (dir == ".MULTINOP")
    {
        ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, m_pass2Ctx);

        if (er.success && er.value > 0)
        {
            for (int32_t j = 0; j < er.value; j++)
            {
                EmitByte (0xEA, emitPC);
            }
        }
    }

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
        if (arg.size () >= 2 && arg.front () == '"' && arg.back () == '"')
        {
            std::string raw       = arg.substr (1, arg.size () - 2);
            std::string processed = ProcessEscapeSequences (raw);

            for (char c : processed)
            {
                EmitByte (m_charMap.table[(unsigned char) c], emitPC);
            }
        }
        else if (arg.size () >= 2 && arg.front () == '"')
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
        else if (arg.size () >= 2 && arg[0] == '\\')
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

Error:
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



    EvaluateDirectiveArgs (info.parsed.directiveArg, m_pass2Ctx, values, info.parsed.lineNumber, m_result.errors);

    if (values.size () != 0 || info.parsed.directiveArg.empty ())
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

Error:
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



    EvaluateDirectiveArgs (info.parsed.directiveArg, m_pass2Ctx, values, info.parsed.lineNumber, m_result.errors);

    if (values.size () != 0 || info.parsed.directiveArg.empty ())
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

Error:
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



    if (!args.empty ())
    {
        ExprResult sizeEr = ExpressionEvaluator::Evaluate (args[0], m_pass2Ctx);

        if (sizeEr.success)
        {
            Byte fillVal = 0;

            if (args.size () >= 2)
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

Error:
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



    if (!info.parsed.directiveArg.empty ())
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

Error:
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



    CHR (ResolveInstructionValue (info, value, emit));

    if (emit)
    {
        CHR (EmitInstructionBytes (info, value, emitPC));
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

        if (!info.classified.expression.empty ())
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
             !info.classified.expression.empty ())
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

Error:
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

Error:
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



    if (!m_options.generateListing)
    {
        goto Error;
    }

    if (info.listingSuppressed && !info.conditionalSkip)
    {
        goto Error;
    }

    {
        AssemblyLine listLine = {};
        listLine.lineNumber = info.parsed.lineNumber;

        if (info.parsed.lineNumber >= 1 && info.parsed.lineNumber <= (int) m_lines.size ())
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

HRESULT AssemblySession::ExtractImage ()
{
    HRESULT hr = S_OK;



    if (m_lowestAddr <= m_highestAddr)
    {
        m_result.bytes.assign (m_image.begin () + m_lowestAddr, m_image.begin () + m_highestAddr + 1);
        m_result.startAddress = m_lowestAddr;
        m_result.endAddress   = (Word) (m_highestAddr + 1);
    }
    else
    {
        m_result.bytes.clear ();
        m_result.endAddress = m_result.startAddress;
    }

Error:
    return hr;
}






////////////////////////////////////////////////////////////////////////////////
//
//  AssemblySession::DetectUnusedLabels
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblySession::DetectUnusedLabels ()
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

        if (kindIt == m_symbolKinds.end () || kindIt->second != SymbolKind::Label)
        {
            continue;
        }

        if (m_referencedLabels.find (sym.first) == m_referencedLabels.end ())
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

Error:
    return hr;
}






////////////////////////////////////////////////////////////////////////////////
//
//  Assemble
//
////////////////////////////////////////////////////////////////////////////////

AssemblyResult Assembler::Assemble (const std::string & sourceText)
{
    AssemblySession session (m_opcodeTable, m_options);
    return session.Run (sourceText);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatListingLine
//
////////////////////////////////////////////////////////////////////////////////

std::string Assembler::FormatListingLine (const AssemblyLine & line, bool showCycleCounts)
{
    // Line number column (cols 1-5, right-justified)
    std::string lineNumStr = std::format ("{:5d}", line.lineNumber);

    // Address column (cols 7-10, 4 hex digits, no $ prefix)
    std::string addrStr;

    if (line.isConditionalSkip)
    {
        addrStr = "   -";
    }
    else if (line.hasAddress)
    {
        addrStr = std::format ("{:04X}", line.address);
    }
    else
    {
        addrStr = "    ";
    }

    // Bytes column (cols 14-22, up to 3 hex bytes, padded to 9 chars)
    std::string bytesStr;

    for (size_t i = 0; i < line.bytes.size () && i < 3; i++)
    {
        if (i > 0)
        {
            bytesStr += " ";
        }

        bytesStr += std::format ("{:02X}", line.bytes[i]);
    }

    while (bytesStr.size () < 9)
    {
        bytesStr += " ";
    }

    // Cycle counts column (optional, between bytes and prefix)
    std::string cycleStr;

    if (showCycleCounts && line.cycleCounts > 0)
    {
        cycleStr = std::format ("[{}] ", line.cycleCounts);
    }

    // Macro expansion prefix (col 23)
    std::string prefix = line.isMacroExpansion ? ">" : " ";

    // AS65 layout: linenum(5) space(1) addr(4) spaces(3) bytes(9) prefix(1) source
    return lineNumStr + " " + addrStr + "   " +
           bytesStr + cycleStr + prefix + line.sourceText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatSymbolTable
//
////////////////////////////////////////////////////////////////////////////////

std::string Assembler::FormatSymbolTable (const std::unordered_map<std::string, Word> & symbols,
                                           const std::unordered_map<std::string, SymbolKind> & symbolKinds)
{
    // Sort symbols alphabetically
    std::vector<std::pair<std::string, Word>> sorted (symbols.begin (), symbols.end ());

    std::sort (sorted.begin (), sorted.end (),
        [] (const auto & a, const auto & b) { return a.first < b.first; });

    std::string output;

    for (const auto & pair : sorted)
    {
        auto kindIt = symbolKinds.find (pair.first);
        bool isRedefinable = (kindIt != symbolKinds.end () && kindIt->second == SymbolKind::Set);

        std::string fullName = (isRedefinable ? "*" : "") + pair.first;
        output += std::format ("{:<16s}${:04X}\n", fullName, pair.second);
    }

    return output;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatDebugInfo
//
////////////////////////////////////////////////////////////////////////////////

std::string Assembler::FormatDebugInfo (const std::unordered_map<std::string, Word> & symbols)
{
    // Sort symbols by address for deterministic output
    std::vector<std::pair<std::string, Word>> sorted (symbols.begin (), symbols.end ());

    std::sort (sorted.begin (), sorted.end (),
        [] (const auto & a, const auto & b) { return a.second < b.second; });

    std::string output;

    for (const auto & pair : sorted)
    {
        output += std::format ("{}=${:04X}\n", pair.first, pair.second);
    }

    return output;
}
