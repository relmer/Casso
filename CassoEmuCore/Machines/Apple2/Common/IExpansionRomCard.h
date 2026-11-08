#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IExpansionRomCard
//
//  A slot card with firmware in the $C800-$CFFF expansion space, selected by
//  Apple's protocol: an access to the card's own $Cs00-$CsFF page (I/O
//  SELECT') sets a latch on the card, and an access to $CF00-$CFFF (I/O
//  STROBE') resets it. While the latch is set the card drives $C800-$CEFF.
//
//  SlotRomPage forwards the $Cs00 page here and ExpansionRomBus arbitrates
//  $C800-$CFFF among every card that implements this, on the ][, ][+ and //e
//  alike.
//
////////////////////////////////////////////////////////////////////////////////

class IExpansionRomCard
{
public:
    virtual ~IExpansionRomCard() = default;

    // An access to the card's $Cs00-$CsFF page. Returns the value of a read;
    // the return value is ignored for a write.
    virtual Byte OnSlotPageAccess       (Word address, bool isWrite, Byte value) = 0;

    // The byte a read of the card's $Cs00-$CsFF page returns, without the
    // read's selection of the expansion ROM. False where the byte cannot be
    // worked out without the read; the default is false.
    virtual bool TryPeekSlotPage        (Word address, Byte & value) const { (void) address; value = 0; return false; }

    virtual bool IsExpansionRomSelected () const = 0;

    // A read of $C800-$CEFF while selected.
    virtual Byte ReadExpansionRom       (Word address) = 0;

    virtual void DeselectExpansionRom   () = 0;

    virtual int  GetSlot                () const = 0;
};
