#include "Pch.h"

#include "InstructionSetProvider.h"

#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetProvider::InstructionSetProvider
//
//  One instruction set, with nothing to switch to. This is what every caller
//  that predates in-source CPU selection gets, and it behaves exactly as a
//  single fixed table always did.
//
////////////////////////////////////////////////////////////////////////////////

InstructionSetProvider::InstructionSetProvider (const Microcode base[256]) :
    m_base        (base),
    m_extended    (base),
    m_hasExtended (false)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetProvider::InstructionSetProvider
//
//  Base and extended sets, for a dialect whose source can select the wider one.
//
//  Both tables are built up front rather than on first use. Building one
//  mid-assembly would put the cost inside the pass and, worse, would make the
//  set available only after the directive that asked for it -- while pass 2 has
//  to be able to replay a selection pass 1 made.
//
////////////////////////////////////////////////////////////////////////////////

InstructionSetProvider::InstructionSetProvider (const Microcode base[256], const Microcode extended[256]) :
    m_base        (base),
    m_extended    (extended),
    m_hasExtended (true)
{
}
