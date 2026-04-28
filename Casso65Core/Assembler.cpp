#include "Pch.h"

#include "Assembler.h"
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





////////////////////////////////////////////////////////////////////////////////
//
//  GetLowerExtension
//
////////////////////////////////////////////////////////////////////////////////

static std::string GetLowerExtension (const std::string & filename)
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

static int HexCharToNibble (char c)
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

static int HexByte (const std::string & s, size_t offset)
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

static std::vector<Byte> ParseSRecord (const std::string & content)
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

static std::vector<Byte> ParseIntelHex (const std::string & content)
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

static std::vector<std::string> GenerateByteDirectives (const std::vector<Byte> & data)
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

static bool IsBranchMnemonic (const std::string & mnemonic)
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

static GlobalAddressingMode::AddressingMode ResolveAddressingMode (
    OperandSyntax    syntax,
    const std::string & mnemonic,
    int32_t          value,
    bool             resolved,
    const OpcodeTable & opcodeTable)
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
        // Try ZeroPageX first if value fits and instruction supports it
        if (resolved && value >= 0 && value <= 0xFF)
        {
            OpcodeEntry entry = {};

            if (opcodeTable.Lookup (mnemonic, AM::ZeroPageX, entry))
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

            if (opcodeTable.Lookup (mnemonic, AM::ZeroPageY, entry))
            {
                return AM::ZeroPageY;
            }
        }

        return AM::AbsoluteY;
    }

    case OperandSyntax::Bare:
    {
        // Branch instructions are always relative
        if (IsBranchMnemonic (mnemonic))
        {
            return AM::Relative;
        }

        // JMP/JSR use JumpAbsolute
        if (mnemonic == "JMP" || mnemonic == "JSR")
        {
            return AM::JumpAbsolute;
        }

        // Try ZeroPage if value fits and instruction supports it
        if (resolved && value >= 0 && value <= 0xFF)
        {
            OpcodeEntry entry = {};

            if (opcodeTable.Lookup (mnemonic, AM::ZeroPage, entry))
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

static Byte EstimateInstructionSize (OperandSyntax syntax, const std::string & mnemonic)
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

        return 3;  // Conservative: assume absolute
    }
    }

    return 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluateDirectiveArgs — evaluate comma-separated expression list
//
////////////////////////////////////////////////////////////////////////////////

static std::string ProcessEscapeSequences (const std::string & str)
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

static bool EvaluateDirectiveArgs (
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
//  Assemble
//
////////////////////////////////////////////////////////////////////////////////

AssemblyResult Assembler::Assemble (const std::string & sourceText)
{
    AssemblyResult result = {};
    result.success      = true;
    result.startAddress = 0;

    auto lines = Parser::SplitLines (sourceText);

    // ---- Line info for both passes ----
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

    std::vector<LineInfo>                              lineInfos;
    std::unordered_map<std::string, Word>              symbols;
    std::unordered_map<std::string, SymbolKind>        symbolKinds;
    Word                                               pc        = result.startAddress;
    bool                                               originSet = false;
    bool                                               endAssembly = false;

    // Three-segment model: code/data/bss each have independent PCs
    enum class Segment { Code, Data, Bss };
    Segment                                            currentSegment = Segment::Code;
    Word                                               segmentPC[3]   = { 0, 0, 0 };

    // Build a Pass 1 expression context (symbols populated as we go)
    std::unordered_map<std::string, int32_t>  exprSymbols;
    ExprContext                                pass1Ctx = { &exprSymbols, 0 };

    // Inject AS65 built-in symbols (overridable by -d flag)
    auto injectBuiltin = [&] (const std::string & name, int32_t value)
    {
        symbols[name]     = (Word) value;
        symbolKinds[name] = SymbolKind::Set;
        exprSymbols[name] = value;
    };

    injectBuiltin ("ERRORS",     0);
    injectBuiltin ("__65SC02__", 0);
    injectBuiltin ("__6502X__",  0);

    // Inject predefined symbols (-d flag) — after built-ins so user can override
    for (const auto & predef : m_options.predefinedSymbols)
    {
        symbols[predef.first]     = (Word) predef.second;
        symbolKinds[predef.first] = SymbolKind::Equ;
        exprSymbols[predef.first] = predef.second;
    }

    // Conditional assembly stack
    std::vector<ConditionalState> condStack;

    // Macro definitions and expansion state
    std::unordered_map<std::string, MacroDefinition> macros;
    bool         collectingMacro = false;
    std::string  currentMacroName;
    int          currentMacroLine = 0;
    std::vector<std::string> currentMacroBody;
    std::vector<std::string> currentMacroParams;
    std::vector<std::string> currentMacroLocals;
    int          macroUniqueCounter = 0;
    static const int kMaxMacroDepth   = 15;
    static const int kMaxIncludeDepth = 16;

    // Listing control state
    int listingLevel = m_options.generateListing ? 1 : 0;

    // Struct definitions
    std::unordered_map<std::string, StructDefinition> structs;
    bool        collectingStruct = false;
    StructDefinition currentStruct = {};

    // Character map
    CharacterMap charMap;

    // Helper: check if currently assembling (all enclosing conditions true)
    auto isAssembling = [&condStack] ()
    {
        return condStack.empty () || condStack.back ().assembling;
    };

    // ---- Pass 1: Parse, collect labels, compute PC ----
    // Use a processing queue to support macro expansion inline
    struct PendingLine
    {
        std::string text;
        int         sourceLineNumber;  // Original source line for error reporting
        int         macroDepth;        // Current macro nesting depth
        int         includeDepth;      // Current include nesting depth
        std::string sourceFile;        // Source file name (empty = main)
    };

    std::deque<PendingLine> pendingLines;

    for (int i = 0; i < (int) lines.size (); i++)
    {
        PendingLine pl = {};
        pl.text = lines[i];
        pl.sourceLineNumber = i + 1;
        pl.macroDepth = 0;
        pendingLines.push_back (pl);
    }

    while (!pendingLines.empty ())
    {
        PendingLine current = pendingLines.front ();
        pendingLines.pop_front ();

        // Stop processing if .END was encountered
        if (endAssembly)
        {
            continue;
        }

        LineInfo info     = {};
        info.parsed       = Parser::ParseLine (current.text, current.sourceLineNumber);
        info.pc           = pc;
        info.isInstruction = false;
        info.isDirective   = false;
        info.isConstant    = false;
        info.hasError      = false;
        info.valueResolved = false;
        info.resolvedValue = 0;
        info.resolvedMode  = GlobalAddressingMode::SingleByteNoOperand;
        info.macroDepth       = current.macroDepth;
        info.conditionalSkip  = false;
        info.listingSuppressed = (listingLevel <= 0);

        // ---- Struct definition collection ----
        if (collectingStruct)
        {
            std::string mnUpper = info.parsed.mnemonic;

            // Check for "end struct" — Parser converts "end" to .END directive
            bool isEndStruct = false;

            if (info.parsed.isDirective && info.parsed.directive == ".END")
            {
                std::string endArg = info.parsed.directiveArg;
                std::string endArgUpper = endArg;

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
                    isEndStruct = true;
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
                    isEndStruct = true;
                }
            }

            if (isEndStruct)
            {
                // Record struct size as a symbol
                int32_t structSize = currentStruct.currentOffset - currentStruct.startOffset;
                symbols[currentStruct.name]     = (Word) structSize;
                symbolKinds[currentStruct.name]  = SymbolKind::Equ;
                exprSymbols[currentStruct.name] = structSize;
                structs[currentStruct.name]     = currentStruct;
                collectingStruct = false;
            }
            else if (!info.parsed.isEmpty)
            {
                // Parse member: MEMBER ds SIZE or MEMBER db or MEMBER dw etc.
                // The member name is in the mnemonic position (column 0)
                // and the size directive is in the operand
                std::string memberName;
                int32_t     memberSize = 0;

                if (!mnUpper.empty () && !info.parsed.operand.empty ())
                {
                    // Could be: MEMBER ds SIZE
                    std::string opStr = info.parsed.operand;
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
                        pass1Ctx.currentPC = (int32_t) pc;
                        ExprResult er = ExpressionEvaluator::Evaluate (sizeExpr, pass1Ctx);

                        if (er.success)
                        {
                            memberSize = er.value;
                        }
                        else
                        {
                            memberSize = 1;
                        }
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
                    member.offset = currentStruct.currentOffset;
                    member.size   = memberSize;
                    currentStruct.members.push_back (member);

                    // Define symbol: STRUCTNAME.MEMBER = offset
                    std::string symName = currentStruct.name + "." + memberName;
                    symbols[symName]     = (Word) currentStruct.currentOffset;
                    symbolKinds[symName]  = SymbolKind::Equ;
                    exprSymbols[symName] = currentStruct.currentOffset;

                    currentStruct.currentOffset += memberSize;
                }
            }

            lineInfos.push_back (info);
            continue;
        }

        // ---- Macro definition collection ----
        if (collectingMacro)
        {
            // Check for endm (end macro)
            std::string mnUpper = info.parsed.mnemonic;

            if (mnUpper == "ENDM" || (info.parsed.isDirective && info.parsed.directive == ".ENDM"))
            {
                MacroDefinition def = {};
                def.name       = currentMacroName;
                def.body       = currentMacroBody;
                def.paramNames = currentMacroParams;
                def.localLabels = currentMacroLocals;
                def.lineNumber = currentMacroLine;
                macros[currentMacroName] = def;
                collectingMacro = false;
            }
            else
            {
                // Check for "local" directive inside macro body
                std::string bodyMn = info.parsed.mnemonic;

                if (bodyMn == "LOCAL" || (info.parsed.isDirective && info.parsed.directive == ".LOCAL"))
                {
                    std::string localArg = bodyMn == "LOCAL" ? info.parsed.operand : info.parsed.directiveArg;
                    auto localNames = Parser::SplitArgList (localArg);

                    for (const auto & ln : localNames)
                    {
                        // Trim and store
                        std::string name = ln;
                        size_t ns = name.find_first_not_of (" \t");
                        size_t ne = name.find_last_not_of (" \t");

                        if (ns != std::string::npos)
                        {
                            name = name.substr (ns, ne - ns + 1);
                        }

                        if (!name.empty ())
                        {
                            currentMacroLocals.push_back (name);
                        }
                    }
                }

                currentMacroBody.push_back (current.text);
            }

            lineInfos.push_back (info);
            continue;
        }

        // ---- Check for macro definition start (NAME macro) ----
        if (!info.parsed.mnemonic.empty () && info.parsed.mnemonic == "MACRO" && isAssembling ())
        {
            // The macro name is in the label field (label: macro) or
            // we need to re-parse: the line is "NAME macro [params]"
            // Actually, ParseLine would have parsed "NAME" as label if it had ':'
            // For AS65 syntax: "NAME macro" where NAME is at column 0
            // ParseLine parsed NAME as mnemonic="NAME", operand is empty
            // But actually ParseLine returns mnemonic="MACRO" because it uppercases
            // The actual name was the first word before "macro"
            // Let me re-examine: "trap macro" → mnemonic="TRAP", operand="macro"
            // Wait no - "trap macro" → first word is "trap", rest is "macro"
            // With ParseLine: mnemonic="TRAP", operand="macro"
            // So mnemonic == "MACRO" means the line is literally "macro ..."
            // For "NAME macro", mnemonic is "NAME" and operand starts with "macro"
            // I need to handle this differently.
        }

        // Re-check: For AS65 macro syntax "NAME macro [params]":
        // ParseLine gives mnemonic="NAME", operand="macro" or "macro param1, param2"
        // So we detect: if operand starts with "macro" (case-insensitive)
        std::string operandUpper;

        if (!info.parsed.operand.empty ())
        {
            operandUpper = info.parsed.operand;

            for (auto & c : operandUpper)
            {
                c = (char) toupper ((unsigned char) c);
            }
        }

        bool isMacroDef = false;

        if (!info.parsed.mnemonic.empty () && !info.parsed.isEmpty && isAssembling ())
        {
            // "NAME macro [params]" — operand starts with "macro"
            if (operandUpper.substr (0, 5) == "MACRO" &&
                (operandUpper.size () == 5 || operandUpper[5] == ' ' || operandUpper[5] == '\t'))
            {
                isMacroDef = true;
            }
        }

        if (isMacroDef)
        {
            // Check for name collision with mnemonics
            if (m_opcodeTable.IsMnemonic (info.parsed.mnemonic))
            {
                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;
                error.message    = "Macro name conflicts with mnemonic: " + info.parsed.mnemonic;
                result.errors.push_back (error);
                result.success = false;
            }

            collectingMacro  = true;
            currentMacroName = info.parsed.mnemonic;
            currentMacroLine = current.sourceLineNumber;
            currentMacroBody.clear ();
            currentMacroParams.clear ();
            currentMacroLocals.clear ();

            // Parse parameter names (after "macro" keyword)
            std::string paramStr;

            if (operandUpper.size () > 5)
            {
                paramStr = info.parsed.operand.substr (6);
            }

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
                        currentMacroParams.push_back (name);
                    }
                }
            }

            lineInfos.push_back (info);
            continue;
        }

        // ---- Conditional assembly directives ----
        // Detect if/else/endif in both mnemonic and directive forms
        std::string condDirective;
        std::string condArg;

        if (info.parsed.isDirective)
        {
            std::string dir = info.parsed.directive;

            if (dir == ".IF")         { condDirective = "IF";     condArg = info.parsed.directiveArg; }
            else if (dir == ".IFDEF")  { condDirective = "IFDEF";  condArg = info.parsed.directiveArg; }
            else if (dir == ".IFNDEF") { condDirective = "IFNDEF"; condArg = info.parsed.directiveArg; }
            else if (dir == ".ELSE")   { condDirective = "ELSE"; }
            else if (dir == ".ENDIF")  { condDirective = "ENDIF"; }
        }
        else if (!info.parsed.mnemonic.empty ())
        {
            if (info.parsed.mnemonic == "IF")         { condDirective = "IF";     condArg = info.parsed.operand; }
            else if (info.parsed.mnemonic == "IFDEF")  { condDirective = "IFDEF";  condArg = info.parsed.operand; }
            else if (info.parsed.mnemonic == "IFNDEF") { condDirective = "IFNDEF"; condArg = info.parsed.operand; }
            else if (info.parsed.mnemonic == "ELSE")   { condDirective = "ELSE"; }
            else if (info.parsed.mnemonic == "ENDIF")  { condDirective = "ENDIF"; }
        }

        if (!condDirective.empty ())
        {
            if (condDirective == "IF")
            {
                ConditionalState state = {};
                state.parentAssembling = isAssembling ();
                state.seenElse = false;

                if (state.parentAssembling)
                {
                    pass1Ctx.currentPC = (int32_t) pc;
                    ExprResult er = ExpressionEvaluator::Evaluate (condArg, pass1Ctx);

                    if (!er.success)
                    {
                        AssemblyError error = {};
                        error.lineNumber = current.sourceLineNumber;
                        error.message    = "Cannot evaluate if expression: " + er.error;
                        result.errors.push_back (error);
                        result.success = false;
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

                condStack.push_back (state);
            }
            else if (condDirective == "IFDEF" || condDirective == "IFNDEF")
            {
                ConditionalState state = {};
                state.parentAssembling = isAssembling ();
                state.seenElse = false;

                if (state.parentAssembling)
                {
                    std::string symName = condArg;

                    // Trim whitespace
                    size_t s = symName.find_first_not_of (" \t");
                    size_t e = symName.find_last_not_of (" \t");

                    if (s != std::string::npos)
                    {
                        symName = symName.substr (s, e - s + 1);
                    }

                    bool defined = (exprSymbols.find (symName) != exprSymbols.end ());
                    state.assembling = (condDirective == "IFDEF") ? defined : !defined;
                }
                else
                {
                    state.assembling = false;
                }

                condStack.push_back (state);
            }
            else if (condDirective == "ELSE")
            {
                if (condStack.empty ())
                {
                    AssemblyError error = {};
                    error.lineNumber = current.sourceLineNumber;
                    error.message    = "else without matching if";
                    result.errors.push_back (error);
                    result.success = false;
                }
                else if (condStack.back ().seenElse)
                {
                    AssemblyError error = {};
                    error.lineNumber = current.sourceLineNumber;
                    error.message    = "Duplicate else";
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    condStack.back ().seenElse = true;

                    if (condStack.back ().parentAssembling)
                    {
                        condStack.back ().assembling = !condStack.back ().assembling;
                    }
                }
            }
            else if (condDirective == "ENDIF")
            {
                if (condStack.empty ())
                {
                    AssemblyError error = {};
                    error.lineNumber = current.sourceLineNumber;
                    error.message    = "endif without matching if";
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    condStack.pop_back ();
                }
            }

            info.isDirective = true;
            lineInfos.push_back (info);
            continue;
        }

        // Skip lines in non-assembling conditional blocks
        if (!isAssembling ())
        {
            info.conditionalSkip = true;
            lineInfos.push_back (info);
            continue;
        }

        // Handle .org BEFORE recording label
        if (info.parsed.isDirective && info.parsed.directive == ".ORG")
        {
            pass1Ctx.currentPC = (int32_t) pc;
            ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, pass1Ctx);

            if (!er.success)
            {
                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;
                error.message    = ".org expression must be resolvable: " + er.error;
                result.errors.push_back (error);
                result.success = false;
            }
            else
            {
                Word newAddr = (Word) er.value;

                {
                    if (originSet && newAddr == pc)
                    {
                        RecordWarning (result, current.sourceLineNumber, "Redundant .org to current address");
                    }

                    pc      = newAddr;
                    info.pc = pc;

                    if (!originSet)
                    {
                        result.startAddress = newAddr;
                        originSet = true;
                    }
                }
            }

            info.isDirective = true;
            lineInfos.push_back (info);
            continue;
        }

        // Handle segment switches BEFORE recording label
        if (info.parsed.isDirective)
        {
            const std::string & dir = info.parsed.directive;

            if (dir == ".SEGMENT_CODE" || dir == ".SEGMENT_DATA" || dir == ".SEGMENT_BSS" ||
                dir == ".CODE"         || dir == ".DATA"         || dir == ".BSS")
            {
                // Save current PC to current segment
                segmentPC[(int) currentSegment] = pc;

                // Switch segment
                if (dir == ".SEGMENT_CODE" || dir == ".CODE") currentSegment = Segment::Code;
                else if (dir == ".SEGMENT_DATA" || dir == ".DATA") currentSegment = Segment::Data;
                else currentSegment = Segment::Bss;

                // Restore target segment's PC
                pc      = segmentPC[(int) currentSegment];
                info.pc = pc;

                info.isDirective = true;
                lineInfos.push_back (info);
                continue;
            }
        }

        // Record label
        if (!info.parsed.label.empty ())
        {
            std::string labelError;

            if (!Parser::ValidateLabel (info.parsed.label, m_opcodeTable, labelError))
            {
                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;
                error.message    = labelError;
                result.errors.push_back (error);
                result.success = false;
            }
            else if (symbols.count (info.parsed.label) > 0)
            {
                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;
                error.message    = "Duplicate label: " + info.parsed.label;
                result.errors.push_back (error);
                result.success = false;
            }
            else
            {
                symbols[info.parsed.label]     = pc;
                symbolKinds[info.parsed.label]  = SymbolKind::Label;
                exprSymbols[info.parsed.label] = (int32_t) pc;

                // Warn if label resembles mnemonic by case (FR-033a)
                std::string upper = info.parsed.label;

                for (auto & c : upper)
                {
                    c = (char) toupper ((unsigned char) c);
                }

                if (upper != info.parsed.label && m_opcodeTable.IsMnemonic (upper))
                {
                    RecordWarning (result, current.sourceLineNumber, "Label name resembles mnemonic: " + info.parsed.label);
                }
            }
        }

        // Handle constant definitions (NAME = EXPR, NAME equ EXPR, NAME set EXPR)
        if (info.parsed.isConstant)
        {
            info.isConstant = true;

            pass1Ctx.currentPC = (int32_t) pc;

            // For Set/= constants: evaluate eagerly in Pass 1
            // For Equ constants: defer to Pass 2 (can forward-reference labels)
            if (info.parsed.constantKind == SymbolKind::Set)
            {
                ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, pass1Ctx);

                if (!er.success)
                {
                    AssemblyError error = {};
                    error.lineNumber = current.sourceLineNumber;
                    error.message    = "Cannot evaluate constant expression: " + er.error;
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    // Check for cross-kind redefinition
                    auto kindIt = symbolKinds.find (info.parsed.constantName);

                    if (kindIt != symbolKinds.end () && kindIt->second != SymbolKind::Set)
                    {
                        AssemblyError error = {};
                        error.lineNumber = current.sourceLineNumber;
                        error.message    = "Cannot redefine " + info.parsed.constantName + " (was defined as immutable)";
                        result.errors.push_back (error);
                        result.success = false;
                    }
                    else
                    {
                        symbols[info.parsed.constantName]     = (Word) er.value;
                        symbolKinds[info.parsed.constantName]  = SymbolKind::Set;
                        exprSymbols[info.parsed.constantName] = er.value;
                    }
                }
            }
            else // Equ
            {
                // Check if already defined
                auto kindIt = symbolKinds.find (info.parsed.constantName);

                if (kindIt != symbolKinds.end ())
                {
                    AssemblyError error = {};
                    error.lineNumber = current.sourceLineNumber;

                    if (kindIt->second == SymbolKind::Equ)
                    {
                        error.message = "Duplicate equ definition: " + info.parsed.constantName;
                    }
                    else
                    {
                        error.message = "Cannot redefine " + info.parsed.constantName + " as equ (already defined as different kind)";
                    }

                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    // Reserve the name — actual value set in Pass 2
                    symbolKinds[info.parsed.constantName] = SymbolKind::Equ;

                    // Check for string literal — value is the string length
                    const std::string & expr = info.parsed.constantExpr;

                    if (expr.size () >= 2 && expr.front () == '"' && expr.back () == '"')
                    {
                        int32_t len = (int32_t) (expr.size () - 2);
                        symbols[info.parsed.constantName]     = (Word) len;
                        exprSymbols[info.parsed.constantName] = len;
                    }
                    else
                    {
                        // Try to evaluate in Pass 1 (may succeed if no forward refs)
                        ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, pass1Ctx);

                        if (er.success)
                        {
                            symbols[info.parsed.constantName]     = (Word) er.value;
                            exprSymbols[info.parsed.constantName] = er.value;
                        }
                    }
                }
            }

            lineInfos.push_back (info);
            continue;
        }

        // Handle data directives
        if (info.parsed.isDirective)
        {
            info.isDirective = true;

            if (info.parsed.directive == ".BYTE")
            {
                // Count items for PC advancement
                pass1Ctx.currentPC = (int32_t) pc;
                std::vector<int32_t> values;
                std::vector<AssemblyError> tempErrors;
                EvaluateDirectiveArgs (info.parsed.directiveArg, pass1Ctx, values, current.sourceLineNumber, tempErrors);

                // If evaluation fails, try counting comma-separated items
                if (values.empty () && !info.parsed.directiveArg.empty ())
                {
                    auto args = Parser::SplitArgList (info.parsed.directiveArg);
                    pc += (Word) args.size ();
                }
                else
                {
                    pc += (Word) values.size ();
                }
            }
            else if (info.parsed.directive == ".WORD")
            {
                auto args = Parser::SplitArgList (info.parsed.directiveArg);
                pc += (Word) (args.size () * 2);
            }
            else if (info.parsed.directive == ".TEXT")
            {
                std::string text = Parser::ParseQuotedString (info.parsed.directiveArg);
                pc += (Word) text.size ();
            }
            else if (info.parsed.directive == ".DD")
            {
                auto args = Parser::SplitArgList (info.parsed.directiveArg);
                pc += (Word) (args.size () * 4);
            }
            else if (info.parsed.directive == ".DS")
            {
                pass1Ctx.currentPC = (int32_t) pc;
                auto args = Parser::SplitArgList (info.parsed.directiveArg);

                if (!args.empty ())
                {
                    ExprResult er = ExpressionEvaluator::Evaluate (args[0], pass1Ctx);

                    if (!er.success)
                    {
                        AssemblyError error = {};
                        error.lineNumber = current.sourceLineNumber;
                        error.message    = ".ds size must be resolvable: " + er.error;
                        result.errors.push_back (error);
                        result.success = false;
                    }
                    else
                    {
                        pc += (Word) er.value;
                    }
                }
            }
            else if (info.parsed.directive == ".ALIGN")
            {
                pass1Ctx.currentPC = (int32_t) pc;
                int alignment = 2;  // Default: align to even address

                if (!info.parsed.directiveArg.empty ())
                {
                    ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, pass1Ctx);

                    if (!er.success)
                    {
                        AssemblyError error = {};
                        error.lineNumber = current.sourceLineNumber;
                        error.message    = ".align expression must be resolvable: " + er.error;
                        result.errors.push_back (error);
                        result.success = false;
                    }
                    else
                    {
                        alignment = er.value;
                    }
                }

                if (alignment > 0)
                {
                    int remainder2 = pc % alignment;

                    if (remainder2 != 0)
                    {
                        pc += (Word) (alignment - remainder2);
                    }
                }
            }
            else if (info.parsed.directive == ".END")
            {
                endAssembly = true;
            }
            else if (info.parsed.directive == ".ERROR")
            {
                std::string msg = Parser::ParseQuotedString (info.parsed.directiveArg);

                if (msg.empty () && !info.parsed.directiveArg.empty ())
                {
                    msg = info.parsed.directiveArg;
                }

                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;
                error.message    = msg.empty () ? "User error directive" : msg;
                result.errors.push_back (error);
                result.success = false;
            }
            else if (info.parsed.directive == ".OPT_NOOP")
            {
                // Recognized but intentionally no-op
            }
            else if (info.parsed.directive == ".LIST")
            {
                listingLevel++;
            }
            else if (info.parsed.directive == ".NOLIST")
            {
                listingLevel--;
            }
            else if (info.parsed.directive == ".PAGE")
            {
                // Page break in listing — handled at listing output time
            }
            else if (info.parsed.directive == ".TITLE")
            {
                result.listingTitle = Parser::ParseQuotedString (info.parsed.directiveArg);

                if (result.listingTitle.empty () && !info.parsed.directiveArg.empty ())
                {
                    result.listingTitle = info.parsed.directiveArg;
                }
            }
            else if (info.parsed.directive == ".INCLUDE")
            {
                if (m_options.fileReader == nullptr)
                {
                    AssemblyError error = {};
                    error.lineNumber = current.sourceLineNumber;
                    error.message    = "No file reader configured for include";
                    result.errors.push_back (error);
                    result.success = false;
                }
                else if (current.includeDepth >= kMaxIncludeDepth)
                {
                    AssemblyError error = {};
                    error.lineNumber = current.sourceLineNumber;
                    error.message    = "Include nesting depth exceeded (max " + std::to_string (kMaxIncludeDepth) + ")";
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    std::string filename = Parser::ParseQuotedString (info.parsed.directiveArg);

                    if (filename.empty ())
                    {
                        filename = info.parsed.directiveArg;

                        // Trim whitespace
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
                        AssemblyError error = {};
                        error.lineNumber = current.sourceLineNumber;
                        error.message    = fr.error;
                        result.errors.push_back (error);
                        result.success = false;
                    }
                    else
                    {
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
                            // Binary include — push synthetic .byte directives
                            for (int il = (int) synthLines.size () - 1; il >= 0; il--)
                            {
                                PendingLine pl = {};
                                pl.text             = synthLines[il];
                                pl.sourceLineNumber = current.sourceLineNumber;
                                pl.macroDepth       = current.macroDepth;
                                pl.includeDepth     = current.includeDepth + 1;
                                pl.sourceFile       = filename;
                                pendingLines.push_front (pl);
                            }
                        }
                        else if (ext != ".bin" && ext != ".s19" && ext != ".s28"
                              && ext != ".s37" && ext != ".hex")
                        {
                            // Assembly source include
                            auto includeLines = Parser::SplitLines (fr.contents);

                            for (int il = (int) includeLines.size () - 1; il >= 0; il--)
                            {
                                PendingLine pl = {};
                                pl.text             = includeLines[il];
                                pl.sourceLineNumber = il + 1;
                                pl.macroDepth       = current.macroDepth;
                                pl.includeDepth     = current.includeDepth + 1;
                                pl.sourceFile       = filename;
                                pendingLines.push_front (pl);
                            }
                        }
                        // else: binary format with no data — nothing to emit
                    }
                }
            }
            else if (info.parsed.directive == ".STRUCT")
            {
                // Start struct definition: struct NAME [, OFFSET]
                auto args = Parser::SplitArgList (info.parsed.directiveArg);

                if (args.empty ())
                {
                    AssemblyError error = {};
                    error.lineNumber = current.sourceLineNumber;
                    error.message    = "struct requires a name";
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    currentStruct = {};
                    currentStruct.name = args[0];
                    currentStruct.startOffset = 0;

                    if (args.size () >= 2)
                    {
                        pass1Ctx.currentPC = (int32_t) pc;
                        ExprResult er = ExpressionEvaluator::Evaluate (args[1], pass1Ctx);

                        if (er.success)
                        {
                            currentStruct.startOffset = er.value;
                        }
                    }

                    currentStruct.currentOffset = currentStruct.startOffset;
                    collectingStruct = true;
                }
            }
            else if (info.parsed.directive == ".CMAP")
            {
                // Character map directive
                std::string arg = info.parsed.directiveArg;
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

                if (arg == "0")
                {
                    // Reset to identity
                    for (int ci = 0; ci < 256; ci++)
                    {
                        charMap.table[ci] = (Byte) ci;
                    }
                }
                else if (arg.size () >= 5 && arg[0] == '\'')
                {
                    // Parse char or range mapping
                    size_t eqPos = arg.find ('=');
                    size_t dashPos = arg.find ('-', 1);

                    if (eqPos != std::string::npos)
                    {
                        std::string lhs = arg.substr (0, eqPos);
                        std::string rhs = arg.substr (eqPos + 1);

                        // Trim
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

                        pass1Ctx.currentPC = (int32_t) pc;
                        ExprResult rhsVal = ExpressionEvaluator::Evaluate (rhs, pass1Ctx);

                        if (rhsVal.success)
                        {
                            // Check for range: 'A'-'Z'=$C1
                            if (dashPos != std::string::npos && dashPos < eqPos &&
                                lhs.size () >= 7 && lhs[0] == '\'' && lhs[2] == '\'')
                            {
                                char startChar = lhs[1];
                                // Find the end char after dash
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
                                        charMap.table[ci] = (Byte) (rhsVal.value + (ci - (unsigned char) startChar));
                                    }
                                }
                            }
                            else if (lhs.size () >= 3 && lhs[0] == '\'' && lhs[2] == '\'')
                            {
                                // Single char: 'A'=$41
                                charMap.table[(unsigned char) lhs[1]] = (Byte) rhsVal.value;
                            }
                        }
                    }
                }
            }

            lineInfos.push_back (info);
            continue;
        }

        // Skip empty lines
        if (info.parsed.mnemonic.empty ())
        {
            lineInfos.push_back (info);
            continue;
        }

        // ---- Multi-NOP: nop EXPR emits multiple NOP bytes ----
        if (info.parsed.mnemonic == "NOP" && !info.parsed.operand.empty ())
        {
            pass1Ctx.currentPC = (int32_t) pc;
            ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.operand, pass1Ctx);

            if (er.success && er.value > 0)
            {
                info.isDirective = true;
                info.parsed.isDirective = true;
                info.parsed.directive = ".MULTINOP";
                info.parsed.directiveArg = info.parsed.operand;
                pc += (Word) er.value;
            }

            lineInfos.push_back (info);
            continue;
        }

        // ---- Macro invocation ----
        auto macroIt = macros.find (info.parsed.mnemonic);

        if (macroIt != macros.end ())
        {
            if (current.macroDepth >= kMaxMacroDepth)
            {
                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;
                error.message    = "Macro nesting depth exceeded (max " + std::to_string (kMaxMacroDepth) + ")";
                result.errors.push_back (error);
                result.success = false;
                lineInfos.push_back (info);
                continue;
            }

            // Split arguments
            std::vector<std::string> args;

            if (!info.parsed.operand.empty ())
            {
                args = Parser::SplitArgList (info.parsed.operand);
            }

            // Generate unique suffix for \?
            macroUniqueCounter++;
            std::string uniqueSuffix = std::format ("{:04d}", macroUniqueCounter);

            // Expand macro body with argument substitution
            const auto & macroDef = macroIt->second;
            const auto & body     = macroDef.body;

            // Build expanded lines, stopping at exitm
            std::vector<std::string> expandedLines;

            for (int bi = 0; bi < (int) body.size (); bi++)
            {
                std::string expanded = body[bi];

                // Check for exitm directive (before substitution)
                std::string exTrimmed = expanded;
                size_t exStart = exTrimmed.find_first_not_of (" \t");

                if (exStart != std::string::npos)
                {
                    exTrimmed = exTrimmed.substr (exStart);
                }

                // Strip comments for exitm check
                size_t scPos = exTrimmed.find (';');

                if (scPos != std::string::npos)
                {
                    exTrimmed = exTrimmed.substr (0, scPos);
                }

                size_t exEnd = exTrimmed.find_last_not_of (" \t");

                if (exEnd != std::string::npos)
                {
                    exTrimmed = exTrimmed.substr (0, exEnd + 1);
                }

                std::string exUpper = exTrimmed;

                for (auto & ec : exUpper)
                {
                    ec = (char) toupper ((unsigned char) ec);
                }

                if (exUpper == "EXITM" || exUpper == ".EXITM")
                {
                    // Count unmatched if blocks in already-expanded lines
                    // and add compensating endif lines
                    int ifDepth = 0;

                    for (const auto & el : expandedLines)
                    {
                        std::string elTrimmed = el;
                        size_t elStart = elTrimmed.find_first_not_of (" \t");

                        if (elStart != std::string::npos)
                        {
                            elTrimmed = elTrimmed.substr (elStart);
                        }

                        // Strip comments
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

                        // Check first word
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

                    for (int ed = 0; ed < ifDepth; ed++)
                    {
                        expandedLines.push_back ("                ENDIF");
                    }

                    break;
                }

                // Skip local directive lines — already processed during collection
                std::string localCheck = exUpper;
                size_t lsp = localCheck.find_first_of (" \t");
                std::string localFirst = (lsp == std::string::npos) ? localCheck : localCheck.substr (0, lsp);

                if (localFirst == "LOCAL" || localFirst == ".LOCAL")
                {
                    continue;
                }

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
                        // Check whole-word boundary
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
                        // Check whole-word boundary
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

                // Strip AS65 single-quote forced substitution zones
                // (single quotes delimit areas where parameter substitution is forced,
                //  then the quotes themselves are removed)
                {
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
                }

                expandedLines.push_back (expanded);
            }

            // Insert expanded lines at the FRONT of the queue (reverse order)
            for (int bi = (int) expandedLines.size () - 1; bi >= 0; bi--)
            {
                PendingLine pl = {};
                pl.text = expandedLines[bi];
                pl.sourceLineNumber = current.sourceLineNumber;
                pl.macroDepth = current.macroDepth + 1;
                pendingLines.push_front (pl);
            }

            lineInfos.push_back (info);
            continue;
        }

        // ---- Colon-less label detection ----
        // If line starts at column 0, mnemonic isn't recognized, and isn't a macro,
        // re-interpret as a colon-less label
        if (info.parsed.startsAtColumn0 && info.parsed.label.empty () &&
            !m_opcodeTable.IsMnemonic (info.parsed.mnemonic) &&
            macros.find (info.parsed.mnemonic) == macros.end ())
        {
            // Record the colon-less label
            std::string labelName = info.parsed.mnemonic;
            std::string labelError;

            // Validate and record label — use original case from the raw line
            // The mnemonic was uppercased; get original from the raw text
            {
                std::string rawTrimmed = current.text;
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

                // Strip comment
                size_t sc = labelName.find (';');

                if (sc != std::string::npos)
                {
                    labelName = labelName.substr (0, sc);
                }
            }

            if (!Parser::ValidateLabel (labelName, m_opcodeTable, labelError))
            {
                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;
                error.message    = labelError;
                result.errors.push_back (error);
                result.success = false;
            }
            else if (symbols.count (labelName) > 0)
            {
                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;
                error.message    = "Duplicate label: " + labelName;
                result.errors.push_back (error);
                result.success = false;
            }
            else
            {
                symbols[labelName]     = pc;
                symbolKinds[labelName]  = SymbolKind::Label;
                exprSymbols[labelName] = (int32_t) pc;
            }

            info.parsed.label = labelName;

            // If there's remaining text (operand), push it as a new line
            if (!info.parsed.operand.empty ())
            {
                PendingLine pl = {};
                pl.text = "    " + info.parsed.operand;  // Indent to prevent re-triggering colon-less
                pl.sourceLineNumber = current.sourceLineNumber;
                pl.macroDepth = current.macroDepth;
                pendingLines.push_front (pl);
            }

            // Record this as a label-only line
            info.parsed.mnemonic.clear ();
            info.parsed.operand.clear ();
            info.isInstruction = false;
            lineInfos.push_back (info);
            continue;
        }

        // Classify operand (syntax only)
        info.classified    = Parser::ClassifyOperand (info.parsed.operand);
        info.isInstruction = true;

        // Try to evaluate expression in Pass 1 for addressing mode selection
        pass1Ctx.currentPC = (int32_t) pc;
        bool exprResolved = false;
        int32_t exprValue = 0;

        if (info.classified.syntax != OperandSyntax::None &&
            info.classified.syntax != OperandSyntax::Accumulator &&
            !info.classified.expression.empty ())
        {
            ExprResult er = ExpressionEvaluator::Evaluate (info.classified.expression, pass1Ctx);

            if (er.success)
            {
                exprResolved = true;
                exprValue    = er.value;
            }
            else if (!er.hasUnresolved)
            {
                // Syntax error — report immediately
                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;
                error.message    = "Expression error: " + er.error;
                result.errors.push_back (error);
                result.success = false;
                info.hasError  = true;
            }
            // else: hasUnresolved — normal forward ref, will resolve in Pass 2
        }

        info.valueResolved = exprResolved;
        info.resolvedValue = exprValue;

        // Determine addressing mode
        GlobalAddressingMode::AddressingMode mode = ResolveAddressingMode (
            info.classified.syntax, info.parsed.mnemonic,
            exprValue, exprResolved, m_opcodeTable);
        info.resolvedMode = mode;

        // Compute instruction size
        OpcodeEntry entry = {};

        if (m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
        {
            pc += 1 + entry.operandSize;
        }
        else
        {
            // Try fallback: ZP → Absolute
            GlobalAddressingMode::AddressingMode altMode = mode;

            if (altMode == GlobalAddressingMode::ZeroPage)
                altMode = GlobalAddressingMode::Absolute;
            else if (altMode == GlobalAddressingMode::ZeroPageX)
                altMode = GlobalAddressingMode::AbsoluteX;
            else if (altMode == GlobalAddressingMode::ZeroPageY)
                altMode = GlobalAddressingMode::AbsoluteY;

            if (altMode != mode && m_opcodeTable.Lookup (info.parsed.mnemonic, altMode, entry))
            {
                info.resolvedMode = altMode;
                pc += 1 + entry.operandSize;
            }
            else if (!info.hasError)
            {
                AssemblyError error = {};
                error.lineNumber = current.sourceLineNumber;

                if (!m_opcodeTable.IsMnemonic (info.parsed.mnemonic))
                {
                    error.message = "Invalid mnemonic: " + info.parsed.mnemonic;
                }
                else if (info.classified.syntax == OperandSyntax::None)
                {
                    error.message = "Missing operand for: " + info.parsed.mnemonic;
                }
                else
                {
                    error.message = "Invalid addressing mode for: " + info.parsed.mnemonic;
                }

                result.errors.push_back (error);
                result.success = false;
                info.hasError  = true;

                pc += EstimateInstructionSize (info.classified.syntax, info.parsed.mnemonic);
            }
        }

        lineInfos.push_back (info);
    }

    // Check for unclosed macro definition
    if (collectingMacro)
    {
        AssemblyError error = {};
        error.lineNumber = currentMacroLine;
        error.message    = "Unclosed macro definition: " + currentMacroName;
        result.errors.push_back (error);
        result.success = false;
    }

    // Check for unclosed if blocks
    if (!condStack.empty ())
    {
        AssemblyError error = {};
        error.lineNumber = (int) lines.size ();
        error.message    = "Unclosed if block (" + std::to_string (condStack.size ()) + " level(s) open)";
        result.errors.push_back (error);
        result.success = false;
    }

    // ---- Pass 2: Emit bytes with full symbol resolution ----
    std::vector<Byte>                         image (65536, m_options.fillByte);
    Word                                      lowestAddr  = 0xFFFF;
    Word                                      highestAddr = 0x0000;
    std::unordered_map<std::string, int>      referencedLabels;

    // Build full expression context with all symbols
    std::unordered_map<std::string, int32_t>  fullSymbols;

    for (const auto & sym : symbols)
    {
        fullSymbols[sym.first] = (int32_t) sym.second;
    }

    ExprContext pass2Ctx = { &fullSymbols, 0 };

    // Resolve deferred equ constants with fixpoint iteration (handles forward-reference chains)
    bool madeProgress = true;
    int  iterations   = 0;

    while (madeProgress && iterations < 100)
    {
        madeProgress = false;
        iterations++;

        for (const auto & info : lineInfos)
        {
            if (!info.isConstant || !info.parsed.isConstant)
            {
                continue;
            }

            if (info.parsed.constantKind != SymbolKind::Equ)
            {
                continue;
            }

            if (fullSymbols.find (info.parsed.constantName) != fullSymbols.end ())
            {
                continue;  // already resolved
            }

            const std::string & expr = info.parsed.constantExpr;

            // Check for string literal — value is the string length
            if (expr.size () >= 2 && expr.front () == '"' && expr.back () == '"')
            {
                int32_t len = (int32_t) (expr.size () - 2);
                symbols[info.parsed.constantName]     = (Word) len;
                fullSymbols[info.parsed.constantName] = len;
                madeProgress = true;
            }
            else
            {
                pass2Ctx.currentPC = (int32_t) info.pc;
                ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, pass2Ctx);

                if (er.success)
                {
                    symbols[info.parsed.constantName]     = (Word) er.value;
                    fullSymbols[info.parsed.constantName] = er.value;
                    madeProgress = true;
                }
            }
        }
    }

    // Report errors for any still-unresolved equs
    for (const auto & info : lineInfos)
    {
        if (!info.isConstant || !info.parsed.isConstant)
        {
            continue;
        }

        if (info.parsed.constantKind == SymbolKind::Equ && fullSymbols.find (info.parsed.constantName) == fullSymbols.end ())
        {
            AssemblyError error = {};
            error.lineNumber = info.parsed.lineNumber;
            error.message    = "Cannot resolve equ expression: " + info.parsed.constantExpr;
            result.errors.push_back (error);
            result.success = false;
        }
    }

    for (const auto & info : lineInfos)
    {
        Word   emitPCStart    = info.pc;
        Word   emitPC         = info.pc;
        bool   lineHasAddress = false;

        auto emitByte = [&] (Byte b)
        {
            image[emitPC] = b;

            if (emitPC < lowestAddr)  lowestAddr  = emitPC;
            if (emitPC > highestAddr) highestAddr = emitPC;

            emitPC++;
        };

        if (info.hasError)
        {
            // Nothing to emit for error lines
        }
        else if (info.isDirective)
        {
            lineHasAddress = true;
            pass2Ctx.currentPC = (int32_t) info.pc;

            // .ORG just moves emitPC — the image handles gaps automatically
            if (info.parsed.directive == ".ORG")
            {
                // Nothing to emit — PC already set via info.pc
            }
            else if (info.parsed.directive == ".BYTE")
            {
                // Emit bytes, applying character map to quoted strings
                auto args = Parser::SplitArgList (info.parsed.directiveArg);
                bool ok = true;

                for (const auto & arg : args)
                {
                    if (arg.size () >= 2 && arg.front () == '"' && arg.back () == '"')
                    {
                        std::string raw       = arg.substr (1, arg.size () - 2);
                        std::string processed = ProcessEscapeSequences (raw);

                        for (char c : processed)
                        {
                            emitByte (charMap.table[(unsigned char) c]);
                        }
                    }
                    else if (arg.size () >= 2 && arg.front () == '"')
                    {
                        // Handle string followed by escape sequence: "text"\n
                        size_t closeQuote = arg.find ('"', 1);

                        if (closeQuote != std::string::npos)
                        {
                            std::string raw       = arg.substr (1, closeQuote - 1);
                            std::string processed = ProcessEscapeSequences (raw);

                            for (char c : processed)
                            {
                                emitByte (charMap.table[(unsigned char) c]);
                            }

                            // Process trailing escape sequences
                            std::string suffix = arg.substr (closeQuote + 1);
                            std::string suffixProcessed = ProcessEscapeSequences (suffix);

                            for (char c : suffixProcessed)
                            {
                                emitByte ((Byte) (unsigned char) c);
                            }
                        }
                        else
                        {
                            ExprResult er = ExpressionEvaluator::Evaluate (arg, pass2Ctx);

                            if (!er.success)
                            {
                                AssemblyError error = {};
                                error.lineNumber = info.parsed.lineNumber;
                                error.message    = "Cannot evaluate expression: " + arg + " (" + er.error + ")";
                                result.errors.push_back (error);
                                ok = false;
                            }
                            else
                            {
                                emitByte ((Byte) (er.value & 0xFF));
                            }
                        }
                    }
                    else if (arg.size () >= 2 && arg[0] == '\\')
                    {
                        // Handle bare escape sequences: \n, \r, \t, \0
                        std::string processed = ProcessEscapeSequences (arg);

                        for (char c : processed)
                        {
                            emitByte ((Byte) (unsigned char) c);
                        }
                    }
                    else
                    {
                        ExprResult er = ExpressionEvaluator::Evaluate (arg, pass2Ctx);

                        if (!er.success)
                        {
                            AssemblyError error = {};
                            error.lineNumber = info.parsed.lineNumber;
                            error.message    = "Cannot evaluate expression: " + arg + " (" + er.error + ")";
                            result.errors.push_back (error);
                            ok = false;
                        }
                        else
                        {
                            emitByte ((Byte) (er.value & 0xFF));
                        }
                    }
                }

                if (!ok)
                {
                    result.success = false;
                }
            }
            else if (info.parsed.directive == ".WORD")
            {
                std::vector<int32_t> values;
                EvaluateDirectiveArgs (info.parsed.directiveArg, pass2Ctx, values, info.parsed.lineNumber, result.errors);

                if (values.size () != 0 || info.parsed.directiveArg.empty ())
                {
                    for (int32_t v : values)
                    {
                        emitByte ((Byte) (v & 0xFF));
                        emitByte ((Byte) ((v >> 8) & 0xFF));
                    }
                }
                else
                {
                    result.success = false;
                }
            }
            else if (info.parsed.directive == ".TEXT")
            {
                std::string text = Parser::ParseQuotedString (info.parsed.directiveArg);

                for (char c : text)
                {
                    emitByte (charMap.table[(unsigned char) c]);
                }
            }
            else if (info.parsed.directive == ".DD")
            {
                std::vector<int32_t> values;
                EvaluateDirectiveArgs (info.parsed.directiveArg, pass2Ctx, values, info.parsed.lineNumber, result.errors);

                if (values.size () != 0 || info.parsed.directiveArg.empty ())
                {
                    for (int32_t v : values)
                    {
                        emitByte ((Byte) (v & 0xFF));
                        emitByte ((Byte) ((v >> 8) & 0xFF));
                        emitByte ((Byte) ((v >> 16) & 0xFF));
                        emitByte ((Byte) ((v >> 24) & 0xFF));
                    }
                }
                else
                {
                    result.success = false;
                }
            }
            else if (info.parsed.directive == ".DS")
            {
                auto args = Parser::SplitArgList (info.parsed.directiveArg);

                if (!args.empty ())
                {
                    ExprResult sizeEr = ExpressionEvaluator::Evaluate (args[0], pass2Ctx);

                    if (sizeEr.success)
                    {
                        Byte fillVal = 0;

                        if (args.size () >= 2)
                        {
                            ExprResult fillEr = ExpressionEvaluator::Evaluate (args[1], pass2Ctx);

                            if (fillEr.success)
                            {
                                fillVal = (Byte) (fillEr.value & 0xFF);
                            }
                        }

                        for (int32_t j = 0; j < sizeEr.value; j++)
                        {
                            emitByte (fillVal);
                        }
                    }
                }
            }
            else if (info.parsed.directive == ".ALIGN")
            {
                int alignment = 2;

                if (!info.parsed.directiveArg.empty ())
                {
                    ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, pass2Ctx);

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
                        int padding = alignment - remainder2;
                        Byte fillVal = m_options.fillByte;

                        for (int j = 0; j < padding; j++)
                        {
                            emitByte (fillVal);
                        }
                    }
                }
            }
            else if (info.parsed.directive == ".MULTINOP")
            {
                ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, pass2Ctx);

                if (er.success && er.value > 0)
                {
                    for (int32_t j = 0; j < er.value; j++)
                    {
                        emitByte (0xEA);  // NOP opcode
                    }
                }
            }
        }
        else if (info.isInstruction)
        {
            lineHasAddress = true;
            pass2Ctx.currentPC = (int32_t) info.pc;

            GlobalAddressingMode::AddressingMode mode = info.resolvedMode;
            int32_t value = 0;
            bool    emit  = true;

            // Evaluate expression with full symbol table
            // But if the expression was already resolved in Pass 1 (e.g., Set
            // variables that change between expansions), use the Pass 1 value
            // to preserve temporal ordering of mutable symbols.
            if (info.valueResolved)
            {
                value = info.resolvedValue;

                // Still track symbol references for unused-label warnings
                if (!info.classified.expression.empty ())
                {
                    for (const auto & sym : symbols)
                    {
                        if (info.classified.expression.find (sym.first) != std::string::npos)
                        {
                            referencedLabels[sym.first] = info.parsed.lineNumber;
                        }
                    }
                }
            }
            else if (info.classified.syntax != OperandSyntax::None &&
                info.classified.syntax != OperandSyntax::Accumulator &&
                !info.classified.expression.empty ())
            {
                ExprResult er = ExpressionEvaluator::Evaluate (info.classified.expression, pass2Ctx);

                if (!er.success)
                {
                    AssemblyError error = {};
                    error.lineNumber = info.parsed.lineNumber;
                    error.message    = "Undefined symbol in: " + info.classified.expression;
                    result.errors.push_back (error);
                    result.success = false;
                    emit = false;

                    // Emit placeholder bytes
                    OpcodeEntry entry = {};

                    if (m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
                    {
                        emitByte (entry.opcode);

                        for (Byte j = 0; j < entry.operandSize; j++)
                        {
                            emitByte (0x00);
                        }
                    }
                }
                else
                {
                    value = er.value;

                    // Track symbol references for unused-label warnings
                    // (simple heuristic: if expression contains an identifier from symbols)
                    for (const auto & sym : symbols)
                    {
                        if (info.classified.expression.find (sym.first) != std::string::npos)
                        {
                            referencedLabels[sym.first] = info.parsed.lineNumber;
                        }
                    }
                }
            }

            if (emit)
            {
                // Handle relative branch offset computation
                if (mode == GlobalAddressingMode::Relative)
                {
                    Word pcAfterInstruction = info.pc + 2;
                    int  offset = value - (int) pcAfterInstruction;

                    if (offset < -128 || offset > 127)
                    {
                        AssemblyError error = {};
                        error.lineNumber = info.parsed.lineNumber;
                        error.message    = "Branch target out of range";
                        result.errors.push_back (error);
                        result.success = false;
                    }

                    value = offset & 0xFF;
                }

                // Immediate mode: truncate to 8 bits (AS65 behavior)
                {
                    OpcodeEntry entry = {};

                    if (!m_opcodeTable.Lookup (info.parsed.mnemonic, mode, entry))
                    {
                        AssemblyError error = {};
                        error.lineNumber = info.parsed.lineNumber;
                        error.message    = "Cannot encode: " + info.parsed.mnemonic;
                        result.errors.push_back (error);
                        result.success = false;
                    }
                    else
                    {
                        emitByte (entry.opcode);

                        if (entry.operandSize == 1)
                        {
                            emitByte ((Byte) (value & 0xFF));
                        }
                        else if (entry.operandSize == 2)
                        {
                            emitByte ((Byte) (value & 0xFF));
                            emitByte ((Byte) ((value >> 8) & 0xFF));
                        }
                    }
                }
            }
        }
        else if (!info.parsed.label.empty ())
        {
            lineHasAddress = true;
        }

        // Build listing entry
        if (m_options.generateListing)
        {
            // Skip listing-suppressed lines unless they are conditional skips
            if (!info.listingSuppressed || info.conditionalSkip)
            {
                AssemblyLine listLine = {};
                listLine.lineNumber = info.parsed.lineNumber;

                if (info.parsed.lineNumber >= 1 && info.parsed.lineNumber <= (int) lines.size ())
                {
                    listLine.sourceText = lines[info.parsed.lineNumber - 1];
                }

                listLine.hasAddress         = lineHasAddress;
                listLine.address            = info.pc;
                listLine.isMacroExpansion   = (info.macroDepth > 0);
                listLine.isConditionalSkip  = info.conditionalSkip;

                // Look up cycle count for instructions
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
                    listLine.bytes.push_back (image[j]);
                }

                result.listing.push_back (listLine);
            }
        }
    }

    // Extract the used portion of the image
    if (lowestAddr <= highestAddr)
    {
        result.bytes.assign (image.begin () + lowestAddr, image.begin () + highestAddr + 1);
        result.startAddress = lowestAddr;
        result.endAddress   = (Word) (highestAddr + 1);
    }
    else
    {
        result.bytes.clear ();
        result.endAddress = result.startAddress;
    }

    result.symbols     = symbols;
    result.symbolKinds = symbolKinds;

    // ---- Post-assembly: unused label warnings ----
    // Also track references in directive expressions
    for (const auto & info : lineInfos)
    {
        if (info.isDirective)
        {
            for (const auto & sym : symbols)
            {
                if (info.parsed.directiveArg.find (sym.first) != std::string::npos)
                {
                    referencedLabels[sym.first] = info.parsed.lineNumber;
                }
            }
        }
    }

    for (const auto & sym : symbols)
    {
        // Only warn about user-defined labels, not equ/set/built-in symbols
        auto kindIt = symbolKinds.find (sym.first);

        if (kindIt == symbolKinds.end () || kindIt->second != SymbolKind::Label)
        {
            continue;
        }

        if (referencedLabels.find (sym.first) == referencedLabels.end ())
        {
            int defLine = 0;

            for (const auto & info : lineInfos)
            {
                if (info.parsed.label == sym.first)
                {
                    defLine = info.parsed.lineNumber;
                    break;
                }
            }

            RecordWarning (result, defLine, "Unused label: " + sym.first);
        }
    }

    return result;
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
