#include "Pch.h"

#include "Shell/MachineParts.h"
#include "Core/Prng.h"
#include "Devices/IAciaEndpoint.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/CharacterRomData.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineParts::MachineParts
//
////////////////////////////////////////////////////////////////////////////////

MachineParts::MachineParts() :
    memoryBus (std::make_unique<MemoryBus>()),
    charRom   (std::make_unique<CharacterRomData>())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineParts::~MachineParts
//
//  Out of line, where the types the slots have are complete.
//
////////////////////////////////////////////////////////////////////////////////

MachineParts::~MachineParts()
{
}
