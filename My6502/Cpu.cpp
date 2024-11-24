#include "Cpu.h"

void Cpu::Reset ()
{
    pc = 0xFFFC;
    sp = 0x0100;

    status.status = 0;
}
