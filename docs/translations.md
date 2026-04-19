# Translations

TilePeek uses Qt's standard internationalization pipeline. Translatable
strings live in the source code wrapped in `tr()` calls; `lupdate` extracts
them into human-editable `.ts` files under `translations/`; `lrelease`
compiles those into binary `.qm` files which the build embeds into the
executable's Qt resource system under `:/i18n/`.

At startup the app loads two translators: one for Qt's own strings (file
dialog buttons, etc.) and one for TilePeek's strings. Both follow the user's
system locale via `QLocale::system()`, with the standard Qt fallback chain
(`fr_CA` → `fr_FR` → `fr`).

## Contents of the `translations/` directory

- `tilepeek_<locale>.ts` — human-editable XML. One file per language. Edit
  with Qt Linguist or any text editor.
- Compiled `.qm` files land in the build directory and are embedded into
  the executable. They are never checked in.

## Adding a new language

1. Add the new `.ts` path to the `qt_add_translations(...)` call in
   `src/CMakeLists.txt`. Example for German:

   ```cmake
   qt_add_translations(tilepeek
       TS_FILES ${CMAKE_SOURCE_DIR}/translations/tilepeek_fr.ts
                ${CMAKE_SOURCE_DIR}/translations/tilepeek_de.ts
       SOURCE_TARGETS tilepeek_lib
       RESOURCE_PREFIX "/i18n"
       LUPDATE_OPTIONS -no-obsolete -locations relative
   )
   ```

2. Re-configure CMake: `cmake -B build -S .`.
3. Populate the new file with source strings:
   `cmake --build build --target update_translations`. This creates the new
   `tilepeek_de.ts` and fills it with every `tr(...)` source string from
   `tilepeek_lib`, each marked `type="unfinished"`.
4. Translate it (see below).
5. Build normally. `lrelease` compiles `.qm` files automatically.

## Editing a `.ts` file

`.ts` files are XML. Each `<message>` pairs a `<source>` (extracted from the
code — never edit this) with a `<translation>` (what you write).

Two ways to edit:

- **Qt Linguist** (`linguist-qt6` on Fedora, or `Qt Linguist.app` on macOS):
  a GUI editor shipped with Qt's development tools. Recommended for
  non-programmers — previews mnemonics, flags missing placeholders, groups
  messages by context, and shows translator comments (`<extracomment>`).
- **Plain text editor**: works fine for small edits. The format is
  straightforward.

### Rules for translators

- Do not edit `<source>` elements.
- Remove the `type="unfinished"` attribute from each `<translation>` you
  finish. If left, Qt falls back to the English source at runtime.
- **Preserve placeholders**: `%1`, `%2`, etc. must appear in your
  translation. They can be reordered — for example, `"Zoom level %1"` →
  `"%1 de zoom"` is valid, but dropping `%1` is not. The number next to
  `%` tells Qt which argument fills each slot.
- **Preserve HTML markup and URLs**: `<a href="...">`, `<b>`, `<br>` tags
  must appear verbatim. Translate the human text between tags, not the
  URLs, email addresses, or HTML attribute values.
- **Preserve literal `\n` newlines** when the `<extracomment>` says so. In
  `.ts` XML this appears as an actual line break inside `<source>`, not
  as the two characters `\n`.
- **Ampersand mnemonics**: menu items use `&Word` to mark the underlined
  access key. Choose a mnemonic letter that's not already used by another
  item in the same menu. To produce a literal `&` character in the UI,
  write `&&`.

### The "TilePeek" brand name

Every `"TilePeek"` occurrence in the code is wrapped in `tr()` specifically
to let translators for non-Latin scripts (CJK, Cyrillic, Arabic, etc.)
transliterate the brand into their script if they choose to. For
Latin-script translations (French, Spanish, German, etc.), translate
`"TilePeek"` as `"TilePeek"` unchanged — but do remove
`type="unfinished"` so Qt Linguist stops flagging it.

## Developer guidelines — adding new translatable strings

- Inside a class that inherits `QObject` (which includes anything with the
  `Q_OBJECT` macro, like every widget and dialog): just call `tr("...")`.
- Inside a plain class that does not derive from `QObject` (for example
  the metadata parser classes): add `Q_DECLARE_TR_FUNCTIONS(ClassName)`
  inside the class body, then call `tr("...")` as usual. The macro injects
  a static `tr()` method scoped to the class.
- For free functions that can't easily be moved into a class, use
  `QCoreApplication::translate("ContextName", "...")`. Prefer the
  class-based forms above — they're less error-prone than stringly-typed
  contexts.
- Never concatenate translated fragments: `tr("foo") + x + tr("bar")` is
  wrong because it prevents translators from reordering the phrase. Use
  positional placeholders: `tr("foo %1 bar").arg(x)`.
- When a `tr()` call's meaning isn't obvious from the source string alone
  (e.g. ambiguous word, placeholder whose meaning isn't self-evident,
  HTML markup that must be preserved), add a translator comment on the
  line directly above:

  ```cpp
  //: Tile-size statistics tooltip. %1 is the zoom level, %2..%6 are sizes.
  QString tip = tr("Zoom %1 — %2 tiles\np50: %3 | ...");
  ```

  `lupdate` extracts `//:` lines as `<extracomment>` entries visible in
  Qt Linguist.

## Refreshing translations after changing source code

After adding, removing, or changing any `tr(...)` call, regenerate the
`.ts` files:

```
cmake --build build --target update_translations
```

This invokes `lupdate`, which:

- Adds new source strings as `type="unfinished"` entries.
- Removes strings that no longer appear in the code (the build's
  `LUPDATE_OPTIONS -no-obsolete` enforces this; without it old strings
  accumulate).
- Leaves existing translations intact.

Then translate any newly added `type="unfinished"` entries and commit the
updated `.ts` file.

## Testing a translation

The app loads the system locale automatically. Ways to trigger a specific
locale:

- **Linux**: `LANG=fr_FR.UTF-8 ./build/src/tilepeek` (or `LC_ALL=fr_FR.UTF-8`
  to override all LC_* variables at once).
- **macOS**: `./TilePeek.app/Contents/MacOS/TilePeek -AppleLanguages '(fr)'`
  (Apple's override syntax; otherwise set system language in System
  Settings).
- **Any platform, ad-hoc**: set `QT_LOCALE=fr_FR` before launch; Qt's
  `QLocale::system()` respects it.
- **During development**: temporarily call
  `QLocale::setDefault(QLocale(QLocale::French, QLocale::France))` in
  `main()` before the translators are installed.
