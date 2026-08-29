********************************
*                              *
*          PRINTFILER          *
*                              *
*       Version 1/30/84        *
*                              *
* Glen E. Bredon  Orig 9/11/82 *
*                              *
*------------------------------*
* This uses the $300 and $8A0  *
* space.                       *
*------------------------------*
* If FRMTFLAG ($300) contains  *
* $80 then formatting spaces   *
* are placed in the file.      *
* If it has 0 then the spaces  *
* are packed into a count      *
* (sign bit off) instead.      *
*------------------------------*
* If MONFLAG ($301) is $80     *
* then output is also sent to  *
* video.                       *
*------------------------------*
* CAUTION: This is incompatible*
* with the DSC psuedo-op; it is*
* OK with PUT or SAV.          *
*                              *
********************************

FORMAT KBD ; 1 = format, 0 = pack
 ERR FORMAT/2 ;Require 0 or 1

MONITOR KBD ; 1 to monitor, 0 don't
 ERR MONITOR/2

CH = $24
CSWL = $36
DSKFLAG = $B2 ;Minus if DSK active
VIDVEC = $C0 ;80 column address or COUT1
JUST = $EE
LISTFLAG = $D1
STRINGS = $860 ;Editor puts string here
AMPER = $3F5
ERRCODE = $B5C5
CONNECT = $A851
BYTE = $B5C3
COUT = $FDED
CROUT = $FD8E
BASIC = $E000
DISWARM = $E01B ;Stable address
FILEMGR = $AB06
OPCODE = $B5BB ;File mngr opcode
SUBOP = $B5BC ; " subcode
DOSERR = $B66B

********************************

 ORG $2A0

 LDA BASIC+2 ;Was it BRUN from Merlin?
 EOR #$E1
 BNE EXIT ;Do nothing if not
 LDY #BACK-INIT ;Move the $8A0 stuff
MOVE LDA CODE,Y
 STA INIT,Y
 DEY
 BPL MOVE
 LDA #$4C ;Set up amper vector
 STA AMPER
 LDA #INIT
 STA AMPER+1
 LDA #>INIT
 STA AMPER+2
EXIT RTS

*----------------------------------------
* This code is moved to $8A0 upon a BRUN:

CODE ORG $8A0

INIT LDA BASIC+2 ;Check that this comes
 CMP #$E1 ; from MERLIN
 BNE BACK ;Exit if not
 LDA STRINGS ;String after USER?
 BEQ BACK ;Exit if not
 LDX #-1 ;Save it
MOVNAM INX
 LDA STRINGS,X
 CMP #$E0
 BLT :SF
 AND #%11011111
:SF STA FILENAME,X
 TAY
 BNE MOVNAM ;Ends with a 0
 STY SPACECNT ; and space count
 JSR CONNECT ;Preserves Y
 JSR SENDMSG ;Open the file
 BNE DISCON ;Branch if no params
REST JSR NEXT ;Send params
 BEQ REST ;Loop till got CR
DISCON JSR DISWARM ;Disconnect DOS
 LDA #OUTPUT ;Connect our outhook
 STA CSWL
 LDA #>OUTPUT
 STA CSWL+1
 LDA #" " ;Usual value in editor
 STA JUST
BACK RTS
FILENAME ERR *+$20/$900 ;File name needs 32 bytes

 ORG ;Back to load adrs
 ERR *-$300 ;Must be at page 3

FRMTFLAG DFB FORMAT*$80
MONFLAG DFB MONITOR*$80
SPACECNT BRK ;Count of spaces for packing

GOVID JMP (VIDVEC)

OUTPUT BIT JUST ;Assembling yet?
 BMI GOVID ;(This is + in ASM)
 PHA
 BIT MONFLAG ;Screen output requested?
 BPL PULL ;Branch if not
 JSR GOVID
PULL PLA
 CMP #" " ;A space?
 BEQ SPACE ;Increase count if so
 CMP #$8D ;End of line?
 BEQ SKIP ;Forget spaces if so
 PHA
CHKSPCNT LDA SPACECNT ;Prepare to send space count
 BEQ PULL2 ; if not zero
 BIT FRMTFLAG ;Formatting requested?
 BPL NOFRMT ;Branch to send count if not
 LDA #" "
 JSR SENDBYTE ;Send formatting spaces
 DEC SPACECNT ; that were counted
 BPL CHKSPCNT ;Loop always

NOFRMT JSR SENDBYTE ;Send space count
 LDA #0
 STA SPACECNT ; and reset it.
PULL2 PLA ;Retrieve chr
SKIP BIT LISTFLAG ;If this - then
 BMI ENDASM ; assembly is over
SENDBYTE EOR #$8D ;Is it a CR?
 BNE XOR
 STA CH ;Zero cursor positions
 STA SPACECNT ; and space count
XOR EOR #$8D ;Get back chr
 STA BYTE ;Set up for file mngr
 PHA
 TXA
 PHA
 TYA
 PHA
 JSR OPERATE
 PLA
 TAY
 PLA
 TAX
 PLA
 RTS

SPACE INC SPACECNT ;Count ALL spaces
 RTS ; before acting on them.

ENDASM LDA #$8D ;Append a CR
 JSR SENDBYTE
 JSR XOR ; and eof marker
 JSR DISWARM
 JSR CONNECT ;Now close up
 JSR DOCLOSE
DOCR JMP CROUT

DOCLOSE LDY #CLOSE-OPEN
SENDMSG LDA OPEN,Y
 BEQ SNDNAM
 JSR COUT
 INY
 BNE SENDMSG
SNDNAM TAY
SN LDA FILENAME,Y
 BEQ DOCR
 CMP #"," ;Zero flag will be true
 BEQ RET ; only if have a comma.
NEXT JSR COUT
 INY
 BNE SN
RET RTS

OPEN HEX 8D84
 ASC "OPEN"
 BRK
CLOSE HEX 8D84
 ASC "CLOSE"
 BRK

OPERATE BIT DSKFLAG ;Has DSK been invoked?
 BMI INCOMPAT ;Abort if so
 LDA #4 ;Write opcode
 STA OPCODE
 LDA #1 ;One byte subcode
 STA SUBOP
 JSR FILEMGR
 BCS ERR
 RTS

INCOMPAT LDA #8 ;Send I/O error in lieu
 STA ERRCODE ; of something better
ERR JSR DISWARM ;Must disconnect ourselves or
 JMP DOSERR ; will print DOS error to disk

 ERR *-1/$3D0
