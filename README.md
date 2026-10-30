# traymd-omarchy

Quick Markdown notes for Omarchy. Open a small popup, jot something down and
get back to what you were doing. Notes save automatically, Markdown renders as
you type, and colours follow your theme.

![TrayMD Omarchy showing headings, emphasis, a list, a code block and a link](assets/screenshot.png)

## Installation

For the Quickshell-based Omarchy desktop:

```bash
omarchy plugin add https://github.com/rabfulton/traymd-omarchy.git --yes
bash ~/.config/omarchy/plugins/traymd-omarchy/install.sh
```

The installer adds the required packages through Omarchy, builds the plugin,
enables its notes icon and restarts the shell.

## Usage

Click the notes icon to open or close the popup. It stays above normal windows
until dismissed with Escape, Ctrl+W or the bar icon. Type normally: headings,
emphasis, lists and code render in place. **Ctrl+click** a link to open it in
your default browser.

Enter continues a list; Enter on an empty item ends it. The right-click menu
includes undo, redo, cut, copy, paste and select all.

## Shortcuts

Defaults while the popup is focused:

| Shortcut | Action |
| --- | --- |
| Alt+Left / Alt+Right | Older / newer note |
| Ctrl+N | New note |
| Escape or Ctrl+W | Close |
| Ctrl+, | Settings |
| Ctrl+S | Save immediately |
| Ctrl+Shift+Delete | Delete, after confirmation |

Edit `~/.config/traymd-omarchy/config.ini` to change these bindings. The file
is created automatically on first use, and changes apply without a restart.
For example:

```ini
[shortcuts]
older_note=Ctrl+Shift+H
newer_note=Ctrl+Shift+L
```

Other keys are `new_note`, `close`, `settings`, `save` and `delete_note`.
An empty value disables a binding. Escape always remains available to cancel
or close. Invalid or duplicate bindings leave the last working settings active.

For a **global** show/hide shortcut, use **Settings → Shortcut → Set**, then
press a Super key combination. Existing shortcuts are checked before saving.

## Files

| Location | Contents |
| --- | --- |
| `~/.local/share/traymd/notes/` | Markdown notes, compatible with XFCE TrayMD |
| `~/.local/share/traymd/notes/.trash/` | Recoverable deleted notes |
| `~/.config/traymd-omarchy/config.ini` | In-app shortcuts |
| `~/.config/traymd-omarchy/notes.ini` | Last selected note |
| `~/.config/omarchy/shell.json` | Popup size and placement, in the plugin's bar entry |
| `~/.config/hypr/bindings.lua` | Global shortcut, in a marked TrayMD block |

Custom `XDG_CONFIG_HOME` and `XDG_DATA_HOME` locations are respected for the
app config and notes. A custom notes folder can be set in Omarchy's bar settings.

## Appearance

On first use, the popup takes half the monitor height, with width = height ÷
1.25, adjusted to fit small screens.
Fonts follow Omarchy's theme and display scaling. Use the gear button to
change the size or choose between the screen centre and the bar icon.

## Updating and removal

```bash
omarchy plugin update traymd-omarchy
bash ~/.config/omarchy/plugins/traymd-omarchy/install.sh
```

For an existing local checkout, run `./install.sh` there.
Remove with `omarchy plugin remove traymd-omarchy`; your notes are kept.

## Development and license

See [development notes](docs/DEVELOPMENT.md) for building and tests.
[MIT licensed](LICENSE).
