# KeyboardSwitcherC

Native Windows tray utility in plain C for keyboard layout conversion.

## English

### Current features

- hidden Win32 application with tray icon and no main window
- tray badge with current layout code: `EN`, `RU`, `UK`, `??`
- selected-text conversion by hotkey
- last typed word conversion by hotkey
- layout switching by the current Windows input-language order
- fallback internal order if the system order cannot be resolved
- settings window from tray menu
- configurable hotkeys for:
  - selected text conversion
  - last word conversion
- optional switch sound
- optional debug log
- settings saved to `%APPDATA%\KeyboardSwitcherC\settings.ini`

### Supported layout mappings

Current key-position mapping tables are implemented for:

- `en`
- `en-gb`
- `ru`
- `uk`

### Important behavior

- Text conversion is based on key-position mapping, not dictionary translation.
- The app uses the Windows list of installed input languages and follows the same effective cycle as system layout switching.
- If the Windows order cannot be used, the app falls back to an internal cycle.
- Only layouts with implemented mapping tables can participate in text conversion.

### Build

Open the project in Visual Studio:

- `D:\Projects\KeyboardSwitcherC\KeyboardSwitcherC.vcxproj`

Or build from PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

### Output binaries

- Debug: `D:\Projects\KeyboardSwitcherC\bin\Debug\KeyboardSwitcherC.exe`
- Release: `D:\Projects\KeyboardSwitcherC\bin\Release\KeyboardSwitcherC.exe`

### Project structure

- `src\main.c` - app bootstrap and global state
- `src\ui.c` - tray UI and settings window
- `src\input.c` - keyboard and mouse hooks, hotkey routing, last-word tracking
- `src\transform.c` - layout detection, conversion, clipboard flow
- `src\settings.c` - settings, logging, sound, hotkey formatting
- `src\app.h` - shared declarations

---

## Українська

### Поточні можливості

- прихована Win32-програма з іконкою в треї без головного вікна
- бейдж у треї з поточною розкладкою: `EN`, `RU`, `UK`, `??`
- перетворення виділеного тексту по гарячій клавіші
- перетворення останнього набраного слова по гарячій клавіші
- перемикання розкладки за поточним порядком мов введення Windows
- резервний внутрішній порядок, якщо системний порядок визначити не вдалося
- вікно налаштувань із меню трея
- налаштовувані гарячі клавіші для:
  - перетворення виділеного тексту
  - перетворення останнього слова
- опційний звук перемикання
- опційний debug-log
- налаштування зберігаються в `%APPDATA%\KeyboardSwitcherC\settings.ini`

### Підтримувані мапінги розкладок

Зараз таблиці мапінгу позицій клавіш реалізовані для:

- `en`
- `en-gb`
- `ru`
- `uk`

### Важлива поведінка

- Перетворення тексту виконується через мапінг клавіш, а не словниковий переклад.
- Програма використовує список встановлених мов введення Windows і намагається йти в тому самому ефективному циклі, що й системне перемикання розкладок.
- Якщо використати системний порядок не виходить, застосунок переходить на внутрішній резервний цикл.
- У перетворенні тексту беруть участь лише ті розкладки, для яких уже є таблиці мапінгу.

### Збірка

Відкрити проєкт у Visual Studio:

- `D:\Projects\KeyboardSwitcherC\KeyboardSwitcherC.vcxproj`

Або зібрати через PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

### Готові збірки

- Debug: `D:\Projects\KeyboardSwitcherC\bin\Debug\KeyboardSwitcherC.exe`
- Release: `D:\Projects\KeyboardSwitcherC\bin\Release\KeyboardSwitcherC.exe`

### Структура проєкту

- `src\main.c` - старт застосунку та глобальний стан
- `src\ui.c` - трей-інтерфейс і вікно налаштувань
- `src\input.c` - keyboard/mouse hook, маршрутизація hotkey, трекінг останнього слова
- `src\transform.c` - визначення розкладок, перетворення, clipboard-сценарій
- `src\settings.c` - налаштування, логування, звук, форматування hotkey
- `src\app.h` - спільні оголошення