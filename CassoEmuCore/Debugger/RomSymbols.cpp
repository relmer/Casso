#include "Pch.h"

#include "Debugger/RomSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kpszMonitorSymbols
//
//  The Apple II Monitor's zero-page locations, its entry points and the
//  original I/O soft switches. Sources: Apple II Reference Manual (Apple
//  Computer, 1979), chapters 3 and 4 and the Monitor listing; Apple IIe
//  Technical Reference Manual (Addison-Wesley, 1985), appendix A.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char * s_kpszMonitorSymbols =
    "; Apple II Monitor: zero page\n"
    "WNDLFT=$0020\n"  "WNDWDTH=$0021\n" "WNDTOP=$0022\n"  "WNDBTM=$0023\n"
    "CH=$0024\n"      "CV=$0025\n"      "GBASL=$0026\n"   "GBASH=$0027\n"
    "BASL=$0028\n"    "BASH=$0029\n"    "BAS2L=$002A\n"   "BAS2H=$002B\n"
    "H2=$002C\n"      "V2=$002D\n"      "MASK=$002E\n"    "COLOR=$0030\n"
    "MODE=$0031\n"    "INVFLG=$0032\n"  "PROMPT=$0033\n"  "YSAV=$0034\n"
    "YSAV1=$0035\n"   "CSWL=$0036\n"    "CSWH=$0037\n"    "KSWL=$0038\n"
    "KSWH=$0039\n"    "PCL=$003A\n"     "PCH=$003B\n"     "A1L=$003C\n"
    "A1H=$003D\n"     "A2L=$003E\n"     "A2H=$003F\n"     "A3L=$0040\n"
    "A3H=$0041\n"     "A4L=$0042\n"     "A4H=$0043\n"     "A5L=$0044\n"
    "A5H=$0045\n"     "ACC=$0045\n"     "XREG=$0046\n"    "YREG=$0047\n"
    "STATUS=$0048\n"  "SPNT=$0049\n"    "RNDL=$004E\n"    "RNDH=$004F\n"
    "; Apple II Monitor: page 2 and 3\n"
    "IN=$0200\n"      "BRKV=$03F0\n"    "SOFTEV=$03F2\n"  "PWREDUP=$03F4\n"
    "AMPERV=$03F5\n"  "USRADR=$03F8\n"  "NMI=$03FB\n"     "IRQLOC=$03FE\n"
    "MSLOT=$07F8\n"
    "; Apple II Monitor: entry points\n"
    "PLOT=$F800\n"    "HLINE=$F819\n"   "VLINE=$F828\n"   "CLRSCR=$F832\n"
    "CLRTOP=$F836\n"  "GBASCALC=$F847\n" "NXTCOL=$F85F\n" "SETCOL=$F864\n"
    "SCRN=$F871\n"    "INSDS1=$F882\n"  "INSDS2=$F88E\n"  "INSTDSP=$F8D0\n"
    "PRNTYX=$F940\n"  "PRNTAX=$F941\n"  "PRNTX=$F944\n"   "PRBLNK=$F948\n"
    "PRBL2=$F94A\n"   "PCADJ=$F953\n"   "PREAD=$FB1E\n"   "INIT=$FB2F\n"
    "SETTXT=$FB39\n"  "SETGR=$FB40\n"   "SETWND=$FB4B\n"  "TABV=$FB5B\n"
    "APPLEII=$FB60\n" "VTAB=$FC22\n"    "VTABZ=$FC24\n"   "ESC1=$FC2C\n"
    "CLREOP=$FC42\n"  "HOME=$FC58\n"    "CR=$FC62\n"      "LF=$FC66\n"
    "SCROLL=$FC70\n"  "CLREOL=$FC9C\n"  "CLEOLZ=$FC9E\n"  "WAIT=$FCA8\n"
    "NXTA4=$FCB4\n"   "NXTA1=$FCBA\n"   "HEADR=$FCC9\n"   "RDKEY=$FD0C\n"
    "KEYIN=$FD1B\n"   "RDCHAR=$FD35\n"  "GETLNZ=$FD67\n"  "GETLN=$FD6A\n"
    "GETLN1=$FD6F\n"  "CROUT1=$FD8B\n"  "CROUT=$FD8E\n"   "PRA1=$FD92\n"
    "PRBYTE=$FDDA\n"  "PRHEX=$FDE3\n"   "COUT=$FDED\n"    "COUT1=$FDF0\n"
    "COUTZ=$FDF6\n"   "IDROUTINE=$FE1F\n" "MOVE=$FE2C\n"  "VERIFY=$FE36\n"
    "LIST=$FE5E\n"    "SETINV=$FE80\n"  "SETNORM=$FE84\n" "SETKBD=$FE89\n"
    "INPORT=$FE8B\n"  "SETVID=$FE93\n"  "OUTPORT=$FE95\n" "GO=$FEB6\n"
    "WRITE=$FECD\n"   "READ=$FEFD\n"    "BELL=$FF3A\n"    "RESTORE=$FF3F\n"
    "SAVE=$FF4A\n"    "OLDRST=$FF59\n"  "MON=$FF65\n"     "MONZ=$FF69\n"
    "BELL1=$FBDD\n"   "BELL2=$FBE4\n"   "STORADV=$FBF0\n" "ADVANCE=$FBF4\n"
    "VIDOUT=$FBFD\n"  "BS=$FC10\n"      "UP=$FC1A\n"      "RESET=$FA62\n"
    "PWRUP=$FAA6\n"   "SLOOP=$FABA\n"   "REGDSP=$FAD7\n"  "RGDSP1=$FADA\n"
    "IRQ=$FA40\n"     "BREAK=$FA4C\n"   "OLDBRK=$FA59\n"  "NMIVECT=$FFFA\n"
    "RESETVECT=$FFFC\n" "IRQVECT=$FFFE\n"
    "; Apple II I/O\n"
    "KBD=$C000\n"     "KBDSTRB=$C010\n" "TAPEOUT=$C020\n" "SPKR=$C030\n"
    "STROBE=$C040\n"  "TXTCLR=$C050\n"  "TXTSET=$C051\n"  "MIXCLR=$C052\n"
    "MIXSET=$C053\n"  "TXTPAGE1=$C054\n" "TXTPAGE2=$C055\n" "LORES=$C056\n"
    "HIRES=$C057\n"   "SETAN0=$C058\n"  "CLRAN0=$C059\n"  "SETAN1=$C05A\n"
    "CLRAN1=$C05B\n"  "SETAN2=$C05C\n"  "CLRAN2=$C05D\n"  "SETAN3=$C05E\n"
    "CLRAN3=$C05F\n"  "TAPEIN=$C060\n"  "BUTN0=$C061\n"   "BUTN1=$C062\n"
    "BUTN2=$C063\n"   "PADDL0=$C064\n"  "PADDL1=$C065\n"  "PADDL2=$C066\n"
    "PADDL3=$C067\n"  "PTRIG=$C070\n"
    //  The Disk II controller in slot 6, where every Apple II boots from.
    //  Source: Apple Disk II reference; the controller's own behavior is in
    //  Disk2Controller. A controller in another slot answers at $C080 + slot
    //  * 16, which these symbols do not cover.
    "; Disk II, slot 6\n"
    "PHASE0OFF=$C0E0\n" "PHASE0ON=$C0E1\n" "PHASE1OFF=$C0E2\n" "PHASE1ON=$C0E3\n"
    "PHASE2OFF=$C0E4\n" "PHASE2ON=$C0E5\n" "PHASE3OFF=$C0E6\n" "PHASE3ON=$C0E7\n"
    "MOTOROFF=$C0E8\n"  "MOTORON=$C0E9\n"  "DRV0EN=$C0EA\n"    "DRV1EN=$C0EB\n"
    "Q6L=$C0EC\n"       "Q6H=$C0ED\n"      "Q7L=$C0EE\n"       "Q7H=$C0EF\n";





////////////////////////////////////////////////////////////////////////////////
//
//  s_kpszIieSymbols
//
//  What the //e and //c add: the memory-management switches, their read
//  locations, and the 80-column and double hi-res switches. Source: Apple
//  IIe Technical Reference Manual, chapter 4; Apple IIc Technical Reference
//  Manual (Addison-Wesley, 1986), chapter 2.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char * s_kpszIieSymbols =
    "; Apple IIe memory management and video switches\n"
    "80STOREOFF=$C000\n" "80STOREON=$C001\n" "RAMRDOFF=$C002\n"  "RAMRDON=$C003\n"
    "RAMWRTOFF=$C004\n"  "RAMWRTON=$C005\n"  "INTCXROMOFF=$C006\n" "INTCXROMON=$C007\n"
    "ALTZPOFF=$C008\n"   "ALTZPON=$C009\n"   "SLOTC3ROMOFF=$C00A\n" "SLOTC3ROMON=$C00B\n"
    "80COLOFF=$C00C\n"   "80COLON=$C00D\n"   "ALTCHARSETOFF=$C00E\n" "ALTCHARSETON=$C00F\n"
    "RDLCBNK2=$C011\n"   "RDLCRAM=$C012\n"   "RDRAMRD=$C013\n"   "RDRAMWRT=$C014\n"
    "RDCXROM=$C015\n"    "RDALTZP=$C016\n"   "RDC3ROM=$C017\n"   "RD80STORE=$C018\n"
    "RDVBL=$C019\n"      "RDTEXT=$C01A\n"    "RDMIXED=$C01B\n"   "RDPAGE2=$C01C\n"
    "RDHIRES=$C01D\n"    "RDALTCHAR=$C01E\n" "RD80COL=$C01F\n"   "DHIRESON=$C05E\n"
    "DHIRESOFF=$C05F\n"  "LCBANK2=$C083\n"   "LCBANK1=$C08B\n"   "ROMIN=$C081\n"
    //  The rest of the bank-switched RAM switches. $C080-$C083 and
    //  $C088-$C08B each repeat four addresses higher; the mirrors are left
    //  without symbols rather than given invented ones, since one name must
    //  not stand for two addresses.
    "READBSR2=$C080\n"   "OFFBSR2=$C082\n"   "READBSR1=$C088\n"
    "WRITEBSR1=$C089\n"  "OFFBSR1=$C08A\n"
    //  The IOU access switches, which decide whether $C058-$C05F are the
    //  annunciators or the mouse and VBL switches. Both pairs drive the one
    //  latch. Source: Apple //c Technical Note #9.
    "IOUDISON=$C078\n"   "IOUDISOFF=$C079\n" "SETIOUDIS=$C07E\n" "CLRIOUDIS=$C07F\n"
    "RDIOUDIS=$C07E\n"   "RDDHIRES=$C07F\n";





////////////////////////////////////////////////////////////////////////////////
//
//  s_kpszApplesoftSymbols
//
//  Applesoft's documented entry points and zero-page pointers. Sources:
//  Applesoft BASIC Programmer's Reference Manual (Apple Computer, 1978),
//  appendix on zero-page usage; Apple II Reference Manual, chapter 4.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char * s_kpszApplesoftSymbols =
    "; Applesoft: zero page\n"
    "LINNUM=$0050\n"  "TXTTAB=$0067\n"  "VARTAB=$0069\n"  "ARYTAB=$006B\n"
    "STREND=$006D\n"  "FRETOP=$006F\n"  "MEMSIZ=$0073\n"  "CURLIN=$0075\n"
    "OLDLIN=$0077\n"  "OLDTEXT=$0079\n" "DATLIN=$007B\n"  "DATPTR=$007D\n"
    "VARNAM=$0081\n"  "VARPNT=$0083\n"  "FORPNT=$0085\n"  "FAC=$009D\n"
    "FACSIGN=$00A2\n" "ARG=$00A5\n"     "ARGSIGN=$00AA\n" "CHRGET=$00B1\n"
    "CHRGOT=$00B7\n"  "TXTPTR=$00B8\n"  "RNDSEED=$00C9\n" "HGRX=$00E0\n"
    "HGRY=$00E2\n"    "HGRCOLOR=$00E4\n" "HPAG=$00E6\n"   "SCALE=$00E7\n"
    "SHAPETBL=$00E8\n" "ROT=$00F9\n"
    "; Applesoft: entry points\n"
    "COLDSTART=$E000\n" "RESTART=$E003\n" "ERROR=$D412\n" "NEWSTT=$D7D2\n"
    "FRMNUM=$DD67\n"  "FRMEVL=$DD7B\n"  "CHKCOM=$DEBE\n"  "SYNCHR=$DEC0\n"
    "PTRGET=$DFE3\n"  "GETBYT=$E6F8\n"  "GTBYTC=$E6F5\n"  "COMBYTE=$E74C\n"
    "GETADR=$E752\n"  "GIVAYF=$E2F2\n"  "GETNUM=$E746\n"  "SNGFLT=$E301\n"
    "FADD=$E7C1\n"    "FSUB=$E7AA\n"    "FMULT=$E97F\n"   "FDIV=$EA66\n"
    "MOVFM=$EAF9\n"   "MOVMF=$EB2B\n"   "FLOAT=$EB93\n"   "QINT=$EBF2\n"
    "STROUT=$DB3A\n"  "LINPRT=$ED24\n"  "FOUT=$ED34\n"
    "HPOSN=$F411\n"   "HPLOT=$F457\n"   "HLIN=$F53A\n"    "HFIND=$F5CB\n"
    "DRAW=$F601\n"    "XDRAW=$F65D\n"   "HGR2=$F3D8\n"    "HGR=$F3E2\n"
    "HCLR=$F3F2\n"    "BKGND=$F3F6\n"   "HCOLOR=$F6E9\n"  "AMPERSAND=$03F5\n";





////////////////////////////////////////////////////////////////////////////////
//
//  s_kpszDos33Symbols
//
//  DOS 3.3's page-3 vectors and the entries the DOS manual documents for
//  programs. Source: The DOS Manual (Apple Computer, 1980), chapter 6 and
//  appendix B.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char * s_kpszDos33Symbols =
    "; DOS 3.3 page-3 vectors and where DOS loads\n"
    "DOSWARM=$03D0\n" "DOSCOLD=$03D3\n" "RWTS=$03D9\n"    "FMPARM=$03DC\n"
    "RWTSPARM=$03E3\n" "CONNECT=$03EA\n" "DOS=$9600\n";





////////////////////////////////////////////////////////////////////////////////
//
//  s_kpszProDosSymbols
//
//  The ProDOS Machine Language Interface and its global page. Source: ProDOS
//  8 Technical Reference Manual (Apple Computer, 1985), chapters 4 and 5.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char * s_kpszProDosSymbols =
    "; ProDOS 8: MLI and global page\n"
    "MLI=$BF00\n"     "JSPARE=$BF03\n"  "DATETIME=$BF06\n" "SYSERR=$BF09\n"
    "SYSDEATH=$BF0C\n" "SERR=$BF0F\n"   "DEVADR=$BF10\n"  "DEVNUM=$BF30\n"
    "DEVCNT=$BF31\n"  "DEVLST=$BF32\n"  "BITMAP=$BF58\n"  "BUFTBL=$BF70\n"
    "DATELO=$BF90\n"  "DATEHI=$BF91\n"  "TIMELO=$BF92\n"  "TIMEHI=$BF93\n"
    "LEVEL=$BF94\n"   "BUBIT=$BF95\n"   "SPARE1=$BF96\n"  "MACHID=$BF98\n"
    "SLTBYT=$BF99\n"  "PFIXPTR=$BF9A\n" "MLIACTV=$BF9B\n" "CMDADR=$BF9C\n"
    "SAVEX=$BF9E\n"   "SAVEY=$BF9F\n"   "IBAKVER=$BFFD\n" "IVERSION=$BFFE\n"
    "KVERSION=$BFFF\n";





////////////////////////////////////////////////////////////////////////////////
//
//  RomSymbols::GetMain
//
//  The //e and //c get the Monitor table with the memory-management switches
//  appended; the ][ and ][+ get the Monitor table alone.
//
////////////////////////////////////////////////////////////////////////////////

const char * RomSymbols::GetMain (const std::string & machineName)
{
    static const std::string  s_iie = std::string (s_kpszMonitorSymbols) + s_kpszIieSymbols;



    return IsIie (machineName) ? s_iie.c_str() : s_kpszMonitorSymbols;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RomSymbols::GetBasic
//
////////////////////////////////////////////////////////////////////////////////

const char * RomSymbols::GetBasic()
{
    return s_kpszApplesoftSymbols;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RomSymbols::GetDos33
//
////////////////////////////////////////////////////////////////////////////////

const char * RomSymbols::GetDos33()
{
    return s_kpszDos33Symbols;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RomSymbols::GetProDos
//
////////////////////////////////////////////////////////////////////////////////

const char * RomSymbols::GetProDos()
{
    return s_kpszProDosSymbols;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RomSymbols::IsIie
//
//  A machine name holding "2e", "2c", "//e" or "//c" has the //e memory map.
//
////////////////////////////////////////////////////////////////////////////////

bool RomSymbols::IsIie (const std::string & machineName)
{
    std::string  lower (machineName);



    for (char & ch : lower)
    {
        ch = (char) tolower ((unsigned char) ch);
    }

    return lower.find ("2e") != std::string::npos || lower.find ("2c") != std::string::npos ||
           lower.find ("//e") != std::string::npos || lower.find ("//c") != std::string::npos;
}
