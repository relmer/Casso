********************************
*                              *
*           KEYMAC             *
*                              *
*   Glen E. Bredon   9/26/84   *
*                              *
*------------------------------*
*   A sample Add/Edit keyboard *
* macro facility.  Also to     *
* illustrate interfacing of    *
* USER routines in general.    *
*------------------------------*
* To use: BRUN KEYMAC  from    *
* EXEC then invoke with USER   *
* from editor.                 *
*                              *
* Keyboard macros are invoked  *
* by pressing one of the Apple *
* keys together with a regular *
* key.                         *
*------------------------------*
*   USER "string" sets up an   *
* AUTOEDIT string which will   *
* be executed on any following *
* EDIT.  (Add/Insert will turn *
* off this mode.)  In the edit *
* string, a ^R followed by     *
* D, >, or <, will give DEL,   *
* or right/left cursor moves.  *
********************************

 TR
 EXP OFF
YES = 1
NO = 0

SAVOBJ KBD "Save object code? (YES/NO)"

* Merlin & Apple stuff:

FROM = $3C ;A1L
ENDMOVE = $3E ;A2L
DEST = $42 ;A4L
INVEC = $AE ;Edit mode key input vector
EFLG = $F0 ;Minus when in edit mode
AMPER = $3F5
STRINGS = $860 ;Editor puts string here
KEY = $C000
STROBE = $C010
MONMOVE = $FE2C
AUXMOVE = $C311
XFER = $C314
MAINWRT = $C004
AUXWRT = $C005
READZP = $C016
OPENAPP = $C061 ;Open apple key
CLSDAPP = $C062 ;Closed apple key
 PAG
* Usage by this program:

STATE = $90
YSAV = $91
MACPNT = $92
INDEX = $94

********************************

INCD MAC
 INC ]1
 BNE NI
 INC ]1+1
NI <<<

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

TOMAIN MAC
 PHA
 BIT ZPFLAGA
 CLC
 STADR ]1;$3ED
 PLA
 JMP XFER
 <<<

********************************

* Setup routines upon BRUN:

 ORG $9000 ;Load address

 LDA $E002
 CMP #$E1 ;Merlin-pro?
 BNE :RET ;Ignore if not
 LDA #$4C ;Set up USER vector
 STA AMPER
 STADR START;AMPER+1
 STADR HEREINT;FROM
 STADR HEREADR-1;ENDMOVE
 STADR START;DEST
 LDY #0
 JSR MONMOVE ;Move interface to page 3
 STADR HEREADR;FROM
 STADR HEREND;ENDMOVE
 STADR $900;DEST
 SEC ;MAIN->AUX
 JSR AUXMOVE ;Move main prog to aux mem
; and return to caller
 LDA READZP ;Aux or main version?
 LSR
 STA ZPFLAGM
 STA AUXWRT
 STA ZPFLAGA
 STA MAINWRT
:RET RTS

*---------------------------------------
* Interface to go on page 3 main memory:
*---------------------------------------

HEREINT ORG $300

START STADR INPUT;INVEC
 LDY #-1
 STA AUXWRT
NXB INY ;Gather string
 LDA STRINGS,Y ; in USER command
 STA AUTOCMD,Y ;(Max length = $20)
 BNE NXB
 STA STATE ;Init STATE
 STA INDEX ;Init index to cmd
 STA MAINWRT
 RTS

INPUT TOAUX AUXINPUT ;Keyget entry

EXIT LDY YSAV ;Keyget exit
 RTS

ZPFLAGM DFB $40 ;Changed on run

 ERR *-1/$3D0

 ORG ;Recall present adrs in
; loaded program (must do
; before next ORG)

*---------------------------------------------
* Main program and mac table to go in aux mem:
*---------------------------------------------

HEREADR ORG $900

* New edit mode key get routine (entry point):

AUXINPUT STY YSAV ;Must preserve Y (& X)
 LDY AUTOCMD ; if used.
 BNE AUTOEDIT ;Branch if an autoedit
 BIT STATE
 BMI GETNXT ;Branch if doing macro
 JSR GETKEY ;Otherwise, get key
 PHA
 LDA OPENAPP ;One of the Apple
 ORA CLSDAPP ; keys pressed?
 ASL ;Answer to carry
 PLA
 BCC GOEXIT ;Branch if not
 STA ENDTBL ;In case cmd not found
 AND #$7F ; save current input
 STA ENDTBL+1 ; to use for it
 STADR MACTBL;MACPNT
CMD? LDA (MACPNT),Y ;Look for command
 CMP ENDTBL ;Match input?
 BEQ GETNXT ;Branch if so
GETCHR INCD MACPNT ;Move to next mac
 LDA (MACPNT),Y
 BMI GETCHR ;Loop till plus
 INCD MACPNT ;Point to cmd byte
 BNE CMD? ;Always taken

GETNXT INCD MACPNT ;Point to macro selected
; or to next chr
 LDA (MACPNT),Y ;Get current chr of macro.
 STA STATE ;Clear flag on last chr
 ORA #$80 ;Must always set high bit
GOEXIT TOMAIN EXIT ;Leave via interface

GETKEY LDA KEY
 BPL GETKEY
 BIT STROBE
 RTS

AUTOEDIT BIT EFLG ;From Add/Insert or edit?
 BMI YINDX ;Branch if from edit
 LDA #0 ;Turn off auto if
 STA STATE ; from Add/Insert
 STA AUTOCMD
 JSR GETKEY
 JMP GOEXIT

YINDX LDY INDEX ;Get index to next cmd chr
 INC INDEX ;Point to next one
 LDA AUTOCMD,Y ;Get next cmd chr
 BNE ISQ ;Branch if not last one
 STA INDEX ;Init index for next line
 LDA #$8D ;Exit with CR
ISQ CMP #"Q"&$9F ;Control Q?
 BNE ISR
 LDY #0 ;Set to process next line
 STY INDEX ; from start of macro
ISR CMP #"R"&$9F ;Control R lead in?
 BNE DOCMD ;Branch if not
 INC INDEX ;Bump index
 LDA AUTOCMD+1,Y
 CMP #"D"
 BNE LAR
 LDA #$FF ;Substitute del
LAR CMP #"<" ;Next chr a <
 BNE RAR
 LDA #"H"&$9F ;Translate to backspace
RAR CMP #">" ;or >
 BNE DOCMD ;Just ignore cntr R if not
 LDA #"U"&$9F ;Translate to cntr U
DOCMD JMP GOEXIT

ZPFLAGA DFB $40 ;Changed on run
MACTBL
 DCI "4 = $"
 DCI "5 DFB %"
 ASC !7" "&$9F!
 DFB $9F&"N",$88,$88,$88,$88,$88,$08
 DCI "\ ERR \"
 ASC "8*------------------------------"0D
 DCI ". >>> "
 ASC ", <<<"0D
 DFB $9F&"K" ;Up arrow
 ASC " PHA"8D
 ASC " TXA"8D
 ASC " PHA"8D
 ASC " TYA"8D
 ASC " PHA"0D
 DFB $9F&"J" ;Down arrow
 ASC " PLA"8D
 ASC " TAY"8D
 ASC " PLA"8D
 ASC " TAX"8D
 ASC " PLA"0D
 ASC !" ASC ""!
 DFB $88,$1F&"I" ;Backspace & insert
 ASC "' ASC ''"
 DFB $88,$1F&"I"
 DCI "3 LDA #"
 DCI "X LDX #"
 DCI "Y LDY #"
 DCI "D DFB "
 DCI "H HEX "
 DCI "= ADC "
 DCI "L LDA "
 DCI "S STA "
 DCI "- SBC "
 DCI "O ORA "
 DCI "C CMP "
 DCI "A AND "
 DCI "E EOR "
 ASC "R RTS"0D
 DCI "J JSR "
 DCI "K JMP "
 DCI "B BVC "
 DCI "V BVS "
 DCI "P BPL "
 DCI "M BMI "
 DCI "N BNE "
 DCI "Q BEQ "
 ASC "Z LD  #0"88888808
 ASC "9(),Y"
 HEX 888888 ;Backspace to between ()
 DFB $1F&"I" ;Exit in insert mode
 ASC "0(,X)"
 HEX 888888 ;Backspace to between ()
 DFB $1F&"I" ;Exit in insert mode

ENDTBL DA 0 ;Command chr put here

AUTOCMD DS $20

 ERR *-1/$1000

 ORG ;Recall real adrs for load

HEREND ERR *-1/$9853

 DO SAVOBJ
 SAVE KEYMAC
 FIN
