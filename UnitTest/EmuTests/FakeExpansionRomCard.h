#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IExpansionRomCard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeExpansionRomCard
//
//  An IExpansionRomCard returning fixed bytes. The last page and ROM
//  addresses accessed are stored, so a routing test can check whether an
//  access was passed to the card.
//
////////////////////////////////////////////////////////////////////////////////

class FakeExpansionRomCard : public IExpansionRomCard
{
public:
    explicit FakeExpansionRomCard (Byte romByte) : m_romByte (romByte) {}

    Byte OnSlotPageAccess (Word address, bool isWrite, Byte value) override
    {
        (void) isWrite;
        (void) value;

        m_selected        = true;
        m_lastPageAddress = address;

        return m_pageByte;
    }

    bool IsExpansionRomSelected() const override { return m_selected; }

    Byte ReadExpansionRom (Word address) override
    {
        m_lastRomAddress = address;

        return m_romByte;
    }

    void DeselectExpansionRom() override { m_selected = false; }

    Byte  m_romByte         = 0;
    Byte  m_pageByte        = 0x99;
    bool  m_selected        = false;
    Word  m_lastPageAddress = 0;
    Word  m_lastRomAddress  = 0;
};
