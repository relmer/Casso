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
    };

    std::vector<LineInfo>                              lineInfos;
    std::unordered_map<std::string, Word>              symbols;
    std::unordered_map<std::string, SymbolKind>        symbolKinds;
    Word                                               pc        = result.startAddress;
    bool                                               originSet = false;
    bool                                               endAssembly = false;

    // Build a Pass 1 expression context (symbols populated as we go)
    std::unordered_map<std::string, int32_t>  exprSymbols;
    ExprContext                                pass1Ctx = { &exprSymbols, 0 };

    // Conditional assembly stack
    std::vector<ConditionalState> condStack;

    // Macro definitions and expansion state
    std::unordered_map<std::string, MacroDefinition> macros;
    bool         collectingMacro = false;
    std::string  currentMacroName;
    int          currentMacroLine = 0;
    std::vector<std::string> currentMacroBody;
    int          macroUniqueCounter = 0;
    static const int kMaxMacroDepth = 15;

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
                def.lineNumber = currentMacroLine;
                macros[currentMacroName] = def;
                collectingMacro = false;
            }
            else
            {
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

            // Parse parameter names (after "macro" keyword)
            std::string paramStr;

            if (operandUpper.size () > 5)
            {
                paramStr = info.parsed.operand.substr (6);
            }

            // TODO: parse named params for Phase 9

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

            if (dir == ".IF")       { condDirective = "IF";    condArg = info.parsed.directiveArg; }
            else if (dir == ".ELSE")  { condDirective = "ELSE"; }
            else if (dir == ".ENDIF") { condDirective = "ENDIF"; }
        }
        else if (!info.parsed.mnemonic.empty ())
        {
            if (info.parsed.mnemonic == "IF")        { condDirective = "IF";    condArg = info.parsed.operand; }
            else if (info.parsed.mnemonic == "ELSE")  { condDirective = "ELSE"; }
            else if (info.parsed.mnemonic == "ENDIF") { condDirective = "ENDIF"; }
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

                if (originSet && newAddr < pc)
                {
                    AssemblyError error = {};
                    error.lineNumber = current.sourceLineNumber;
                    error.message    = ".org address is backward from current position";
                    result.errors.push_back (error);
                    result.success = false;
                }
                else
                {
                    if (newAddr == pc)
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
            else if (info.parsed.directive == ".SEGMENT_NOOP")
            {
                // Recognized but intentionally no-op
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
            char uniqueBuf[8];
            snprintf (uniqueBuf, sizeof (uniqueBuf), "%04d", macroUniqueCounter);
            std::string uniqueSuffix (uniqueBuf);

            // Expand macro body with argument substitution
            const auto & body = macroIt->second.body;

            // Insert expanded lines at the FRONT of the queue (so they process next)
            for (int bi = (int) body.size () - 1; bi >= 0; bi--)
            {
                std::string expanded = body[bi];

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

                // Replace \? with unique suffix
                {
                    size_t pos = 0;

                    while ((pos = expanded.find ("\\?", pos)) != std::string::npos)
                    {
                        expanded.replace (pos, 2, uniqueSuffix);
                        pos += uniqueSuffix.size ();
                    }
                }

                PendingLine pl = {};
                pl.text = expanded;
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
            else if (info.parsed.directive == ".DD")
            {
                std::vector<int32_t> values;
                EvaluateDirectiveArgs (info.parsed.directiveArg, pass2Ctx, values, info.parsed.lineNumber, result.errors);

                if (values.size () != 0 || info.parsed.directiveArg.empty ())
                {
                    for (int32_t v : values)
                    {
                        output.push_back ((Byte) (v & 0xFF));
                        output.push_back ((Byte) ((v >> 8) & 0xFF));
                        output.push_back ((Byte) ((v >> 16) & 0xFF));
                        output.push_back ((Byte) ((v >> 24) & 0xFF));
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
                            output.push_back (fillVal);
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
                            output.push_back (fillVal);
                        }
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
