#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Core/IInterruptController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockIrqAsserter
//
//  Test double that exposes Assert() / Clear() against an injected
//  IInterruptController. Used to drive the interrupt aggregation path
//  in unit tests without standing up real source devices.
//
////////////////////////////////////////////////////////////////////////////////

class MockIrqAsserter
{
public:
    explicit            MockIrqAsserter (IInterruptController * ic);

    HRESULT             Bind   ();
    void                Assert ();
    void                Clear  ();

    IrqSourceId         GetSourceId  () const { return m_sourceId; }
    bool                IsBound      () const { return m_bound; }

private:
    IInterruptController *    m_ic       = nullptr;
    IrqSourceId               m_sourceId = 0;
    bool                      m_bound    = false;
};
