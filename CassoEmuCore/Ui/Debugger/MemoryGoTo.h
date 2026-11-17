#pragma once

class DebugSession;
class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryGoTo
//
//  A memory address, from the memory address box or the console's MD: any
//  expression the console evaluates, alone or inside a 6502 operand form.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryGoTo
{
public:
    //  `peek` reads the pointer of an indirect form; empty for I/O.
    using GoToPeek = std::function<std::optional<Byte> (Word address)>;

    static bool  TryResolveGoTo          (const std::string & text, const IDebugExpressionContext & context, const GoToPeek & peek, Word & address, std::string & error);
    static bool  TryResolveMemoryAddress (DebugSession & session, const std::string & text, Word & address, std::string & error);

private:
    struct GoToOperand
    {
        std::string  core;
        char         index         = 0;
        bool         isIndirect    = false;
        bool         isIndexInside = false;
    };

    static GoToOperand  SplitGoToOperand   (const std::string & text);
    static size_t       FindClosingParen   (const std::string & text);
    static bool         IsWrittenAbsolute  (const std::string & core);
    static bool         TryReadGoToPointer (const GoToOperand & operand, Word value, Word index, bool isZeroPage, const GoToPeek & peek, Word & address, std::string & error);
};
