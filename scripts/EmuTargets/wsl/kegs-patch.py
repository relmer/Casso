"""Two changes to KEGS's source, for KEGS and for GSplus, which is built on it.

A raw outgoing socket passes $00 bytes to the 8530; stock KEGS drops them,
which suits a terminal and breaks a binary link. A failed boot from slot 7
falls back to slot 6, where stock KEGS boots slot 5 and skips slot 6.

Usage: kegs-patch.py <KEGS src folder>
"""

import sys

src = sys.argv[1]

edits = [
    ("scc_socket_driver.c", "if(tmp_buf[i] == 0) {", "if(tmp_buf[i] == 0 && g_scc[port].cur_state != 2) {"),
    ("smartport.c",         "engine.kpc = 0xc500;\t\t// Try to boot slot 5", "engine.kpc = 0xc600;\t\t// Try to boot slot 6, not 5"),
]

for name, old, new in edits:
    path = f"{src}/{name}"
    text = open(path).read()

    if new in text:
        print(f"{name}: already changed")
        continue

    if old not in text:
        sys.exit(f"{name}: '{old}' isn't there")

    open(path, "w").write(text.replace(old, new, 1))
    print(f"{name}: changed")
