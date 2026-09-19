#pragma once

#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConditionContext
//
//  What a breakpoint's IF expression is evaluated in: the session's
//  registers, memory and symbols, plus the pseudo-symbols ACCESS and VALUE
//  on a watch hit, which give the accessed address and the byte read or
//  written. A read of an I/O address is never made, because reading one
//  changes the machine; the first one attempted is recorded instead.
//
////////////////////////////////////////////////////////////////////////////////

class ConditionContext : public IDebugExpressionContext
{
public:
    ConditionContext (const IDebugExpressionContext & inner, std::optional<Word> access, std::optional<Byte> value);

    //  True when the condition is empty, or evaluates to nonzero; result
    //  receives the value when there is one. A condition that cannot be
    //  evaluated is not met.
    static bool     IsMet    (const Expression                & condition,
                              const IDebugExpressionContext   & inner,
                              std::optional<Word>               access,
                              std::optional<Byte>               value,
                              std::optional<int32_t>          & result);

    //  Evaluates the condition once as it is set, with ACCESS and VALUE
    //  present as zero where the breakpoint provides them. Fails on an
    //  unknown symbol or any other error, and on a read of an I/O address.
    static HRESULT  Validate (const Expression                & condition,
                              const IDebugExpressionContext   & inner,
                              bool                              hasAccess,
                              bool                              hasValue,
                              std::string                     & error);

    bool  TryGetRegister   (const std::string & name, Word & value) const override;
    bool  TryPeek          (Word address, Byte & value) const override;
    bool  TryResolveSymbol (const std::string & name, Word & address) const override;

private:
    static constexpr Word  kIoFirst = 0xC000;
    static constexpr Word  kIoLast  = 0xC0FF;

    const IDebugExpressionContext  & m_inner;
    std::optional<Word>              m_access;
    std::optional<Byte>              m_value;
    mutable std::optional<Word>      m_ioRead;
};
