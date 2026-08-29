********************************
*                              *
*          MAKE DUMP           *
*                              *
*   Glen E. Bredon   8/15/84   *
*                              *
*------------------------------*
*                              *
*     Merlin-pro assembler     *
*                              *
********************************

* This makes a hex dump text file from a Merlin-pro
* object file after a valid assembly of any source.
* (It is NOT usable with older versions of Merlin.)
*
* To use it, BRUN it from EXEC in Merlin-pro;
* Load desired source;
* ASM it;
* Type USER from the editor.
*
* (This will ask if you mean it, because it will
* destroy the existing source code in memory.)
*
* If successful, the current source in memory will
* be replaced with the hex dump of the object file
* previously assembled.  The last line of the file
* will contain a BSAVE line with the correct address
* and length but without file name.  You can use
* Merlin's editor to insert the desired file name
* and then can use Merlin's Write command to save
* this file as a DOS EXEC file.  (If you do not
* insert the file name then you will get a SYNTAX
* ERROR when the file is EXECed.

 TR
 EXP OFF

LOADADR = $9000 ;Address of BRUN
MAINPROG = $900 ;Main program in aux mem
INTRFACE = $300 ;Interface in main mem

* Merlin stuff:

SOURCE = $A ;Source beginning
HIMEM = $C ;Top of usable memory
ENDSRC = $E ;End of source file
OBJPNT = $E2 ;Address of obj (in aux mem)
OBLEN = $E4 ;Length of object code
ADRSV = $E6 ;ORG address of last assembly
VALID = $E8 ;Flags valid assembly
TABS = $F4 ;Merlin editor's tabs

* Usage by this program:

ADRS = $90 ;Save for ADRSV
LEN = $92 ;Save for OBLEN
TXTP = $94 ;For SENDMSG routine
BYTCNT = $96 ;Countdown for bytes/line
XREG = $9E ;Register saves for
AREG = $9F ; bank switching use.

* Data:

BYTESPER = 16 ;# bytes/line desired

* Apple stuff:

FROM = $3C ;A1L
ENDMOVE = $3E ;A2L
DEST = $42 ;A4L
AMPER = $3F5
BASIC = $E000
KEYIN = $FD0C
COUT = $FDED
MONMOVE = $FE2C

* AUX mem equates:

MAINWRT = $C004
AUXWRT = $C005
READZP = $C016
AUXMOVE = $C311
XFER = $C314

 AST 50


MOV MAC
 LDA ]1
 STA ]2
 <<<

MOVD MAC
 MOV ]1;]2
 MOV ]1+1;]2+1
 <<<

INCD MAC
 INC ]1
 BNE NI
 INC ]1+1
NI
 <<<

DECD MAC
 LDA ]1
 BNE ND
 DEC ]1+1
ND DEC ]1
 <<<

STADR MAC
 LDA #]1
 STA ]2
 LDA #>]1
 STA ]2+1
 <<<

TOAUX MAC
 PHA
 BIT ZPFLAGM
 SEC
 STADR ]1;$3ED
 PLA
 JMP XFER
 <<<

CALL MAC
 STX XREG
 LDX #A]1-ADRTBL
 JSR CALLMAIN
 <<<

STORE MAC
 LDX #]1END-]1-1
LP LDA ]1,X
 JSR PUTSRC
 DEX
 BPL LP
 <<<

ADDRESS MAC
 LDA ]1+1
 LDX ]1
 JSR MAKADR
 <<<

 AST 50

* Setup routines upon BRUN:

 ORG LOADADR

 LDA #$4C ;Set up USER vector
 STA AMPER
 STADR START;AMPER+1
 STADR HEREINT;FROM
 STADR HEREMAIN-1;ENDMOVE
 STADR START;DEST
 LDY #0
 JSR MONMOVE ;Move interface to page 3
 STADR HEREMAIN;FROM
 STADR HEREND-1;ENDMOVE
 STADR MAINPROG;DEST
 SEC ;MAIN->AUX
 JSR AUXMOVE ;Move main prog to aux mem
; and return to caller
 LDA READZP
 LSR
 STA ZPFLAGM
 STA AUXWRT
 STA ZPFLAGA
 STA MAINWRT
 RTS

*---------------------------------------
* Interface to go on page 3 main memory:
*---------------------------------------

HEREINT ORG INTRFACE

START TOAUX MAIN ;Jump to start of main program

CALLER JSR GOROUT ;Call routine indexed by X
 TOAUX RETURN ;Jump to RTS in aux mem

GOROUT LDA ADRTBL+1,X ;Get address of desired
 PHA ; routine and put it
 LDA ADRTBL,X ; on stack
 PHA
 LDA AREG ;Replace registers
 LDX XREG
 RTS ;Go to routine via RTS
;(Returns to line after CALLER)
ADRTBL
AKEYIN DA KEYIN-1
ACOUT DA COUT-1
ARET DA RET-1
AGETENDS DA GETENDS-1
AMERLIN DA MERLIN-1

GETENDS LDY #0
 LDA (ENDSRC),Y
 RTS

RET PLA
 PLA
 PLA
 PLA
 RTS

MERLIN JMP BASIC ;Exec entry

ZPFLAGM DFB $40 ;Changed on run

 ERR \$3D0

 ORG ;Recall actual load address

*-------------------------------
* Main program to go in aux mem:
*-------------------------------

HEREMAIN ORG MAINPROG

MAIN LDA BASIC ;From Merlin?
 CMP #$2D
 BNE :EXIT ;Ignore if not
 LDA BASIC+2 ;Merlin-pro?
 CMP #$E1
 BNE :EXIT ;Ignore if not
 JSR SENDMSG
 HEX 8D8D
 ASC "This destroys current source."8D8D
 ASC "Do you really want it (Y/N)? "00
:GK CALL KEYIN
 AND #%11011111
 CMP #"N"
 BEQ :EXIT
 CMP #"Y"
 BNE :GK
 LDA OBLEN
 STA LEN ;Save for FINISH
 ORA OBLEN+1 ;Object length 0 ?
 BEQ :EXIT ;Leave if so
 MOV OBLEN+1;LEN+1
 MOVD ADRSV;ADRS
 CALL GETENDS ;End of source should have
 CLC ; an $FF if assembly valid
 ADC #1 ;(Note GETENDS leaves Y=0)
 ORA VALID ;Valid assembly done?
 BEQ :OK ;Branch if so
;Fall into :EXIT if not
:EXIT STA AUXWRT ;Must do before XFER call
 CALL RET

* Note Y=0 throughout the main loop:

:OK STA MAINWRT ;Prepare to make hex file
 MOVD SOURCE;ENDSRC
 STORE FRST ;Store CALL-151
:BIGLOOP ADDRESS ADRSV ;Store current obj adrs
 LDA #BYTESPER ;Set # bytes/line
 STA BYTCNT
 LDA #":" ;Colon after adrs
:LOOP JSR PUTSRC ;Put colon or space
 LDA (OBJPNT),Y ;Get object byte
 JSR MAKBYTE ;Put it in source
 INCD OBJPNT ;Point to next obj byte
 DECD OBLEN ;Count down obj length
 BNE :ON ;Taken if OBLEN low not 0
 LDA OBLEN+1
 BEQ :FINISH ;Done when OBLEN reaches 0
:ON LDA #$A0 ;Prepare to store space
 DEC BYTCNT
 BNE :LOOP ;Loop if line not done
 JSR PUTCR ;Store CR
 LDA ADRSV ;Bump address
 CLC
 ADC #BYTESPER
 STA ADRSV
 BCC :BIGLOOP
 INC ADRSV+1
 BCS :BIGLOOP ;Always taken

:FINISH JSR PUTCR ;Put CR
 STORE LAST ;Store BSAVE ,A$
 ADDRESS ADRS ;Store adrs for BSAVE
 STORE LL ;Store ,L$
 ADDRESS LEN ; and length
 JSR PUTCR ; and CR
 LDX #8
 TYA ;=0
:ZT STA TABS,X ;Zero all tabs before
 DEX ; returning to editor
 BPL :ZT
 JMP :EXIT ;All done

FRST HEX 8D
 REV "CALL-151"
FRSTEND

LAST REV "BSAVE ,A$"
LASTEND

LL REV ",L$"
LLEND

PUTCR LDA #$8D
 BNE PUTSRC

MAKADR JSR MAKBYTE
 TXA
MAKBYTE PHA
 LSR ;Shift high nibble to low
 LSR
 LSR
 LSR
 JSR :MAKNIB
 PLA
 AND #$F ;Make low nibble
:MAKNIB ORA #"0"
 CMP #"9"+1
 BLT PUTSRC ;Branch if dec number
 ADC #"A"-":"-1 ;Add A-F offset
PUTSRC STA (ENDSRC),Y
 INCD ENDSRC
 LDA ENDSRC
 CMP HIMEM ;Gone too far?
 LDA ENDSRC+1
 SBC HIMEM+1
 BGE ABORT ;Abort if so
 RTS

ABORT STA AUXWRT ;Must do before calling XFER
 MOVD SOURCE;ENDSRC ;Kill "source"
 CALL MERLIN ;Return to EXEC

 AST 50

SENDMSG PLA ;General msg output routine
 STA TXTP
 PLA
 STA TXTP+1
 BNE :NXT ;Always taken
:PCHR JSR CHROUT
:NXT LDY #0
 INC TXTP
 BNE :NI
 INC TXTP+1
:NI LDA (TXTP),Y
 BNE :PCHR
 LDA TXTP+1
 PHA
 LDA TXTP
 PHA
 RTS

CHROUT CALL COUT
RETURN RTS

CALLMAIN STA AREG
 BIT ZPFLAGA ;Clear or set overflow bit
 CLC ;To main
 STADR CALLER;$3ED
 JMP XFER

ZPFLAGA DFB $40 ;Changed on run

 ERR \$1000 ;Don't encroach on symbol tbl

 ORG ;Recall real adrs for load

HEREND ERR \$9600
