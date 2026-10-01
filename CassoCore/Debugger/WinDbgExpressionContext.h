#pragma once

#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgExpressionContext
//
//  Another context read with WinDbg's number syntax, so a line typed in
//  WinDbg mode is evaluated as typed.
//
////////////////////////////////////////////////////////////////////////////////

class WinDbgExpressionContext : public IDebugExpressionContext
{
public:
    explicit WinDbgExpressionContext (const IDebugExpressionContext & inner);

    bool          TryGetRegister   (const std::string & name, Word & value) const override;
    bool          TryPeek          (Word address, Byte & value) const override;
    bool          TryResolveSymbol (const std::string & name, Word & address) const override;
    NumberSyntax  GetNumberSyntax  () const override;

private:
    const IDebugExpressionContext  & m_inner;
};
