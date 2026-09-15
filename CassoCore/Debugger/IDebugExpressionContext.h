#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  IDebugExpressionContext
//
//  What an expression can read while it is evaluated: the registers, memory
//  through a side-effect-free peek, and the enabled symbol tables. Symbol
//  lookup ignores case.
//
////////////////////////////////////////////////////////////////////////////////

class IDebugExpressionContext
{
public:
    virtual ~IDebugExpressionContext() = default;

    virtual bool  TryGetRegister   (const std::string & name, Word & value) const    = 0;
    virtual bool  TryPeek          (Word address, Byte & value) const                = 0;
    virtual bool  TryResolveSymbol (const std::string & name, Word & address) const  = 0;
};
