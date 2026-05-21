## Inter font (Skeuomorphic + Dark Modern)

`Inter-Regular.ttf` is a vendored placeholder (0 bytes) in this commit.
ThemeManager falls back to Segoe UI at runtime when the file is empty.

To drop in the real font:

    1. Download Inter from https://github.com/rsms/inter/releases
    2. Copy `Inter-Regular.ttf` over `Inter-Regular.ttf` in this folder.
    3. Rebuild — the .rc embeds the bytes into Casso.exe; on the next
       launch AssetBootstrap::EnsureThemes extracts the real font.

License: SIL Open Font License 1.1 — see OFL.txt (which DOES ship the
real text even though the .ttf is a placeholder, so the upgrade is
just a file-drop).
