#include "app.h"

static BOOL BuildSettingsDirectoryPath(wchar_t *path, size_t capacity) {
    DWORD length;

    length = GetEnvironmentVariableW(L"APPDATA", path, (DWORD)capacity);
    if (length == 0 || length >= capacity) {
        if (GetModuleFileNameW(NULL, path, (DWORD)capacity) == 0) {
            return FALSE;
        }

        wchar_t *slash = wcsrchr(path, L'\\');
        if (slash != NULL) {
            *slash = L'\0';
        }
    } else {
        if (FAILED(StringCchCatW(path, capacity, L"\\KeyboardSwitcherC"))) {
            return FALSE;
        }
        CreateDirectoryW(path, NULL);
    }

    return TRUE;
}

static BOOL BuildSettingsFilePath(wchar_t *path, size_t capacity) {
    if (!BuildSettingsDirectoryPath(path, capacity)) {
        return FALSE;
    }

    if (wcsstr(path, L"KeyboardSwitcherC") == NULL) {
        if (FAILED(StringCchCatW(path, capacity, L"\\KeyboardSwitcherC"))) {
            return FALSE;
        }
        CreateDirectoryW(path, NULL);
    }

    return SUCCEEDED(StringCchCatW(path, capacity, L"\\settings.ini"));
}

static BOOL IsExtendedVirtualKey(UINT vk) {
    switch (vk) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_DIVIDE:
    case VK_NUMLOCK:
    case VK_RCONTROL:
    case VK_RMENU:
        return TRUE;
    default:
        return FALSE;
    }
}

static void GetVirtualKeyDisplayName(UINT vk, wchar_t *buffer, size_t capacity) {
    LONG scan_code;

    if (vk == 0) {
        StringCchCopyW(buffer, capacity, L"Not assigned");
        return;
    }

    switch (vk) {
    case VK_PAUSE: StringCchCopyW(buffer, capacity, L"Pause"); return;
    case VK_SPACE: StringCchCopyW(buffer, capacity, L"Space"); return;
    case VK_RETURN: StringCchCopyW(buffer, capacity, L"Enter"); return;
    case VK_TAB: StringCchCopyW(buffer, capacity, L"Tab"); return;
    case VK_ESCAPE: StringCchCopyW(buffer, capacity, L"Esc"); return;
    case VK_BACK: StringCchCopyW(buffer, capacity, L"Backspace"); return;
    default:
        break;
    }

    scan_code = (LONG)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16;
    if (IsExtendedVirtualKey(vk)) {
        scan_code |= 1 << 24;
    }

    if (GetKeyNameTextW(scan_code, buffer, (int)capacity) == 0) {
        StringCchPrintfW(buffer, capacity, L"VK %u", vk);
    }
}

void SetDefaultSettings(AppSettings *settings) {
    ZeroMemory(settings, sizeof(*settings));
    settings->logging_enabled = TRUE;
    settings->sound_enabled = FALSE;
    settings->selected_hotkey.vk = DEFAULT_SELECTED_HOTKEY_VK;
    settings->selected_hotkey.modifiers = DEFAULT_SELECTED_HOTKEY_MODIFIERS;
    settings->lastword_hotkey.vk = DEFAULT_LASTWORD_HOTKEY_VK;
    settings->lastword_hotkey.modifiers = DEFAULT_LASTWORD_HOTKEY_MODIFIERS;
}

void NormalizeHotkeyBinding(HotkeyBinding *binding) {
    if (binding->vk == 0) {
        binding->modifiers = 0;
    }
}

BOOL IsModifierVirtualKey(UINT vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT
        || vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL
        || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU
        || vk == VK_LWIN || vk == VK_RWIN;
}

UINT CaptureCurrentModifiers(void) {
    UINT modifiers = 0;

    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= MOD_SHIFT;
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= MOD_CONTROL;
    if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) modifiers |= MOD_ALT;
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0) modifiers |= MOD_WIN;

    return modifiers;
}

void LoadSettings(AppSettings *settings) {
    wchar_t path[MAX_PATH];

    SetDefaultSettings(settings);
    if (!BuildSettingsFilePath(path, sizeof(path) / sizeof(path[0]))) {
        return;
    }

    settings->logging_enabled = GetPrivateProfileIntW(L"General", L"LoggingEnabled", settings->logging_enabled, path) != 0;
    settings->sound_enabled = GetPrivateProfileIntW(L"General", L"SoundEnabled", settings->sound_enabled, path) != 0;
    settings->selected_hotkey.vk = (UINT)GetPrivateProfileIntW(L"Hotkeys", L"SelectedVk", (int)settings->selected_hotkey.vk, path);
    settings->selected_hotkey.modifiers = (UINT)GetPrivateProfileIntW(L"Hotkeys", L"SelectedModifiers", (int)settings->selected_hotkey.modifiers, path);
    settings->lastword_hotkey.vk = (UINT)GetPrivateProfileIntW(L"Hotkeys", L"LastWordVk", (int)settings->lastword_hotkey.vk, path);
    settings->lastword_hotkey.modifiers = (UINT)GetPrivateProfileIntW(L"Hotkeys", L"LastWordModifiers", (int)settings->lastword_hotkey.modifiers, path);

    NormalizeHotkeyBinding(&settings->selected_hotkey);
    NormalizeHotkeyBinding(&settings->lastword_hotkey);
}

void SaveSettings(const AppSettings *settings) {
    wchar_t path[MAX_PATH];
    wchar_t number[32];

    if (!BuildSettingsFilePath(path, sizeof(path) / sizeof(path[0]))) {
        return;
    }

    StringCchPrintfW(number, sizeof(number) / sizeof(number[0]), L"%d", settings->logging_enabled ? 1 : 0);
    WritePrivateProfileStringW(L"General", L"LoggingEnabled", number, path);

    StringCchPrintfW(number, sizeof(number) / sizeof(number[0]), L"%d", settings->sound_enabled ? 1 : 0);
    WritePrivateProfileStringW(L"General", L"SoundEnabled", number, path);

    StringCchPrintfW(number, sizeof(number) / sizeof(number[0]), L"%u", settings->selected_hotkey.vk);
    WritePrivateProfileStringW(L"Hotkeys", L"SelectedVk", number, path);

    StringCchPrintfW(number, sizeof(number) / sizeof(number[0]), L"%u", settings->selected_hotkey.modifiers);
    WritePrivateProfileStringW(L"Hotkeys", L"SelectedModifiers", number, path);

    StringCchPrintfW(number, sizeof(number) / sizeof(number[0]), L"%u", settings->lastword_hotkey.vk);
    WritePrivateProfileStringW(L"Hotkeys", L"LastWordVk", number, path);

    StringCchPrintfW(number, sizeof(number) / sizeof(number[0]), L"%u", settings->lastword_hotkey.modifiers);
    WritePrivateProfileStringW(L"Hotkeys", L"LastWordModifiers", number, path);
}

void FormatHotkeyBinding(const HotkeyBinding *binding, wchar_t *buffer, size_t capacity) {
    wchar_t key_name[64];
    wchar_t prefix[128] = L"";

    if (binding->vk == 0) {
        StringCchCopyW(buffer, capacity, L"Not assigned");
        return;
    }

    if ((binding->modifiers & MOD_CONTROL) != 0) StringCchCatW(prefix, sizeof(prefix) / sizeof(prefix[0]), L"Ctrl+");
    if ((binding->modifiers & MOD_ALT) != 0) StringCchCatW(prefix, sizeof(prefix) / sizeof(prefix[0]), L"Alt+");
    if ((binding->modifiers & MOD_SHIFT) != 0) StringCchCatW(prefix, sizeof(prefix) / sizeof(prefix[0]), L"Shift+");
    if ((binding->modifiers & MOD_WIN) != 0) StringCchCatW(prefix, sizeof(prefix) / sizeof(prefix[0]), L"Win+");

    GetVirtualKeyDisplayName(binding->vk, key_name, sizeof(key_name) / sizeof(key_name[0]));
    StringCchPrintfW(buffer, capacity, L"%ls%ls", prefix, key_name);
}

BOOL HotkeyBindingsEqual(const HotkeyBinding *left, const HotkeyBinding *right) {
    return left->vk == right->vk && left->modifiers == right->modifiers;
}

BOOL HotkeyOwnsEvent(const HotkeyBinding *binding, const KBDLLHOOKSTRUCT *kbd) {
    if (binding->vk == 0) {
        return FALSE;
    }

    if (kbd->vkCode != binding->vk) {
        return FALSE;
    }

    return CaptureCurrentModifiers() == binding->modifiers;
}

void PlaySwitchSound(void) {
    if (!g_settings.sound_enabled) {
        return;
    }

    MessageBeep(MB_OK);
}

void LogDebug(const wchar_t *format, ...) {
    wchar_t message[1024];
    wchar_t line[1400];
    wchar_t path[MAX_PATH];
    wchar_t *slash;
    FILE *file;
    SYSTEMTIME st;
    va_list args;

    if (!g_settings.logging_enabled) {
        return;
    }

    GetLocalTime(&st);
    GetModuleFileNameW(NULL, path, MAX_PATH);
    slash = wcsrchr(path, L'\\');
    if (slash != NULL) {
        slash[1] = L'\0';
    }
    wcscat(path, L"keyboard-switcher-c.log");

    va_start(args, format);
    vswprintf(message, sizeof(message) / sizeof(message[0]), format, args);
    va_end(args);

    swprintf(
        line,
        sizeof(line) / sizeof(line[0]),
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u | %s\n",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        message);

    file = _wfopen(path, L"a+, ccs=UTF-8");
    if (file == NULL) {
        return;
    }

    fputws(line, file);
    fclose(file);
}
