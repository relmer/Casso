## VT323 font (Retro Terminal)

`VT323-Regular.ttf` is a vendored placeholder (0 bytes) in this commit.
ThemeManager falls back to Consolas at runtime when the file is empty.

To drop in the real font:

    1. Download VT323 from Google Fonts:
       https://fonts.google.com/specimen/VT323
    2. Copy `VT323-Regular.ttf` over the placeholder in this folder.
    3. Rebuild — the .rc embeds the bytes into Casso.exe; on the next
       launch AssetBootstrap::EnsureThemes extracts the real font.

License: SIL Open Font License 1.1 — see OFL.txt.
