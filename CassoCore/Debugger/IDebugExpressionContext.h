#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  NumberSyntax
//
//  How a number is written. AppleWin marks hex with $ and decimal with #;
//  WinDbg also takes 0x for hex and 0n for decimal. A bare number is hex in
//  both.
//
////////////////////////////////////////////////////////////////////////////////

enum class NumberSyntax
{
    AppleWin,
    WinDbg,
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDebugExpressionContext
//
//  What an expression can read while it is evaluated: the registers, memory
//  through a side-effect-free peek, and the enabled symbol tables. Symbol
//  lookup ignores case. The number syntax is the one the line was typed in.
//
////////////////////////////////////////////////////////////////////////////////

class IDebugExpressionContext
{
public:
    virtual ~IDebugExpressionContext() = default;

    virtual bool          TryGetRegister   (const std::string & name, Word & value) const    = 0;
    virtual bool          TryPeek          (Word address, Byte & value) const                = 0;

    //  The byte a read of a $C000-$C0FF switch would return, worked out by its
    //  device without operating it; false where reading is the operation. The
    //  default returns false, since it has no devices.
    virtual bool          TryPeekIo        (Word address, Byte & value) const                { (void) address; value = 0; return false; }
    virtual bool          TryResolveSymbol (const std::string & name, Word & address) const  = 0;
    virtual NumberSyntax  GetNumberSyntax  () const                                          { return NumberSyntax::AppleWin; }
};
