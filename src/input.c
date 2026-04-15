#include "app.h"

static HHOOK g_keyboard_hook = NULL;
static HHOOK g_mouse_hook = NULL;

static BOOL IsModifierKey(DWORD vk) {
    return IsModifierVirtualKey((UINT)vk);
}

static BOOL ShouldResetWordTrackerForKey(DWORD vk) {
    switch (vk) {
    case VK_SPACE:
    case VK_RETURN:
    case VK_TAB:
    case VK_ESCAPE:
    case VK_DELETE:
    case VK_INSERT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
        return TRUE;
    default:
        return FALSE;
    }
}

static BOOL HasResetModifierCombo(DWORD vk) {
    SHORT ctrl = GetAsyncKeyState(VK_CONTROL);
    SHORT alt = GetAsyncKeyState(VK_MENU);
    SHORT lwin = GetAsyncKeyState(VK_LWIN);
    SHORT rwin = GetAsyncKeyState(VK_RWIN);

    if (IsModifierKey(vk)) {
        return FALSE;
    }

    if ((ctrl & 0x8000) != 0) {
        return TRUE;
    }

    if ((alt & 0x8000) != 0) {
        return TRUE;
    }

    if ((lwin & 0x8000) != 0 || (rwin & 0x8000) != 0) {
        return TRUE;
    }

    return FALSE;
}

static int TranslateHookKeyToChars(const KBDLLHOOKSTRUCT *kbd, wchar_t *buffer, int capacity) {
    BYTE key_state[256];
    HWND foreground;
    DWORD thread_id;
    HKL layout;
    int result;

    ZeroMemory(key_state, sizeof(key_state));
    if (!GetKeyboardState(key_state)) {
        ZeroMemory(key_state, sizeof(key_state));
    }

    key_state[kbd->vkCode] |= 0x80;
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) key_state[VK_SHIFT] = 0x80;
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) key_state[VK_CONTROL] = 0x80;
    if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) key_state[VK_MENU] = 0x80;

    foreground = GetForegroundWindow();
    thread_id = foreground ? GetWindowThreadProcessId(foreground, NULL) : 0;
    layout = GetKeyboardLayout(thread_id);

    result = ToUnicodeEx((UINT)kbd->vkCode, (UINT)kbd->scanCode, key_state, buffer, capacity, 0, layout);
    if (result < 0) {
        wchar_t dummy[8];
        ToUnicodeEx((UINT)kbd->vkCode, (UINT)kbd->scanCode, key_state, dummy, 8, 0, layout);
        return 0;
    }

    return result;
}

static void BackspaceLastWordTracker(void) {
    if (g_last_word.length <= 0) {
        ResetLastWordTracker();
        return;
    }

    g_last_word.length--;
    g_last_word.text[g_last_word.length] = L'\0';
    if (g_last_word.length == 0) {
        g_last_word.source_layout = NULL;
    }
}

static void AppendTrackedChars(HWND foreground, const LayoutDef *layout, const wchar_t *chars, int count) {
    int i;

    if (foreground == NULL || layout == NULL) {
        ResetLastWordTracker();
        return;
    }

    if (g_last_word.length > 0 && g_last_word.hwnd != foreground) {
        ResetLastWordTracker();
    }

    if (g_last_word.length > 0 && g_last_word.source_layout != NULL &&
        wcscmp(g_last_word.source_layout->code, layout->code) != 0) {
        ResetLastWordTracker();
    }

    if (g_last_word.length == 0) {
        g_last_word.hwnd = foreground;
        g_last_word.source_layout = layout;
    }

    for (i = 0; i < count; ++i) {
        wchar_t ch = chars[i];
        if (ch < 0x20 || ch == 0x7F) {
            continue;
        }

        if (g_last_word.length >= LAST_WORD_MAX - 1) {
            ResetLastWordTracker();
            g_last_word.hwnd = foreground;
            g_last_word.source_layout = layout;
        }

        g_last_word.text[g_last_word.length++] = ch;
        g_last_word.text[g_last_word.length] = L'\0';
    }
}

void ResetLastWordTracker(void) {
    g_last_word.hwnd = NULL;
    g_last_word.length = 0;
    g_last_word.text[0] = L'\0';
    g_last_word.source_layout = NULL;
}

void UpdateLastWordTrackerFromKey(const KBDLLHOOKSTRUCT *kbd) {
    HWND foreground = GetForegroundWindow();
    const LayoutDef *current_layout = GetCurrentLayoutDef();
    wchar_t chars[8];
    int char_count;

    if (foreground == NULL) {
        ResetLastWordTracker();
        return;
    }

    if (g_last_word.length > 0 && g_last_word.hwnd != foreground) {
        ResetLastWordTracker();
    }

    if (ShouldResetWordTrackerForKey(kbd->vkCode)) {
        ResetLastWordTracker();
        return;
    }

    if (HasResetModifierCombo(kbd->vkCode)) {
        ResetLastWordTracker();
        return;
    }

    if (kbd->vkCode == VK_BACK) {
        BackspaceLastWordTracker();
        return;
    }

    char_count = TranslateHookKeyToChars(kbd, chars, (int)(sizeof(chars) / sizeof(chars[0])));
    if (char_count <= 0) {
        return;
    }

    AppendTrackedChars(foreground, current_layout, chars, char_count);
}

BOOL IsKeyDownMessage(WPARAM wParam) {
    return wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
}

BOOL IsKeyUpMessage(WPARAM wParam) {
    return wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
}

TransformRequestMode ResolveTransformModeForEvent(const KBDLLHOOKSTRUCT *kbd) {
    BOOL selected_match = HotkeyOwnsEvent(&g_settings.selected_hotkey, kbd);
    BOOL lastword_match = HotkeyOwnsEvent(&g_settings.lastword_hotkey, kbd);

    if (selected_match && lastword_match) {
        return TRANSFORM_MODE_COMBINED;
    }

    if (selected_match) {
        return TRANSFORM_MODE_SELECTED;
    }

    if (lastword_match) {
        return TRANSFORM_MODE_LASTWORD;
    }

    return TRANSFORM_MODE_NONE;
}

void QueueTransformRequest(TransformRequestMode mode) {
    ULONGLONG now;

    if (mode == TRANSFORM_MODE_NONE) {
        return;
    }

    now = GetTickCount64();
    if (now - g_last_pause_tick < PAUSE_DEBOUNCE_MS) {
        return;
    }

    g_last_pause_tick = now;
    LogDebug(L"Hotkey accepted. Mode=%d.", (int)mode);
    PostMessageW(g_main_window, WMAPP_TRANSFORM, (WPARAM)mode, 0);
}

static void HandleTransform(TransformRequestMode mode) {
    Sleep(ACTION_DELAY_MS);
    RefreshInstalledLayouts();

    switch (mode) {
    case TRANSFORM_MODE_SELECTED:
        if (TransformSelectedText()) {
            ResetLastWordTracker();
            PlaySwitchSound();
            return;
        }
        LogDebug(L"TransformSelectedText failed.");
        return;

    case TRANSFORM_MODE_LASTWORD:
        if (TransformLastWord()) {
            PlaySwitchSound();
            return;
        }
        LogDebug(L"TransformLastWord failed.");
        return;

    case TRANSFORM_MODE_COMBINED:
    default:
        if (TransformSelectedText()) {
            ResetLastWordTracker();
            PlaySwitchSound();
            return;
        }

        LogDebug(L"TransformSelectedText failed. Trying last word.");
        if (TransformLastWord()) {
            PlaySwitchSound();
            return;
        }

        LogDebug(L"TransformLastWord failed.");
        return;
    }
}

DWORD WINAPI TransformWorkerThread(LPVOID param) {
    TransformRequestMode mode = (TransformRequestMode)(INT_PTR)param;
    HandleTransform(mode);
    InterlockedExchange(&g_transform_running, 0);
    return 0;
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT *)lParam;
    TransformRequestMode mode;

    if (nCode < 0 || kbd == NULL) {
        return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
    }

    if ((kbd->flags & LLKHF_INJECTED) != 0) {
        return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
    }

    if (HandleSettingsHotkeyCapture(kbd, wParam)) {
        return 1;
    }

    mode = ResolveTransformModeForEvent(kbd);
    if (mode != TRANSFORM_MODE_NONE) {
        if (IsKeyUpMessage(wParam)) {
            QueueTransformRequest(mode);
        }
        return 1;
    }

    if (IsKeyDownMessage(wParam)) {
        UpdateLastWordTrackerFromKey(kbd);
    }

    return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
}

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    if (nCode < 0) {
        return CallNextHookEx(g_mouse_hook, nCode, wParam, lParam);
    }

    switch (wParam) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_MOUSEWHEEL:
        ResetLastWordTracker();
        break;
    default:
        break;
    }

    return CallNextHookEx(g_mouse_hook, nCode, wParam, lParam);
}

BOOL InstallKeyboardHook(void) {
    g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_instance, 0);
    if (g_keyboard_hook == NULL) {
        LogDebug(L"SetWindowsHookEx failed. Win32=%lu.", GetLastError());
        return FALSE;
    }

    LogDebug(L"Keyboard hook installed.");
    return TRUE;
}

BOOL InstallMouseHook(void) {
    g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, g_instance, 0);
    if (g_mouse_hook == NULL) {
        LogDebug(L"SetWindowsHookEx mouse failed. Win32=%lu.", GetLastError());
        return FALSE;
    }

    LogDebug(L"Mouse hook installed.");
    return TRUE;
}

void RemoveKeyboardHook(void) {
    if (g_keyboard_hook != NULL) {
        UnhookWindowsHookEx(g_keyboard_hook);
        g_keyboard_hook = NULL;
        LogDebug(L"Keyboard hook removed.");
    }
}

void RemoveMouseHook(void) {
    if (g_mouse_hook != NULL) {
        UnhookWindowsHookEx(g_mouse_hook);
        g_mouse_hook = NULL;
        LogDebug(L"Mouse hook removed.");
    }
}
