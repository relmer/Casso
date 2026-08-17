#pragma once

#include "AssemblerTypes.h"
#include "ExpressionEvaluator.h"
#include "OpcodeTable.h"
#include "InstructionSetProvider.h"
#include "Directive.h"
#include "Parser.h"





class DialectProfile;
struct SubsetBoundaryRow;



enum class Segment { Code, Data, Bss };



struct LineInfo
{
    ParsedLine        parsed;
    ClassifiedOperand classified;
    Word              pc;

    // Where this line's bytes LAND, as against `pc`, which is what address they
    // will RUN at. The same number for every line of an as65 assembly and for
    // most of a Merlin one; they part company at a Merlin origin directive,
    // which relocates the program counter and leaves the output contiguous.
    //
    // Recorded per line rather than recomputed in pass 2 for the same reason
    // `usedExtendedSet` is: pass 2 walks this list, not the source, and would
    // have to replay every origin to arrive at the number pass 1 already knew.
    Word              outputPos;

    bool              isInstruction;
    bool              isDirective;
    bool              isConstant;
    bool              hasError;

    // Whether this line was sized and addressed but must produce no bytes at
    // all. A dummy section is the only thing that asks for it: labels inside one
    // bind to real addresses so the rest of the file can name them, while the
    // output cursor never moves, so anything emitted would land on top of the
    // bytes a previous line already placed.
    //
    // RECORDED per line for the reason `outputPos` is. Pass 2 walks this list
    // rather than the source, so it cannot see which section a line sat in
    // without being told, and re-deriving it would mean replaying every dummy
    // section that conditional assembly may have entered differently.
    bool              emitsNoBytes = false;

    GlobalAddressingMode::AddressingMode resolvedMode;
    int32_t                              resolvedValue;
    bool                                 valueResolved;

    int              macroDepth;
    bool             conditionalSkip;
    bool             listingSuppressed;

    // The file this line came from, carried across from the PendingLine so
    // pass 2 can attribute a diagnostic as accurately as pass 1. Empty means
    // the top-level input. Pass 2 walks this list rather than the pending
    // lines, so without it every pass-2 diagnostic would lose the file.
    std::string      sourceFile;

    // Which instruction set was active when pass 1 sized this line. RECORDED
    // rather than recomputed, because pass 2 must size the line exactly as
    // pass 1 did. Recomputing would only agree if the CPU-selection directive
    // were reached identically in both passes, and conditional assembly can
    // break that -- a directive inside a block whose taken-ness differs would
    // leave pass 2 emitting against a table pass 1 never used, surfacing as a
    // byte mismatch arbitrarily far from its cause.
    bool             usedExtendedSet = false;
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
//  One run of the assembler over one source text: two passes, and all the
//  state they share.
//
//  A SESSION rather than methods on Assembler, so every assembly starts from a
//  clean slate. Symbols, macros, the conditional stack, and the output image
//  are all per-run state, and holding them on a long-lived object is how a
//  second assembly ends up contaminated by the first.
//
//  Two passes because forward references demand it. Pass 1 walks the pending
//  lines to size and bind every instruction and label; pass 2 walks the
//  recorded line info to emit bytes, by which time every symbol is known.
//
//  The two walk DIFFERENT collections on purpose. Pass 1 consumes a deque that
//  macros and includes splice into at the front -- expansion happens by
//  requeuing lines, so the same pass handles them with no recursion -- while
//  pass 2 walks the flat line list pass 1 produced.
//
//  Directives live in ONE table spanning both passes, not two. The passes
//  previously kept independent lists of which directives exist and could
//  disagree about it; a single row per directive makes that impossible, and
//  the uniform handler shape per pass is what lets dispatch be an array
//  indexed by token rather than an if-chain.
//
////////////////////////////////////////////////////////////////////////////////

class AssemblySession
{
public:
    AssemblySession (const InstructionSetProvider & instructionSets, const AssemblerOptions & options);

    AssemblyResult Run (const std::string & sourceText);

private:
    HRESULT Initialize         (const std::string & sourceText);
    HRESULT RunPass1           ();
    HRESULT RunPass2           ();
    HRESULT DetectUnusedLabels ();

    HRESULT HandleStructCollection     (const PendingLine & current, LineInfo & info);
    HRESULT CollectMacroBody           (const PendingLine & current, LineInfo & info);
    HRESULT CollectLoopBody            (const PendingLine & current, LineInfo & info);
    HRESULT DetectMacroDefinition      (const PendingLine & current, LineInfo & info,
                                        const std::string & operandUpper, bool & handled);

    // Pushes one definition onto the collecting stack. Shared by the prelude,
    // which opens the first, and by the collector, which opens every further
    // one -- so the two cannot disagree about what opening a definition means.
    void    OpenMacroDefinition        (const PendingLine & current, const LineInfo & info,
                                        const std::string & operandUpper);
    HRESULT HandleConditionalDirective (const PendingLine & current, LineInfo & info, bool & handled);
    HRESULT HandleOrgDirective         (const PendingLine & current, LineInfo & info);
    HRESULT HandleKeyboardInput        (const PendingLine & current, LineInfo & info);
    HRESULT HandleSubsetBoundary       (const PendingLine & current, LineInfo & info, bool & outClaimed);
    HRESULT HandleSegmentSwitch        (LineInfo & info, bool & handled);
    HRESULT RecordLabel                (const PendingLine & current, LineInfo & info, Word address);
    HRESULT RecordRebindableLabel      (const PendingLine & current, LineInfo & info, Word address);
    HRESULT ApplyLocalLabelScope       (const PendingLine & current, LineInfo & info);
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
    HRESULT HandlePass1String  (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Hex     (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Dd      (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Ds      (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Align   (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1End     (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Error   (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1List    (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Nolist  (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Title   (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1Loop    (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1LoopEnd (const PendingLine & current, LineInfo & info);

    // The address-only section and its terminator, the in-source CPU selection,
    // and the directive naming the output. Declared apart from the run above
    // only because these names are wider than it.
    HRESULT HandlePass1DummySection    (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1DummySectionEnd (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1CpuSelect       (const PendingLine & current, LineInfo & info);
    HRESULT HandlePass1ObjectFile      (const PendingLine & current, LineInfo & info);

    // The directive that assigns the positional parameters with no macro call.
    // Both passes bind, through one helper, because a reference resolves against
    // the assignment most recently before it and pass 2 would otherwise read one
    // table built after pass 1 finished -- the same disagreement
    // RebindMutableConstant exists for.
    HRESULT HandlePass1ParameterBinding (const PendingLine & current, LineInfo & info);
    HRESULT RebindParameters            (const LineInfo & info, Word & emitPC);
    HRESULT BindPositionalParameters    (int lineNumber, const ParsedLine & parsed,
                                         ExprContext & ctx, bool isFinalPass);

    // Every collected line requeued as many times as the count asked for, and
    // the collecting state left behind. A separate step from the terminator's
    // handler because it is the whole of what the terminator MEANS, and a
    // reader looking for where a loop turns into lines should not have to read
    // a nesting check first.
    void    ExpandCollectedLoop();

    // Recognized, and deliberately does nothing in pass 1 (.OPT_NOOP is
    // accepted for as65 source compatibility; .PAGE acts at listing time).
    HRESULT IgnorePass1Directive (const PendingLine & current, LineInfo & info);

    HRESULT EmitTextDirective     (const LineInfo & info, Word & emitPC);
    HRESULT EmitStringDirective   (const LineInfo & info, Word & emitPC);
    HRESULT EmitHexDirective      (const LineInfo & info, Word & emitPC);
    HRESULT EmitErrorIfDirective  (const LineInfo & info, Word & emitPC);
    HRESULT EmitMultiNopDirective (const LineInfo & info, Word & emitPC);

    // The bytes one encoded-string directive produces. Shared by both passes so
    // the size pass 1 reserves and the bytes pass 2 writes cannot disagree --
    // deriving the size any other way is how a one-byte terminator convention
    // ends up shifting every label after it.
    static bool TryEncodeStringOperand (const ParsedLine & parsed, std::vector<Byte> & outBytes,
                                        std::string & outError);

    // A run of hexadecimal digit pairs, APPENDED to what the caller already
    // has. Refuses an odd digit count rather than padding it.
    static bool TryParseHexBytes       (const std::string & text, std::vector<Byte> & outBytes);

    // The bytes one raw-hexadecimal directive produces. Shared by both passes
    // for the same reason the string encoder is: a size derived any other way
    // can disagree with the bytes and move every label after it.
    static bool TryEncodeHexOperand    (const ParsedLine & parsed, std::vector<Byte> & outBytes,
                                        std::string & outError);

    // The separator joining a local label to the global label it belongs to.
    //
    // A period BECAUSE Parser::ValidateLabel rejects one in a label: no symbol a
    // source can spell may contain it, so a scoped name cannot collide with a
    // global however the two are spelled. The expression tokenizer does accept
    // it inside an identifier, which is what lets a scoped name be resolved from
    // an operand the ordinary way rather than through a second lookup path.
    static constexpr char  kLocalScopeSeparator = '.';

    // A local name joined to its scope, and one operand's local references
    // rewritten the same way, so a definition and a use cannot disagree about
    // what the name became. An empty scope leaves the text alone -- the caller
    // reports that, since a rewrite there would only hide it.
    static std::string  QualifyLocalName       (const std::string & scope, const std::string & name);
    static std::string  QualifyLocalReferences (const std::string & text, char prefix,
                                                const std::string & scope, bool & outSawLocal);
    HRESULT ExpandMacro                (const PendingLine & current, LineInfo & info, bool & handled);
    HRESULT SubstituteMacroParams      (const MacroDefinition & macroDef,
                                        const std::vector<std::string> & args,
                                        const std::string & uniqueSuffix,
                                        std::vector<std::string> & expandedLines);
    HRESULT HandleColonlessLabel       (const PendingLine & current, LineInfo & info, bool & handled);
    void    NormalizeBitOp             (const PendingLine & current, LineInfo & info);
    HRESULT ClassifyAndResolve         (const PendingLine & current, LineInfo & info);
    HRESULT ValidateAssemblyCompletion ();
    
    // Source order for the final diagnostic lists; recording order is not it.
    void     SortDiagnosticsByLine();

    HRESULT ResolveEquConstants   ();
    HRESULT ReportUnresolvedEqus  ();
    HRESULT RebindMutableConstant (const LineInfo & info);
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

        // Inside a repeat block, whose lines are collected and then requeued
        // once per iteration. The same shape a macro body uses, and for the
        // same reason: expansion is lines going back through pass 1, so the
        // pass needs no recursion and the expanded lines are read exactly as
        // written ones are.
        CollectingLoop,
    };

    enum class Pass1Prelude
    {
        None,               // nothing here claims the line; go on to Content
        MacroDefinition,    // "NAME macro [params]" opens a definition
        Conditional,        // .IF / .IFDEF / .IFNDEF / .ELSE / .ENDIF
        Skipped,            // inactive block, and not a conditional directive
        Org,                // .ORG
        SegmentSwitch,      // .CODE / .DATA / .BSS / .SEGMENT_*

        // KBD, whose label names a symbol rather than an address. It is claimed
        // here so the label stage never runs on it -- binding the name to the
        // program counter would leave the conditional that reads it testing
        // where the line sat instead of what the answer was.
        KeyboardInput,

        // A construct the active profile's boundary table refuses. Claimed in
        // the prelude so it never reaches content dispatch and fails as an
        // unknown directive -- "Merlin support is broken" is the wrong reading
        // of a construct that was recognized and deliberately declined.
        SubsetBoundary,
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
    static bool        IsMacroDefinitionStart (const ParsedLine & parsed, const std::string & operandUpper,
                                               const std::string & defKeyword);
    static bool        IsConditionalLine      (const ParsedLine & parsed);
    static bool        IsConditionalDirective (Directive token);
    static bool        IsSegmentDirective     (Directive token);

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

    // The same two bytes in the reverse order. Its own run because the name is
    // too long to sit in the column beside its neighbors.
    HRESULT EmitWordHighFirstDirective (const LineInfo & info, Word & emitPC);

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
    // One addressing mode an operand syntax may resolve to. `needsZeroPage`
    // marks the candidates that are only eligible once the operand is known to
    // fit in page zero -- which pass 1 often cannot know yet.
    struct ModeCandidate
    {
        GlobalAddressingMode::AddressingMode  mode;
        bool                                  needsZeroPage;
    };

    // The whole syntax -> mode policy, one row per OperandSyntax. Candidates
    // are tried in order and the first the mnemonic actually carries wins;
    // `fallback` is what the syntax means when it carries none of them, which
    // is also what an unencodable line resolves to before it is reported.
    struct AddressingRule
    {
        OperandSyntax                         syntax;
        std::span<const ModeCandidate>        candidates;
        GlobalAddressingMode::AddressingMode  fallback;
    };

    static std::span<const AddressingRule> GetAddressingRules();

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
    // The text inside a delimited operand, delimiters removed. Merlin takes any
    // character as the delimiter, so the pair is read off the operand itself.
    static std::string              StripDelimitedText        (const std::string & operand);
    static std::string              GetLeadingWord            (const std::string & text);
    static std::string              ToUpperCase               (const std::string & text);
    static std::string              StripCommentAndTrim       (const std::string & text);
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
    static bool                     TryEvaluateDirectiveArgs     (const std::string & argText,
                                                               const ExprContext & ctx,
                                                               std::vector<int32_t> & values,
                                                               int lineNumber,
                                                               std::vector<AssemblyError> & errors);
    GlobalAddressingMode::AddressingMode ResolveAddressingMode (OperandSyntax syntax,
                                                                 const std::string & mnemonic,
                                                                 int32_t value, bool resolved) const;

    void RecordError   (int lineNumber, const std::string & message);
    void RecordWarning (int lineNumber, const std::string & message);

    // The same, pointed at a field other than the line's own. A diagnostic
    // about the operand or about the label says so, rather than sending the
    // reader to the opcode -- see m_diagnosticColumn for why that is the
    // ambient answer and this is the exception.
    void RecordErrorAt (int lineNumber, int column, const std::string & message);

    // A refusal rather than a complaint about the source. Separate from
    // RecordError so the kind cannot be forgotten at a call site: a boundary
    // refusal recorded as an ordinary error is indistinguishable from "your
    // source is wrong", which is the one reading it exists to prevent.
    void RecordRefusal (int lineNumber, const std::string & message);

    // Which column a diagnostic about this line as a whole points at: the
    // opcode field, or the label where the line has no opcode. 0 where the
    // dialect recorded no columns at all.
    static int PrimaryColumn (const ParsedLine & parsed);

    // What to say about a word the active dialect does not recognize as an
    // operation. Three answers in order of how much they explain: the dialect's
    // own account of the mistake, the name of the dialect that DOES define the
    // word, and the plain report.
    std::string  DescribeUnknownOperation (const ParsedLine & parsed) const;

    // What to say about a directive spelling no dialect table resolved. Names
    // the spelling, and names the dialect that DOES define it where one does.
    std::string  DescribeUnknownDirective (const ParsedLine & parsed) const;

    // Whether the field after the opcode names something this assembler could
    // execute -- an instruction under the active dialect's spellings, or one of
    // its directives. Dialect-NEUTRAL: it consults the active profile's own
    // tables and the shared instruction set, and names no dialect.
    bool  OperandNamesAnOperation (const ParsedLine & parsed) const;

    // The highest positional parameter a macro body refers to, or 0 for a body
    // that refers to none. An invocation supplying fewer arguments than this has
    // nothing to put in the gap.
    static int  HighestParameterReferenced (const MacroDefinition & macroDef, char sigil);

    // One construct the boundary refused, held until the pass ends.
    //
    // DEFERRED, and not for tidiness. The advice a relocatable module gets
    // depends on whether ANY line of it declares an external symbol, which is
    // not known until the last line has been read -- so composing the message
    // where the construct was met would have to guess, and guessing wrong
    // offers a fix that cannot work. The file is captured here for the reason
    // every deferred diagnostic captures one; see m_currentSourceFile.
    struct BoundaryOffense
    {
        const SubsetBoundaryRow *  row;
        int                        lineNumber;
        int                        column;
        std::string                file;
    };

    // Every refusal, composed and recorded once the whole source has been seen.
    void ReportSubsetBoundaryRefusals();

    // The file whose line is being processed right now. Stamped onto every
    // diagnostic AT THE POINT IT IS RECORDED, which is the only moment the
    // originating file is still known -- by the time a diagnostic is reported,
    // the only path in hand is the top-level input, which is exactly how errors
    // inside included files came to be misattributed.
    //
    // THE BOUNDARY OF THIS DESIGN, which matters more than the design:
    //
    //   * Correct for a diagnostic raised AT the line being processed. That is
    //     the overwhelming majority, and it is why this is ambient state rather
    //     than an argument threaded through every call site.
    //
    //   * WRONG for a diagnostic about a construct that opened somewhere else.
    //     By the time an unclosed IF or an unterminated macro is reported, this
    //     holds whatever was processed last. Every DEFERRED carrier must capture
    //     its own file when the construct opens -- ConditionalState::openFile
    //     and MacroDefinition::sourceFile exist for exactly that -- and restore
    //     it here before recording.
    //
    // A new deferred diagnostic needs a new captured file. Reaching for ambient
    // state there produces the right line and the wrong file, which reads as a
    // correct diagnostic and is the harder bug to see.
    std::string  m_currentSourceFile;

    // The column of the line being processed right now, travelling with the
    // file above and stamped by the same three recorders. Every caveat on
    // m_currentSourceFile applies unchanged, including the deferred one: a
    // construct that opened elsewhere must carry its own column, or it is
    // reported at the right line and the wrong place on it.
    //
    // It answers for the LINE, not for the message -- the opcode field, or the
    // label where there is no opcode. A diagnostic whose subject is a different
    // field says so through RecordErrorAt rather than by moving this, because
    // ambient state left pointing somewhere by one call site and read by the
    // next is how a plausible wrong column outlives an obviously absent one.
    //
    // 0 for a dialect that records no columns, which is every dialect that
    // predates the profile seam. That is what keeps their diagnostics the
    // shape they have always had.
    int          m_diagnosticColumn = 0;

    bool IsAssembling  () const;
    void InjectBuiltin (const std::string & name, int32_t value);
    void EmitByte      (Byte b, Word & emitPC);

    // Reserves `count` bytes in pass 1: the program counter and the output
    // cursor both move, because every directive that occupies space moves where
    // the next byte lands AND what address it will run at, by the same amount.
    // An origin directive is the only thing that can move one without the other,
    // which is why this is the single place the two advance together.
    void ReserveBytes  (Word count);

    // A POINTER rather than a reference, because an in-source CPU directive
    // re-seats it mid-assembly. Only ever changed at a line boundary, never
    // within one, so both passes agree about which table a given line used.
    const InstructionSetProvider  & m_instructionSets;
    const OpcodeTable             * m_opcodeTable;
    bool                            m_extendedActive = false;

    const AssemblerOptions & m_options;

    // The grammar every line is read with, and the source of the origin an
    // assembly starts at when nothing in the source names one. Resolved once
    // from the options rather than looked up per line: profiles are stateless
    // and shared, so the reference is stable for the whole run.
    const DialectProfile   & m_dialect;

    AssemblyResult           m_result             = {};

    std::vector<std::string>                           m_lines;
    std::vector<LineInfo>                              m_lineInfos;
    std::unordered_map<std::string, Word>              m_symbols;
    std::unordered_map<std::string, SymbolKind>        m_symbolKinds;
    std::unordered_map<std::string, int32_t>           m_exprSymbols;
    ExprContext                                        m_pass1Ctx           = { &m_exprSymbols, 0 };
    Word                                               m_pc                 = 0;

    // Where the next byte LANDS, which m_pc says nothing about once a dialect
    // whose origin only relocates has moved the program counter. Seeded from
    // the same default origin, advanced by exactly the same amounts, and
    // repositioned by an origin directive only where the dialect says so.
    Word                                               m_outputPos          = 0;

    // Whether anything has been reserved yet. The FIRST origin directive
    // establishes where output begins whatever the dialect, so it moves the
    // cursor too; after that, moving it is the seeking dialect's behavior alone
    // and doing it for a relocating one would strand the bytes already placed.
    bool                                               m_outputStarted      = false;

    bool                                               m_originSet          = false;
    bool                                               m_endAssembly        = false;
    Segment                                            m_currentSegment     = Segment::Code;
    Word                                               m_segmentPC[3]       = { 0, 0, 0 };

    // The output cursor each segment left off at, saved and restored beside its
    // program counter. Kept as a pair rather than derived from m_segmentPC,
    // which would only be right for a dialect whose origin seeks.
    //
    // NO TEST DISCRIMINATES THIS TODAY, and that is recorded rather than left
    // to be rediscovered: segment directives are as65-only tokens, and as65's
    // two cursors never part, so this array and m_segmentPC hold the same
    // values on every path anything can reach. It stays because it is what
    // keeps the property true by construction for the first relocating dialect
    // that gains segments -- verified by mutation, which nothing caught.
    Word                                               m_segmentOutputPos[3] = { 0, 0, 0 };
    // The global label every local label since it belongs to. Empty until the
    // first global one, which is why a local before it is an error rather than
    // a symbol in an unnamed scope.
    std::string                                        m_localLabelScope;

    std::vector<ConditionalState>                      m_condStack;
    std::unordered_map<std::string, MacroDefinition>   m_macros;
    Pass1State                                         m_pass1State         = Pass1State::Normal;

    // The definitions being collected, outermost first. A STACK rather than one
    // set of fields, because a definition opened while another is already open
    // does not nest inside it -- it OVERLAPS it. From that point every line
    // belongs to both bodies, and the first terminator closes both.
    //
    // That is what a source relying on the shorthand needs: one definition
    // written as a prefix of the next, sharing its tail and its terminator, so
    // the pair costs one copy of the common lines instead of two. Collecting
    // into a single body instead swallows the second opening line as body text,
    // and the definition it should have started is never made -- the source is
    // then read as one definition that never ends.
    //
    // Non-empty exactly while m_pass1State is CollectingMacro.
    std::vector<MacroDefinition>                       m_pendingMacros;
    int                                                m_macroUniqueCounter = 0;

    // The repeat block being collected. The body is kept as PENDING LINES
    // rather than as text, because every copy has to carry the line number and
    // the file its original was written in -- a diagnostic inside the third
    // iteration still belongs to the line the author wrote once.
    std::vector<PendingLine>                           m_loopBody;
    int                                                m_loopCount          = 0;

    // How many repeat blocks were opened INSIDE the one being collected. A
    // terminator only closes the outer block at zero; deeper ones are body text
    // and are expanded again, freshly, in every copy of it.
    int                                                m_loopNesting        = 0;
    int                                                m_loopLine           = 0;
    int                                                m_loopColumn         = 0;
    std::string                                        m_loopFile;

    // The spelling the source opened the block with, so an unterminated one is
    // named in the dialect's own words rather than in a canonical spelling the
    // author never wrote.
    std::string                                        m_loopDirective;

    // Whether lines are currently being ADDRESSED WITHOUT BEING PLACED. Inside
    // a dummy section the program counter advances so labels describe a real
    // memory layout, and the output cursor does not, so nothing the section
    // contains occupies a byte of the object.
    bool                                               m_inDummySection     = false;

    // Where the section was entered from, restored when it closes. Both cursors
    // are saved, not just the program counter: the output one never moved, and
    // restoring it from the program counter would be right only for a dialect
    // whose two cursors cannot part in the first place.
    Word                                               m_dummyReturnPC      = 0;
    Word                                               m_dummyReturnOutput  = 0;
    int                                                m_dummyLine          = 0;
    int                                                m_dummyColumn        = 0;
    std::string                                        m_dummyFile;
    std::string                                        m_dummyDirective;
    int                                                m_listingLevel;
    std::unordered_map<std::string, StructDefinition>  m_structs;
    StructDefinition                                   m_currentStruct      = {};
    CharacterMap                                       m_charMap;
    std::deque<PendingLine>                            m_pendingLines;
    std::vector<Byte>                                  m_image;
    Word                                               m_lowestAddr         = 0xFFFF;
    Word                                               m_highestAddr        = 0x0000;
    std::unordered_map<std::string, int>               m_referencedLabels;
    std::unordered_map<std::string, int32_t>           m_fullSymbols;
    ExprContext                                        m_pass2Ctx           = { &m_fullSymbols, 0 };

    // Every construct the boundary refused, in the order they were met, and how
    // many times each one has been reached. The count is what separates a
    // construct accepted once from the same construct refused twice; without it
    // the first occurrence of a cumulative directive would be refused too.
    std::vector<BoundaryOffense>                       m_boundaryOffenses;
    std::unordered_map<int, int>                       m_boundaryOccurrences;

    static const int kMaxMacroDepth   = 15;
    static const int kMaxIncludeDepth = 16;

    // How many times one repeat block may be expanded. CASSO's guard on the
    // pending queue rather than a claim about the dialect: expansion is lines
    // pushed onto a deque, so a count arriving as a mistyped expression would
    // otherwise be answered with memory rather than with a diagnostic. A block
    // repeated more times than a 16-bit address space has bytes cannot be what
    // the source meant, which is what makes this a safe place to draw the line.
    static const int kMaxLoopIterations = 32768;

    // How many positional macro parameters a sigil form can name. ONE digit is
    // the whole form, so this is the highest single digit rather than a limit
    // anyone chose -- `]12` is `]1` followed by a literal 2, not a twelfth
    // parameter. The vendor macro library uses 1 through 3.
    static const int kMaxPositionalParams = 9;
};

