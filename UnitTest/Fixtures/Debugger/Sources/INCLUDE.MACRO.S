* Two-level macro and an included file, Merlin form.
         ORG $300

CLEAR    MAC
         LDA #0
         <<<

STORE    MAC
         CLEAR
         STA ]1
         <<<

START    LDX #3
LOOP     STORE $400
         DEX
         BNE LOOP
         JSR SETUP
         RTS

         PUT INCLUDE.MACRO
