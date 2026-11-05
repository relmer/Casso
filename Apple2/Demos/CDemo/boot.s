;
;  boot.s -- the entry point of the direct-boot C demo
;
;  The disk has no DOS, and the machine boots it before Applesoft starts, so
;  the zero page and the DOS vectors contain whatever memory had at power-on.
;  cc65's startup code reads three of them:
;
;      $BF00    a JMP here indicates ProDOS
;      HIMEM    the top of the C stack when ProDOS is absent
;      DOSWARM  the jump taken when the program exits, or on Reset
;
;  The code below sets all three and then falls through into STARTUP, which
;  cdemo.cfg places right after this segment.
;

        .include        "apple2.inc"

        .import         __HIMEM__               ; from cdemo.cfg

MONZ    :=      $FF69                           ; the Monitor's * prompt

        .segment        "BOOT"

        stz     $BF00                           ; no ProDOS
        lda     #<__HIMEM__
        sta     HIMEM
        lda     #>__HIMEM__
        sta     HIMEM+1
        lda     #$4C                            ; JMP MONZ
        sta     DOSWARM
        lda     #<MONZ
        sta     DOSWARM+1
        lda     #>MONZ
        sta     DOSWARM+2
