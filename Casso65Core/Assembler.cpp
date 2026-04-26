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
            std::string str = arg.substr (1, arg.size () - 2);

            for (char c : str)
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
    };

    std::vector<LineInfo>                              lineInfos;
    std::unordered_map<std::string, Word>              symbols;
    std::unordered_map<std::string, SymbolKind>        symbolKinds;
    Word                                               pc        = result.startAddress;
    bool                                               originSet = false;

    // Build a Pass 1 expression context (symbols populated as we go)
    std::unordered_map<std::string, int32_t>  exprSymbols;
    ExprContext                                pass1Ctx = { &exprSymbols, 0 };

    // ---- Pass 1: Parse, collect labels, compute PC ----
    for (int i = 0; i < (int) lines.size (); i++)
    {
        LineInfo info     = {};
        info.parsed       = Parser::ParseLine (lines[i], i + 1);
        info.pc           = pc;
        info.isInstruction = false;
        info.isDirective   = false;
        info.isConstant    = false;
        info.hasError      = false;
        info.valueResolved = false;
        info.resolvedValue = 0;
        info.resolvedMode  = GlobalAddressingMode::SingleByteNoOperand;

        // Handle .org BEFORE recording label
        if (info.parsed.isDirective && info.parsed.directive == ".ORG")
        {
            pass1Ctx.currentPC = (int32_t) pc;
            ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.directiveArg, pass1Ctx);

            if (!er.success)
            {
                AssemblyError error = {};
                error.lineNumber = i + 1;
                error.message    = ".org expression must be resolvable: " + er.error;
                result.errors.push_back (error);
                result.success = false;
            }
            else
            {
                Word newAddr = (Word) er.value;

                if (originSet && newAddr < pc)
                {
                    AssemblyError error = {};
                    error.lineNumber = i + 1;
                    error.message    = ".org address is backward from current position";
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    if (newAddr == pc)
                    {
                        RecordWarning (result, i + 1, "Redundant .org to current address");
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

        // Record label
        if (!info.parsed.label.empty ())
        {
            std::string labelError;

            if (!Parser::ValidateLabel (info.parsed.label, m_opcodeTable, labelError))
            {
                AssemblyError error = {};
                error.lineNumber = i + 1;
                error.message    = labelError;
                result.errors.push_back (error);
                result.success = false;
            }
            else if (symbols.count (info.parsed.label) > 0)
            {
                AssemblyError error = {};
                error.lineNumber = i + 1;
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
                    RecordWarning (result, i + 1, "Label name resembles mnemonic: " + info.parsed.label);
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
                    error.lineNumber = i + 1;
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
                        error.lineNumber = i + 1;
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
                    error.lineNumber = i + 1;

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

                    // Try to evaluate in Pass 1 (may succeed if no forward refs)
                    ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, pass1Ctx);

                    if (er.success)
                    {
                        symbols[info.parsed.constantName]     = (Word) er.value;
                        exprSymbols[info.parsed.constantName] = er.value;
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
                EvaluateDirectiveArgs (info.parsed.directiveArg, pass1Ctx, values, i + 1, tempErrors);

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

            lineInfos.push_back (info);
            continue;
        }

        // Skip empty lines
        if (info.parsed.mnemonic.empty ())
        {
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
                error.lineNumber = i + 1;
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
                error.lineNumber = i + 1;

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

    // ---- Pass 2: Emit bytes with full symbol resolution ----
    std::vector<Byte>                         output;
    std::unordered_map<std::string, int>      referencedLabels;

    // Build full expression context with all symbols
    std::unordered_map<std::string, int32_t>  fullSymbols;

    for (const auto & sym : symbols)
    {
        fullSymbols[sym.first] = (int32_t) sym.second;
    }

    ExprContext pass2Ctx = { &fullSymbols, 0 };

    // Resolve deferred equ constants (can forward-reference labels)
    for (const auto & info : lineInfos)
    {
        if (!info.isConstant || !info.parsed.isConstant)
        {
            continue;
        }

        if (info.parsed.constantKind == SymbolKind::Equ && fullSymbols.find (info.parsed.constantName) == fullSymbols.end ())
        {
            pass2Ctx.currentPC = (int32_t) info.pc;
            ExprResult er = ExpressionEvaluator::Evaluate (info.parsed.constantExpr, pass2Ctx);

            if (!er.success)
            {
                AssemblyError error = {};
                error.lineNumber = info.parsed.lineNumber;
                error.message    = "Cannot resolve equ expression: " + info.parsed.constantExpr;
                result.errors.push_back (error);
                result.success = false;
            }
            else
            {
                symbols[info.parsed.constantName]     = (Word) er.value;
                fullSymbols[info.parsed.constantName] = er.value;
            }
        }
    }

    for (const auto & info : lineInfos)
    {
        size_t bytesStart     = output.size ();
        bool   lineHasAddress = false;

        if (info.hasError)
        {
            // Nothing to emit for error lines
        }
        else if (info.isDirective)
        {
            lineHasAddress = true;
            pass2Ctx.currentPC = (int32_t) info.pc;

            if (info.parsed.directive == ".BYTE")
            {
                std::vector<int32_t> values;
                EvaluateDirectiveArgs (info.parsed.directiveArg, pass2Ctx, values, info.parsed.lineNumber, result.errors);

                if (values.size () != 0 || info.parsed.directiveArg.empty ())
                {
                    for (int32_t v : values)
                    {
                        output.push_back ((Byte) (v & 0xFF));
                    }
                }
                else
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
                        output.push_back ((Byte) (v & 0xFF));
                        output.push_back ((Byte) ((v >> 8) & 0xFF));
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
                    output.push_back ((Byte) c);
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
            if (info.classified.syntax != OperandSyntax::None &&
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
                        output.push_back (entry.opcode);

                        for (Byte j = 0; j < entry.operandSize; j++)
                        {
                            output.push_back (0x00);
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

                // Range check for immediate mode
                if (mode == GlobalAddressingMode::Immediate && (value < -128 || value > 255))
                {
                    AssemblyError error = {};
                    error.lineNumber = info.parsed.lineNumber;
                    error.message    = "Operand value out of range for immediate mode: " + std::to_string (value);
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
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
                        output.push_back (entry.opcode);

                        if (entry.operandSize == 1)
                        {
                            output.push_back ((Byte) (value & 0xFF));
                        }
                        else if (entry.operandSize == 2)
                        {
                            output.push_back ((Byte) (value & 0xFF));
                            output.push_back ((Byte) ((value >> 8) & 0xFF));
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
            AssemblyLine listLine = {};
            listLine.lineNumber = info.parsed.lineNumber;
            listLine.sourceText = lines[info.parsed.lineNumber - 1];
            listLine.hasAddress = lineHasAddress;
            listLine.address    = info.pc;

            for (size_t j = bytesStart; j < output.size (); j++)
            {
                listLine.bytes.push_back (output[j]);
            }

            result.listing.push_back (listLine);
        }
    }

    result.bytes      = output;
    result.symbols    = symbols;
    result.endAddress = result.startAddress + (Word) output.size ();

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

std::string Assembler::FormatListingLine (const AssemblyLine & line)
{
    char addrBuf[8] = {};

    // Address column (5 chars)
    if (line.hasAddress)
    {
        snprintf (addrBuf, sizeof (addrBuf), "$%04X", line.address);
    }
    else
    {
        snprintf (addrBuf, sizeof (addrBuf), "     ");
    }

    // Bytes column (up to 3 hex bytes, padded to 10 chars)
    std::string bytesStr;

    for (size_t i = 0; i < line.bytes.size () && i < 3; i++)
    {
        char hexBuf[4];

        if (i > 0)
        {
            bytesStr += " ";
        }

        snprintf (hexBuf, sizeof (hexBuf), "%02X", line.bytes[i]);
        bytesStr += hexBuf;
    }

    while (bytesStr.size () < 10)
    {
        bytesStr += " ";
    }

    return std::string (addrBuf) + "  " + bytesStr + line.sourceText;
}
