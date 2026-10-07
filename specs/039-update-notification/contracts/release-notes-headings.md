# Contract: release-note headings (consumed)

CHANGELOG version section:

```text
## [1.30.0] - 2026-10-03: The one with 1.21 gigawatts of WOZ 2.1 flux support
```

Pattern `^## \[(\d+)\.(\d+)\.(\d+)\]`; body runs to the next `^## `.
`## [Unreleased]` does not match and is never shown.

README release highlight:

```text
### [2026-10-03 · 1.30] WOZ 2.1 flux support—great Scott!
```

Pattern `^### \[[^\]]*\x{B7}\s*(\d+)\.(\d+)\]`; body runs to the next heading of
level 1 to 3. The heading text after `]` is shown as the highlight title.

Markdown subset rendered: `#`-`####` headings, `-`/`*` bullets with nesting by
indent, blank-line paragraphs, `**bold**`, `` `code` ``, `[text](url)` links.
Anything else is shown as plain text, never dropped, with one exception:
inline HTML tags and comments (`<a id="v1-29"></a>`, `<!-- ... -->`) are
dropped, since they draw nothing in a rendered README. Text between two tags
is kept. A `<` that does not open a tag or comment (`a < b`) stays as text,
and so does anything inside a code span.

A CHANGELOG section's heading is shown without the link brackets around its
version: `## [1.30.0] - 2026-10-03: Title` reads `1.30.0 - 2026-10-03: Title`.

Images: markdown ![alt](src), an image inside a link ([![alt](src)](href),
shown as the image alone), and HTML <img src="..." alt="..." width="..">
each become an image block. Only a percent width is kept (a percent of the
body width). In the README's two-column tables, each cell's <sub>...</sub>
after its image is that image's caption, and the cells flatten to one image
after another; a <sub> with no image before it in the cell stays text. A
line of nothing but HTML structure (<table>, <tr>, </td>) draws
nothing.

A relative src resolves against
https://raw.githubusercontent.com/relmer/Casso/<tag>/; an https URL is
fetched as written; anything else (plain http, data:, other schemes,
//host) is not fetched, and its alt text shows instead. Images are fetched
after the notes, at most 8 MB each, decoded with WIC, cached for the session,
and canceled when the dialog closes.

The running version's release date is read from its own heading in the new
tag's CHANGELOG (## [1.29.0] - 2026-09-20...) and gives the age the update
dialog's remark can use.
