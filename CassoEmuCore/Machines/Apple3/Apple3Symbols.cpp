#include "Pch.h"

#include "Machines/Apple3/Apple3Symbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kpszApple3Symbols
//
//  The I/O space is decoded in blocks of 16 (Service Reference Manual 2.30).
//  Each display, A/D and I/O control switch has an even address that clears
//  it and an odd one that sets it. The VIA registers follow the 6522's
//  order; the environment register is the D VIA's port A without handshake
//  and the bank register is the E VIA's.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char * s_kpszApple3Symbols =
    "; Apple /// I/O space\n"
    "KBD=$C000\n"       "KBDMOD=$C008\n"    "KBDSTRB=$C010\n"   "DESELC8=$C020\n"
    "SPKR=$C030\n"      "BEEP=$C040\n"
    "BWON=$C050\n"      "COLORON=$C051\n"   "COL40=$C052\n"     "COL80=$C053\n"
    "PAGE1=$C054\n"     "PAGE2=$C055\n"     "TEXTON=$C056\n"    "GRAPHON=$C057\n"
    "AD0OFF=$C058\n"    "AD0ON=$C059\n"     "AD2OFF=$C05A\n"    "AD2ON=$C05B\n"
    "ADRAMP=$C05C\n"    "ADSTART=$C05D\n"   "AD1OFF=$C05E\n"    "AD1ON=$C05F\n"
    "SWB1=$C060\n"      "SWA0=$C061\n"      "SWB0=$C062\n"      "SWA1=$C063\n"
    "SLOT2=$C064\n"     "SLOT1=$C065\n"     "ADTIMEOUT=$C066\n" "CLOCK=$C070\n"
    "EXT2OFF=$C0D0\n"   "EXT2ON=$C0D1\n"    "EXT1OFF=$C0D2\n"   "EXT1ON=$C0D3\n"
    "INTON=$C0D4\n"     "INTOFF=$C0D5\n"    "SCROLLOFF=$C0D8\n" "SCROLLON=$C0D9\n"
    "CHRDLOFF=$C0DA\n"  "CHRDLON=$C0DB\n"   "ENSELOFF=$C0DC\n"  "ENSELON=$C0DD\n"
    "ENSIOIN=$C0DE\n"   "ENSIOOUT=$C0DF\n"
    "PHASE0OFF=$C0E0\n" "PHASE0ON=$C0E1\n"  "PHASE1OFF=$C0E2\n" "PHASE1ON=$C0E3\n"
    "PHASE2OFF=$C0E4\n" "PHASE2ON=$C0E5\n"  "PHASE3OFF=$C0E6\n" "PHASE3ON=$C0E7\n"
    "MOTOROFF=$C0E8\n"  "MOTORON=$C0E9\n"   "DRVINT=$C0EA\n"    "DRVEXT=$C0EB\n"
    "Q6L=$C0EC\n"       "Q6H=$C0ED\n"       "Q7L=$C0EE\n"       "Q7H=$C0EF\n"
    "ACIADATA=$C0F0\n"  "ACIASTAT=$C0F1\n"  "ACIACMD=$C0F2\n"   "ACIACTL=$C0F3\n"
    "; Apple /// D VIA\n"
    "ZPREG=$FFD0\n"     "DORA=$FFD1\n"      "DDDRB=$FFD2\n"     "DDDRA=$FFD3\n"
    "DT1CL=$FFD4\n"     "DT1CH=$FFD5\n"     "DT1LL=$FFD6\n"     "DT1LH=$FFD7\n"
    "DT2CL=$FFD8\n"     "DT2CH=$FFD9\n"     "DSR=$FFDA\n"       "DACR=$FFDB\n"
    "DPCR=$FFDC\n"      "DIFR=$FFDD\n"      "DIER=$FFDE\n"      "ENVREG=$FFDF\n"
    "; Apple /// E VIA\n"
    "SOUND=$FFE0\n"     "EORA=$FFE1\n"      "EDDRB=$FFE2\n"     "EDDRA=$FFE3\n"
    "ET1CL=$FFE4\n"     "ET1CH=$FFE5\n"     "ET1LL=$FFE6\n"     "ET1LH=$FFE7\n"
    "ET2CL=$FFE8\n"     "ET2CH=$FFE9\n"     "ESR=$FFEA\n"       "EACR=$FFEB\n"
    "EPCR=$FFEC\n"      "EIFR=$FFED\n"      "EIER=$FFEE\n"      "BANKREG=$FFEF\n"
    "; 6502 vectors\n"
    "NMIVECT=$FFFA\n"   "RESETVECT=$FFFC\n" "IRQVECT=$FFFE\n";





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Symbols::GetMain
//
////////////////////////////////////////////////////////////////////////////////

const char * Apple3Symbols::GetMain()
{
    return s_kpszApple3Symbols;
}
