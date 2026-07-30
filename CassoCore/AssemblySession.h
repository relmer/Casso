#pragma once

#include "AssemblerTypes.h"
#include "ExpressionEvaluator.h"
#include "OpcodeTable.h"
#include "Directive.h"
#include "Parser.h"





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
    HRESULT HandleCmapDirective        (const PendingLine & current, LineInfo & info);

    // Each pass has one handler shape, which is what lets dispatch be an
    // array indexed by token rather than a chain.
    using Pass1DirectiveFn = HRESULT (AssemblySession::*) (const PendingLine & current, LineInfo & info);
    using Pass2DirectiveFn = HRESULT (AssemblySession::*) (const LineInfo & info, Word & emitPC);

    // One row per directive, spanning both passes. Deliberately not two
    // tables: pass 1 and pass 2 previously kept independent lists of which
    // directives exist, and could disagree about it.
    struct DirectiveRow
    {
        Directive         token;   // must equal its own index
        Pass1DirectiveFn  pass1;   // nullptr = nothing to do in pass 1
        Pass2DirectiveFn  pass2;   // nullptr = emits no bytes
    };

    // The table lives inside a function rather than at file scope because a
    // file-scope initializer cannot name private members.
    static const DirectiveRow * GetDirectiveRows();

    HRESULT HandlePass1Word    (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Text    (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Dd      (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Ds      (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Align   (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1End     (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Error   (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1List    (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Nolist  (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Title   (const PendingLine & current, LineInfo & info);

    // Recognized, and deliberately does nothing in pass 1 (.OPT_NOOP is
    // accepted for as65 source compatibility; .PAGE acts at listing time).
    HRESULT IgnorePass1Directive (const PendingLine & current, LineInfo & info);

    HRESULT EmitTextDirective     (const LineInfo & info, Word & emitPC);
    HRESULT EmitMultiNopDirective (const LineInfo & info, Word & emitPC);
    HRESULT ExpandMacro                (const PendingLine & current, LineInfo & info, bool & handled);
    HRESULT SubstituteMacroParams      (const MacroDefinition & macroDef,
                                        const std::vector<std::string> & args,
                                        const std::string & uniqueSuffix,
                                        std::vector<std::string> & expandedLines);
    HRESULT HandleColonlessLabel       (const PendingLine & current, LineInfo & info, bool & handled);
    void    NormalizeBitOp             (const PendingLine & current, LineInfo & info);
    HRESULT ClassifyAndResolve         (const PendingLine & current, LineInfo & info);
    HRESULT ValidateAssemblyCompletion ();

    HRESULT ResolveEquConstants  ();
    HRESULT ReportUnresolvedEqus ();
    HRESULT EmitDirectiveBytes   (const LineInfo & info, Word & emitPC);
    HRESULT EmitInstruction      (const LineInfo & info, Word & emitPC);
    HRESULT BuildListingEntry    (const LineInfo & info, Word emitPCStart, Word emitPC, bool lineHasAddress);
    HRESULT ExtractImage         ();


    HRESULT ProcessPass1Line           (const PendingLine & current);

    // Pass 1 runs in three phases, in this order, and the order is the
    // language. Each phase is a switch; RunPass1Stages sequences them.
    //
    //   1. Collecting  -- a .STRUCT or macro body swallows the line whole.
    //   2. Prelude     -- directives that decide whether the line assembles
    //                     at all, or that move the PC.
    //   3. Content     -- the line's actual payload, at the settled PC.
    //
    // RecordLabel sits between 2 and 3 because that is exactly when the
    // address a label binds to stops moving.

    // The one mode the parse is in. Replaces a pair of independent bools that
    // between them could represent "collecting a struct AND a macro", which is
    // not a thing.
    enum class Pass1State
    {
        Normal,
        CollectingStruct,   // inside .STRUCT ... .ENDSTRUCT
        CollectingMacro,    // inside NAME macro ... .ENDM
    };

    enum class Pass1Prelude
    {
        None,               // nothing here claims the line; go on to Content
        MacroDefinition,    // "NAME macro [params]" opens a definition
        Conditional,        // .IF / .IFDEF / .IFNDEF / .ELSE / .ENDIF
        Skipped,            // inactive block, and not a conditional directive
        Org,                // .ORG
        SegmentSwitch,      // .CODE / .DATA / .BSS / .SEGMENT_*
    };

    enum class Pass1Content
    {
        ConstantDefinition, // NAME = expr, NAME .EQU expr
        Directive,          // every directive not already claimed
        Empty,              // label-only or blank
        Instruction,        // a mnemonic -- possibly a multi-NOP, a macro call
                            // or a colon-less label; only trying can tell
    };

    // Both pure: read the line and the mode, change nothing.
    Pass1Prelude ClassifyPrelude (const LineInfo & info, const std::string & operandUpper) const;
    static Pass1Content ClassifyContent (const LineInfo & info);

    // Shared by each classifier and the handler it routes to, so the two
    // cannot drift apart about what a line is.
    static bool        IsMacroDefinitionStart (const ParsedLine & parsed, const std::string & operandUpper);
    static bool        IsConditionalLine      (const ParsedLine & parsed);
    static bool        IsConditionalDirective (Directive token);
    static bool        IsSegmentDirective     (Directive token);
    static std::string GetUpperOperand           (const std::string & operand);

    HRESULT RunPass1Stages         (const PendingLine & current, LineInfo & info);
    HRESULT RunCollectingState     (const PendingLine & current, LineInfo & info, bool & outClaimed);
    HRESULT RunPreludeDirectives   (const PendingLine & current, LineInfo & info, bool & outClaimed);
    HRESULT RunContentStages       (const PendingLine & current, LineInfo & info);

    // The Instruction tail: stages that can only decide by trying.
    HRESULT ResolveInstructionLine (const PendingLine & current, LineInfo & info);
    HRESULT HandleMultiNop             (const PendingLine & current, LineInfo & info, bool & handled);
    HRESULT CountExitmIfDepth          (const std::vector<std::string> & expandedLines, int & ifDepth);

    // Phase 5 sub-helpers
    HRESULT CheckEndStruct            (const PendingLine & current, LineInfo & info, bool & isEnd);
    HRESULT ParseStructMember         (const PendingLine & current, LineInfo & info);
    HRESULT GetStructMemberSize       (const std::string & operand, int32_t & outSize);
    HRESULT RecordStructMember        (const std::string & name, int32_t size);
    HRESULT HandleIfDirective         (const PendingLine & current, const std::string & condArg);
    HRESULT HandleIfdefDirective      (const PendingLine & current, Directive token,
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
    // How wide one element of a struct member is, keyed by the directive that
    // declared it. `elementSize` is kSizeFromOperand when the operand supplies
    // the count instead (`.DS`), and a member type absent from the table is not
    // a storage directive at all.
    struct StructMemberType
    {
        Directive  token;
        int32_t    elementSize;
    };

    static constexpr int32_t        kSizeFromOperand = -1;

    static std::span<const StructMemberType> GetStructMemberTypes();
    static std::string              GetLeadingWord            (const std::string & text);
    static std::string              ToUpperCase               (const std::string & text);
    static std::string              GetLowerExtension         (const std::string & filename);
    static int                      HexCharToNibble           (char c);
    static int                      HexByte                   (const std::string & s, size_t offset);
    static std::vector<Byte>        ParseSRecord              (const std::string & content);
    static std::vector<Byte>        ParseIntelHex             (const std::string & content);
    static std::vector<std::string> GenerateByteDirectives    (const std::vector<Byte> & data);
    bool                            IsBranchMnemonic          (const std::string & mnemonic) const;
    static bool                     IsBitOpMnemonic           (const std::string & mnemonic);
    Byte                            EstimateErrorRecoverySize (OperandSyntax syntax, const std::string & mnemonic) const;
    static std::string              ProcessEscapeSequences    (const std::string & str);
    static bool                     EvaluateDirectiveArgs     (const std::string & argText,
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
    Pass1State                                          m_pass1State         = Pass1State::Normal;
    std::string                                         m_currentMacroName;
    int                                                 m_currentMacroLine   = 0;
    std::vector<std::string>                            m_currentMacroBody;
    std::vector<std::string>                            m_currentMacroParams;
    std::vector<std::string>                            m_currentMacroLocals;
    int                                                 m_macroUniqueCounter = 0;
    int                                                 m_listingLevel;
    std::unordered_map<std::string, StructDefinition>   m_structs;
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

