# Merlin listing fixture

`PI.ADD.LST` is Merlin Pro 2.23's own assembly listing of `PI.ADD.S`, printed
by the emulated assembler (`PRTR 1`, then `ASM`) and written to this file by
the printer card's text copy (`CASSO_PRINTER_TEXT`). It is Merlin's output,
not this project's: the source it assembles is Glen Bredon's, from the Merlin
distribution disk, under the terms recorded in
`UnitTest/Fixtures/Merlin/LICENSE` (CC BY-NC-ND 3.0), and this listing is
covered the same way. It is a test fixture and is not compiled into or
distributed with any Casso binary.

Nothing here was typed, transcribed or edited. The only changes the text copy
makes to the byte stream are the ones a text file needs: the high bit the
Apple II sets is dropped, a carriage return ends a line, and the line feed the
printer driver adds after it is left out.

It was checked against an independent capture of the same assembly taken at
the character-output routine: of the 232 listing lines the two have in
common, 228 are identical and the other four differ only in trailing spaces,
which the 80-column screen cuts and the printer keeps.
