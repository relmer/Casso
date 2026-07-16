#pragma once

#include "AssemblerTypes.h"
#include "Ehm.h"
#include "ExpressionEvaluator.h"
#include "OpcodeTable.h"
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
    HRESULT HandleCmapDirective        (LineInfo & info);
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
    static bool                     IsBitOpMnemonic         (const std::string & mnemonic);
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

