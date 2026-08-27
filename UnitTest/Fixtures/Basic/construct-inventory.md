# The Applesoft construct corpus

`construct-corpus.bas` is Casso's own Applesoft program, written to exercise
every construct the tokenizer has to store rather than the constructs any
particular vendor program happens to use. It replaces the stock master's
`HELLO` as the tokenizer's round-trip corpus, because `HELLO` is Apple's and
does not belong in the repository. Nothing of Apple's is committed here: the
listing is ours, and the tokenized fixture beside it is a *dump of what
Applesoft stored* when the listing was typed into a booted machine, which is
data about our program, produced by running their ROM as the oracle.

## The two files

- `construct-corpus.bas` — the listing, exactly as the scenario suite types it
  into the guest and exactly as the unit suite hands it to
  `ApplesoftTokenizer::Tokenize`.
- `construct-corpus.tok` — the bytes Applesoft itself stored for that listing,
  read back out of guest memory between TXTTAB and VARTAB. Links included,
  based at $0801.

## What checks what

- **Unit suite** (`UnitTest/BasicCorpusTests.cpp`): our tokenizer's output for
  the listing is byte-identical to the fixture; detokenize → retokenize over
  the fixture is the identity; every token byte $80-$EA occurs in the fixture;
  the operand edges below are present.
- **Scenario suite** (`ScenarioTests/BasicCorpusFixtureTests.cpp`): boots the
  stock DOS 3.3 master, types the listing line by line, and asserts the bytes
  Applesoft stored are byte-identical to the committed fixture. This is the
  oracle: a tokenizer checked against its own detokenizer agrees with itself
  perfectly while storing something no guest would recognize.

## THE CIRCULARITY GUARD — read before regenerating

If the tokenizer changes and someone regenerates the fixture from the
tokenizer's own output, the unit-suite check goes circular and asserts
nothing. The fixture may only ever be regenerated from the **guest**: run the
scenario suite (`scripts/RunTests.ps1 -Scenario`), and on a mismatch the
scenario case writes Applesoft's actual bytes to a file in the temp directory
and names the path in its failure message. Copy that file over
`construct-corpus.tok`, rebuild (the unit suite embeds the fixture at build
time), and re-run both suites. Regeneration therefore requires the DOS 3.3
System Master and a booted guest, so it cannot happen by accident, and a
fixture diff is visible in review.

## Construct coverage

Worked from the Applesoft reference and the tokenizer's own table rather than
from what any existing program uses. The unit suite enforces totality
mechanically — every token byte $80-$EA must occur in the fixture — so this
list is the human-readable map, not the gate.

- **Statements**: END, FOR/NEXT (STEP, negative STEP, nested, `NEXT Y,X`),
  DATA, INPUT (bare and prompted), DEL, DIM, READ, GR, TEXT, PR#, IN#, CALL
  (positive and negative), PLOT, HLIN…AT, VLIN…AT, HGR2, HGR, HCOLOR=,
  HPLOT (…TO, and the bare `HPLOT TO` continuation), DRAW…AT, XDRAW…AT, HTAB,
  HOME, ROT=, SCALE=, SHLOAD, TRACE, NOTRACE, NORMAL, INVERSE, FLASH, COLOR=,
  POP, VTAB, HIMEM:, LOMEM:, ONERR GOTO, RESUME, RECALL, STORE, SPEED=, LET
  (and assignment without LET), GOTO, RUN (bare and with a line), IF…THEN
  (line target, statement target, nested IF), RESTORE, & (ampersand hook),
  GOSUB/RETURN, REM, STOP, ON…GOTO, ON…GOSUB, WAIT, LOAD, SAVE, DEF FN / FN,
  POKE, PRINT (`;` and `,` separators, bare, TAB(, SPC(), CONT, LIST, CLEAR,
  GET, NEW.
- **Functions**: SGN, INT, ABS, USR, FRE, SCRN(, PDL, POS, SQR, RND, LOG,
  EXP, COS, SIN, TAN, ATN, PEEK, LEN, STR$, VAL, ASC, CHR$, LEFT$, RIGHT$,
  MID$.
- **Operators**: + - * / ^ AND OR NOT > = < and the two-token pairs >=, <=,
  <>.
- **Operand edges**: line 0 and line 63999; 32767 and -32768; floats with
  leading and trailing points (`.5`, `5.`); scientific notation (`1E10`,
  `2.5E-7`, `3E4`); the empty string; a quote inside an unquoted DATA payload
  and inside a REM; a DATA payload whose quoted string protects a comma and a
  colon; an unterminated string literal; the `?` shorthand for PRINT;
  multi-statement lines throughout; a REM whose text spells keywords without
  tokenizing them; a CTRL-D command string built with CHR$(4); integer (`%`)
  and string (`$`) variables and arrays.

Two constructs are deliberately *not* here, and where they are covered
instead: a literal CTRL-D byte inside a string cannot be committed as
printable text or typed through this corpus, and rides in the scenario
suite's round trip of the master's own greeting; the keyword-collision edges
(`ATN` vs `A TO`, `TOTAL` as TO + TAL, `PR INT` as one token) are pinned
byte-for-byte in `ApplesoftTokenizerTests.cpp` against hand-measured lines.

`construct-corpus.bas` must stay printable ASCII with no trailing spaces on
any line: a DATA payload's trailing space is significant and is exactly what
an editor's trim-on-save would eat, and the unit suite's token sweep leans on
the listing carrying no high-bit bytes.
