#pragma once

#include "Ui/Debugger/DebuggerViewSnapshot.h"
#include "Ui/Debugger/InstructionTouches.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  CodeAnnotations
//
//  The words beside a code line: the registers its instruction reads, the
//  address it accesses and the byte there, the pointer an indirect mode
//  reads, the flags, and what running it changes.
//
////////////////////////////////////////////////////////////////////////////////

class CodeAnnotations
{
public:
    static std::string                            GetAnnotation      (DebugSession & session, const DisassemblyLine & line,
                                                                      const Cpu6502Registers & registers, const InstructionTouches::Result & touches);
    static DebuggerViewSnapshot::AnnotationParts  GetAnnotationParts (DebugSession & session, const DisassemblyLine & line,
                                                                      const Cpu6502Registers & registers, const InstructionTouches::Result & touches);

    //  A code line's annotation from its parts, with or without the operand's
    //  value and the pointer an indirect mode reads.
    static std::string  ComposeAnnotation (const DebuggerViewSnapshot::AnnotationParts & parts, bool withOperandValue, bool withPointer);

    static std::string  GetEffect       (DebugSession & session, const Cpu6502Registers & registers,
                                         const InstructionTouches::Result & touches, Word next);
    static Byte         GetRegisterByte (const std::string & name, const Cpu6502Registers & registers);
};
