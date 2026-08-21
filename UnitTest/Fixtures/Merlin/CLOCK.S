********************************
*                              *
*            CLOCK             *
*                              *
*   Glen E. Bredon   9/15/84   *
*                              *
********************************


*-------------------------------------------------------
* An interrupt driven clock for use with Merlin-pro
* (2.00 at least) or BASIC.  Requires a //c since it
* uses VBLINT.
*
* Just BRUN CLOCK from Merlin or BASIC.  This enters a
* time edit routine.  When time has been entered hit
* RETURN.  Time starts at this point.
*
* CAUTION:  This is not compatible with things like XREF
* which use page 3.  Hit RESET before using such
* utilities!  (Reassembly to an address in high memory,
* before $9885, would be compatible but you would have
* to be very careful to avoid overwrite while interrupts
* are coming.)
*
* Disk accesses and other things which disable
* interrupts will cause the clock to run slow.  You can
* compensate for this by pressing the mouse button which
* causes the time to jump to the next minute.  Fast
* increases of time may be acheived by holding down
* the mouse button and the closed apple key.
*
*                ------> WARNING <------
*
* Do not use this with programs that you are not SURE
* are compatible.  It could result in loss of disk.
*-------------------------------------------------------

SAVOBJ KBD "Save object code? (1=yes, 0=no)"
VERSION KBD "Want 12 or 24 hour version (12/24)?"
HOURS = VERSION-25/-1*12+12
 ERR HOURS-VERSION
 TR

DOSWRM = $3D0
IRQVEC = $3FE
LINE1 = $400
ONSCREEN = LINE1+32
KEY = $C000
STROBE = $C010
PAGE1 = $C054
PAGE2 = $C055
CLSAPPLE = $C062 ;Closed apple key
MOUSEBUT = $C063
CONTROL = $C080
ROMREAD = %10
ENABLE = %11
MOUSE = $C400
SETMOUSE = MOUSE+$3D ;Address adjusted
IDBYTE = $FBB3
IDBYTE2 = $FBC0
BELL2 = $FBDD
HOME = $FC58

 AST 50

 ORG $240

START LDA IDBYTE ;Is it a //c?
 CMP #6
 BNE :ABORT ;Ignore if not
 LDA IDBYTE2
 BEQ :OK
:ABORT RTS ;Sorry, not sympatico
:OK JSR HOME
 BIT CONTROL!ENABLE
 BIT CONTROL!ENABLE
 LDY #PATCHEND-PATCH-1
]LOOP LDA PATCH,Y ;Fix monitor bell routine
 STA BELL2,Y ; so it will sound ok with
 DEY ; interrupts.  This will not
 BPL ]LOOP ; do anything in BASIC unless
 BIT CONTROL!ROMREAD ; you copy BASIC to the LC

 LDY #IRQEND-IRQHAND-1
]LOOP LDA HEREIRQ,Y
 STA IRQHAND,Y
 DEY
 BPL ]LOOP
 SEI ;Disable interupts while
 LDA #IRQHAND ; changing vector!
 STA IRQVEC
 LDA #>IRQHAND
 STA IRQVEC+1

*--------------------------
*       Time editor:
*--------------------------
 LDX #HOURS/24!1
 JSR SHOWTIME ;Put clock on screen
LOOP LDA ONSCREEN,X
 PHA ;Save screen char
 AND #$3F ;Make cursor
 STA ONSCREEN,X
:GK LDY KEY ;Wait for key
 BPL :GK
 BIT STROBE
 PLA
:RCHR STA ONSCREEN,X ;Replace screen char
 CPY #$8D
 BEQ :DONE ;Exit on CR
 CPY #$88 ;Backspace
 BEQ :BS
 CPY #"U"&$9F ;->
 BNE :ISDIG
 LDA :TABLE,X
 TAX
:BS LDA :TABLE+5,X
 TAX
:ISDIG CPY #"0" ;Is it a digit
 BLT LOOP
 CPY #"9"+1
 BGE LOOP ;We don't check for over
 TYA ; 60 on minutes, etc.;
 STA TIME,X ; we assume user intelligence!
 LDY #"U"&$9F
 BLT :RCHR ;Always taken

:TABLE DFB 3,4,0,6,7,7,0,1,1,3,4,4,6

:DONE LDA MOUSE+$12 ;Get offset for the
 STA JSET+1 ; SETMOUSE routine
 LDA #8
JSET JSR SETMOUSE ;This does a CLI
 JMP DOSWRM ;Must do to fix bug in
; BRK handler
*-------------------------------------
* Fix for monitor bell routine so that
* bells sound ok with interrupts:
*-------------------------------------
PATCH PHP
 SEI
 NOP
 NOP
 LDY #$C0
]LOOP LDA #$C
 JSR $FCA8
 LDA $C030
 DEY
 BNE ]LOOP
 PLP
 RTS
PATCHEND

*-----------------------------------------------------
* Patch for DOS to make sure interrupt handler has
* not been overwritten.  (If it is it forces a reset.)
* If you use other DOS patches or a non-standard DOS
* then you MUST make sure this area is available or
* change it to some other place!  Do not use PR#6
* to reboot.  (Cntr-open apple-reset is ok.)
*-----------------------------------------------------

HEREIRQ ORG $BFC8

IRQHAND PHA ;Saving registers is not
 TYA ; really needed on the //c
 PHA
 LDY #CHKEND-CHKCODE-1
]LOOP LDA CHKCODE,Y ;Compare copy of code
 CMP CLOCK,Y ; with start of clock code
 BNE * ;Force RESET if not same
 DEY
 BPL ]LOOP
 JMP CLOCK ;Handle the interrupt

 ERR \$BFDC
IRQEND ORG

 ERR \$300
 DS $300-*

*--------------------------------------------------
*          Main clock routine, at $300:
*
* If you want to use the open apple key in place
* of the mouse button, change the line  BMI :NOBUT
* to  BPL  :NOBUT  and change MOUSEBUT to be $C061.
*--------------------------------------------------

CLOCK CLD ;All interrupts are assumed
 CLC ; to be ours.
 BIT MOUSEBUT ;Mouse button pushed?
 BMI :NOBUT ;Branch if not
 BIT CLSAPPLE ;Closed apple key also down?
 BMI :DOINC ;Branch if so
 BIT BUTFLAG ;Waiting for release?
 BMI :NOACT ;Branch if so
:DOINC JSR INCMIN ;Bump minutes if not
 SEC
:NOBUT ROR BUTFLAG ;Set release flag
:NOACT JSR SHOWTIME ;Update 60 times/second
 LDA TIME
 CMP #"0"
 BNE :OK
 LDA #" " ;Blank leading "0"
 STA ONSCREEN
:OK JSR INCTIME
 PLA
 TAY
 PLA
 RTI

INCMIN LDY #3 ;Force carry to minutes
 LDA #"9"
]LOOP STA TIME+6,Y
 DEY
 BPL ]LOOP
INCTIME LDY #9
]LOOP LDA TIME,Y
 CLC
 ADC #1
 CMP MAX,Y
 BLT :OUT
 LDA FIRST,Y
:OUT STA TIME,Y
 DEY
 BGE ]LOOP
 CPY #5 ;Add 1/60 th sec every
 BEQ INCTIME ; 10 seconds (approx)
 LDA TIME+1
 CMP #HOURS/24+3."0"
 LDA TIME
 SBC #HOURS/12."0"
 BLT :RET ;Branch if not
 LDA #"0" ;Fix hours which
 STA TIME ; haven't properly
 LDA #HOURS/24!1+"0"
 STA TIME+1 ; carried.
:RET RTS

SHOWTIME LDY #7
 STA KEY+1 ;Allow page 2 switch to
 STA PAGE2 ; use aux memory
 LDA #" "
]LOOP STA ONSCREEN,Y
 DEY
 BPL ]LOOP
 STA PAGE1
 LDY #8
]LOOP LDA TIME-1,Y
 STA ONSCREEN-1,Y
 DEY
 BPL ]LOOP
 RTS

 ASC " "
TIME ASC "00:00:0000"
MAX ASC ":::6::6:6:"
FIRST ASC "00:00:0000"
BUTFLAG BRK ;Flag to read button only once

CHKCODE CLD ;Copy of start of clock code
 CLC ; to check for overwrite
 BIT MOUSEBUT
CHKEND

 ERR \$3D0

 DO SAVOBJ
 DO HOURS-12
 SAV CLOCK.24
 ELSE
 SAV CLOCK.12
 FIN
 FIN
