#include "Pch.h"

#include "Debugger/ConditionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConditionContext::ConditionContext
//
////////////////////////////////////////////////////////////////////////////////

ConditionContext::ConditionContext (
    const IDebugExpressionContext  & inner,
    std::optional<Word>              access,
    std::optional<Byte>              value) :
    m_inner  (inner),
    m_access (access),
    m_value  (value)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConditionContext::IsMet
//
////////////////////////////////////////////////////////////////////////////////

bool ConditionContext::IsMet (
    const Expression                & condition,
    const IDebugExpressionContext   & inner,
    std::optional<Word>               access,
    std::optional<Byte>               value,
    std::optional<int32_t>          & result)
{
    ConditionContext  context   (inner, access, value);
    int32_t           evaluated = 0;
    std::string       error;
    HRESULT           hr        = S_OK;



    result.reset();

    if (condition.postfix.empty())
    {
        return true;
    }

    hr = DebugExpressionEvaluator::Evaluate (condition, context, evaluated, error);

    if (FAILED (hr))
    {
        return false;
    }

    result = evaluated;
    return evaluated != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConditionContext::Validate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ConditionContext::Validate (
    const Expression                & condition,
    const IDebugExpressionContext   & inner,
    bool                              hasAccess,
    bool                              hasValue,
    std::string                     & error)
{
    HRESULT              hr       = S_OK;
    std::optional<Word>  access   = hasAccess ? std::optional<Word> (0) : std::nullopt;
    std::optional<Byte>  value    = hasValue  ? std::optional<Byte> (0) : std::nullopt;
    ConditionContext     context  (inner, access, value);
    int32_t              result   = 0;
    bool                 isIoRead = false;



    hr       = DebugExpressionEvaluator::Evaluate (condition, context, result, error);
    isIoRead = context.m_ioRead.has_value();

    CBRFEx (!isIoRead, E_INVALIDARG,
            error = std::format ("The condition reads ${:04X}, an I/O address. Reading it would change the machine.", *context.m_ioRead));
    CHR    (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConditionContext::TryGetRegister
//
////////////////////////////////////////////////////////////////////////////////

bool ConditionContext::TryGetRegister (const std::string & name, Word & value) const
{
    return m_inner.TryGetRegister (name, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConditionContext::TryPeek
//
////////////////////////////////////////////////////////////////////////////////

bool ConditionContext::TryPeek (Word address, Byte & value) const
{
    if (address >= kIoFirst && address <= kIoLast)
    {
        if (!m_ioRead.has_value())
        {
            m_ioRead = address;
        }

        return false;
    }

    return m_inner.TryPeek (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConditionContext::TryResolveSymbol
//
//  ACCESS and VALUE come first, where the hit provides them, so a program
//  symbol of either name cannot hide them.
//
////////////////////////////////////////////////////////////////////////////////

bool ConditionContext::TryResolveSymbol (const std::string & name, Word & address) const
{
    if (m_access.has_value() && _stricmp (name.c_str(), "ACCESS") == 0)
    {
        address = *m_access;
        return true;
    }

    if (m_value.has_value() && _stricmp (name.c_str(), "VALUE") == 0)
    {
        address = *m_value;
        return true;
    }

    return m_inner.TryResolveSymbol (name, address);
}
