# Contributing to Cwitcher

Thanks for taking the time to contribute.

## Before you start

- Please check existing issues before opening a new one.
- For larger changes, open an issue first so the direction can be discussed before implementation.
- Keep changes focused. Small, well-scoped pull requests are easier to review and test.

## Development setup

### Requirements

- Windows 10 or Windows 11
- Visual Studio 2022 with `Desktop development with C++`
- Windows SDK

### Build

Open `Cwitcher.sln` in Visual Studio and build `Debug x64` or `Release x64`.

Or use:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

## Project layout

- `src/main.c` - app startup and global state
- `src/ui.c` - tray icon, menu, settings window
- `src/input.c` - keyboard and mouse hooks, hotkey routing
- `src/transform.c` - layout mapping and text transformation
- `src/settings.c` - settings, logging, sound
- `src/app.h` - shared declarations

## Coding guidelines

- Prefer small, readable functions.
- Keep Win32-specific behavior explicit and well-scoped.
- Avoid unrelated refactors in the same change.
- Preserve existing behavior unless the change explicitly targets that behavior.
- Add comments only when they clarify non-obvious logic.

## Testing checklist

Before opening a pull request, please manually test the relevant scenarios:

- tray startup and exit
- selected text conversion
- last word conversion
- layout cycling
- settings persistence
- at least one target app that previously worked

If your change affects app-specific behavior, mention which apps you tested.

## Pull requests

Please include:

- a short summary of the change
- why the change is needed
- testing notes
- screenshots or short recordings if UI behavior changed

## Security

For security-sensitive issues, please follow [SECURITY.md](./SECURITY.md) instead of opening a public bug report first.
