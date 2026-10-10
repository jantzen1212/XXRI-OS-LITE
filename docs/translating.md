# Translating XXRI OS Lite

XXRI's GTK programs use GNU gettext. English is the source language and the
default, and no translation ships yet. A translation is one `.po` file per
program; a build does not change until one exists.

## What can be translated

| Program | Domain (catalog name) | Messages | Template |
|---|---|---|---|
| Settings | `xxri-settings` | 320 | `src/xxri-settings/po/xxri-settings.pot` |
| Store | `xxri-store` | 145 | `src/xxri-store/po/xxri-store.pot` |
| Installer (and the `xxri-dialog` helper) | `xxri-installer` | 67 | `src/xxri-installer/po/xxri-installer.pot` |
| Media player | `xxri-media` | 19 | `src/xxri-media/po/xxri-media.pot` |
| Control Center | `xxri-control-center` | 10 | `src/xxri-control-center/po/xxri-control-center.pot` |
| Power menu | `xxri-power-menu` | 10 | `src/xxri-power-menu/po/xxri-power-menu.pot` |
| Launcher | `xxri-launcher` | 4 | `src/xxri-launcher/po/xxri-launcher.pot` |

Not covered:

* XXRI File (a PCManFM fork) keeps PCManFM's upstream `po/` but is built with
  `--disable-nls`. XXRI Browser (Qt WebEngine) would use Qt's `.qm` files.
* Text from the shell backends (`xxri-network`, `xxri-hardware`, ...) that
  Settings displays, such as a Wi-Fi error reason, stays English.
* Application names and descriptions in `.desktop` files and the Store catalog
  come from their publishers.
* GTK's own strings (the `gtk30` domain) come from Tiny Core's GTK package.

## Contributing a translation

You need GNU gettext (`xgettext`, `msginit`, `msgmerge`, `msgfmt`, `msggrep`),
bash 4 or newer (macOS: `brew install bash`) and a clone of this repository.
Run the commands from its top directory.

```sh
tools/xxri-i18n new pt_PT                 # po/pt_PT.po for every program
tools/xxri-i18n new pt_PT xxri-power-menu # ...or for one program
```

The code is a glibc locale name without the charset (`pt_PT`, `de`, `es_MX`).
`pt-PT` is accepted and becomes `pt_PT`: the file name is the directory gettext
looks up at run time. Each program gets `src/<program>/po/<code>.po`, to be
filled in with any PO editor. Translating a single program is a fine
contribution.

Rules for translators:

* Keep `%s`, `%d`, `%ld`, `%%` and every other `%` placeholder, and the `\n`
  line breaks. When the target language needs another word order, use numbered
  placeholders: `"%2$s depois de %1$s"`.
* Plural messages (`%d app` / `%d apps`) get as many forms as the `Plural-Forms`
  header says; `msginit` sets it for the language.
* The Installer's welcome line contains Pango markup
  (`<span ...>XXRI OS Lite</span>`): keep the tags.
* Some messages are fragments the program joins, such as `" · Default"` or
  `" and "`. Translate them as they stand, and open an issue if a language
  cannot be expressed that way.
* An empty message keeps the English text. A `fuzzy` one is a `msgmerge` guess
  and is not used until you review it and remove the flag.
* Fill in `Last-Translator` and `Language-Team`, and keep the
  `charset=UTF-8` line.

Check your work:

```sh
tools/xxri-i18n check      # templates current, every .po valid (msgfmt -c)
tools/xxri-i18n stats      # translated / fuzzy / untranslated per file
```

To try it on a running system, compile the catalog and start the program in
that language:

```sh
msgfmt -c -o xxri-settings.mo src/xxri-settings/po/pt_PT.po
sudo install -D xxri-settings.mo /usr/local/share/locale/pt_PT/LC_MESSAGES/xxri-settings.mo
LANG=pt_PT.UTF-8 xxri-settings
```

The system also needs the locale (next section). Send the `.po` files as a pull
request, not the `.mo` files.

## Keeping a translation up to date

After code changes the maintainers refresh the templates with
`tools/xxri-i18n pot`; a translator then merges them into the existing files:

```sh
tools/xxri-i18n update pt_PT   # msgmerge: new messages empty, changed ones fuzzy
```

Then review the fuzzy and empty messages; `stats` counts them.

## Locales: what a language needs besides the catalog

A catalog is only used if the system has that language's locale. Tiny Core's
base has none, so programs only had the POSIX locale; `xxri-locales` compiles
UTF-8 locales with the toolchain's `localedef` into `/usr/lib/locale`. The list
is `XXRI_LOCALES_LIST` in `ptxdist/rules/xxri-locales.make`: `en_US`, `en_GB`,
`id_ID`, `pt_PT`. To add a language, add its glibc locale name there. Files that
locales share, such as collation, are stored once.

Selecting the language:

| How | What |
|---|---|
| Boot code `lang=pt_PT` | Replaces the stored setting. `pt_PT`, `pt_PT.utf8` and `pt_PT.UTF-8` mean the same. `lang=C` is the POSIX locale. |
| `xxri-desktop language list` | The locales installed. |
| `xxri-desktop language set pt_PT [LC_TIME=en_US ...]` | Stored in `/etc/sysconfig/language` (`LANG=` and any `LC_*` category, `LC_ALL` included), used by sessions started afterwards. |
| (nothing set) | `en_US.UTF-8`. |

`xxri-config` reads the setting at boot, writes it back normalised and warns if
the locale is not installed (programs then run in `C`).
`/etc/profile.d/xxri-locale.sh` exports the `LC_*` variables to the session.

### Keyboard layout

X has its own layout, separate from the console keymap `kmap=`, and defaults to
US, which has no `ç`, `º` or `ª` keys. Set it with the boot code `xkb=pt`
(`xkb=LAYOUT:VARIANT`) or `xxri-input set-xkb pt [VARIANT]`. It is saved in
`/etc/sysconfig/xkb`, written to `/etc/X11/xorg.conf.d/00-xxri-keyboard.conf`
and read when X next starts. Layout names are the files in
`/usr/local/share/X11/xkb/symbols`.

Dead keys (`~` then `a` gives `ã`) work in every locale. In a Portuguese locale
GTK also loads its cedilla input method, so `´` then `c` gives `ç` (`ć` in other
locales).

### File names

Tiny Core's `/etc/profile` exports `G_FILENAME_ENCODING=iso8859-1`, so GLib reads
UTF-8 file names as Latin-1 (`ção.txt` shows as `Ã§Ã£o.txt`) and stores a typed
`ç` as one non-UTF-8 byte. `xxri-locale.sh` sets it to UTF-8 for the session.

## For developers

Each GTK program has an identical copy of `xxri-i18n.h`, like `xxri-theme.h`
(`tools/xxri-i18n check` compares them):

```c
#define GETTEXT_PACKAGE "xxri-settings"   /* the program's name = its catalog */
#include "xxri-i18n.h"
...
int main(...) { xxri_i18n_init(); ... }   /* setlocale, bindtextdomain, UTF-8 */
```

* `_("text")` where a string is shown; `N_("text")` in static tables, with
  `_()` applied where the entry is used. `C_("context", "text")` separates
  identical English words, `ngettext(one, other, n)` handles counts, and a
  `/* TRANSLATORS: ... */` comment before a string reaches the catalog.
* Mark only text meant for people, not identifiers, CSS classes, commands, JSON
  keys, paths or values compared with `strcmp`. Compare the msgid and translate
  at display time, as `cat_title()` does in the Store.
* Prefer whole sentences with `%s` to fragments joined in code.
* After changing strings run `tools/xxri-i18n pot`, then `check`.

A new program needs its source files in `src/<program>/po/POTFILES.in`, a copy
of `xxri-i18n.h`, an entry in the `DIR` table of `tools/xxri-i18n`, and in its
PTXdist rule `xxri/mo-compile` after the compile step and `xxri/mo-install` in
`targetinstall` (see `xxri-settings.make`). `msgfmt -c` compiles the `.po` files
to `/usr/local/share/locale/<lang>/LC_MESSAGES/<program>.mo`. PTXdist does not
watch the sources, so a changed `.po` needs `ptxdist clean <program>`.

## Known limits

* Right-to-left languages are untested.
* The language is chosen with the boot code or `xxri-desktop language set`;
  Settings shows it but has no picker. The Installer's language list only sets
  the installed system's language.
