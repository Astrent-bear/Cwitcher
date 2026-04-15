#include "app.h"

static HWND g_settings_window = NULL;
static AppSettings g_pending_settings = { 0 };
static SettingsControls g_settings_controls = { 0 };
static CaptureTarget g_capture_target = CAPTURE_NONE;
static NOTIFYICONDATAW g_tray = { 0 };
static HICON g_icon_en = NULL;
static HICON g_icon_ru = NULL;
static HICON g_icon_uk = NULL;
static HICON g_icon_unknown = NULL;
static wchar_t g_last_badge[8] = L"";

static HICON CreateBadgeIcon(const wchar_t *text, COLORREF background) {
    BITMAPV5HEADER bi;
    ICONINFO ii;
    HDC screen;
    HDC dc;
    HBITMAP color;
    HBITMAP mask;
    HFONT font;
    HGDIOBJ old_bitmap;
    HGDIOBJ old_font;
    HBRUSH brush;
    RECT rc;
    RECT text_rc;
    HICON icon;
    void *bits;

    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = 32;
    bi.bV5Height = -32;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    rc.left = 0;
    rc.top = 0;
    rc.right = 32;
    rc.bottom = 32;
    icon = NULL;
    bits = NULL;

    screen = GetDC(NULL);
    dc = CreateCompatibleDC(screen);
    color = CreateDIBSection(screen, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    mask = CreateBitmap(32, 32, 1, 1, NULL);
    if (dc == NULL || color == NULL || mask == NULL) {
        if (screen != NULL) {
            ReleaseDC(NULL, screen);
        }
        if (dc != NULL) {
            DeleteDC(dc);
        }
        if (color != NULL) {
            DeleteObject(color);
        }
        if (mask != NULL) {
            DeleteObject(mask);
        }
        return NULL;
    }

    old_bitmap = SelectObject(dc, color);
    brush = CreateSolidBrush(background);
    FillRect(dc, &rc, brush);
    FrameRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    text_rc = rc;
    text_rc.left += 1;
    text_rc.right -= 1;

    font = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    old_font = SelectObject(dc, font);
    DrawTextW(dc, text, -1, &text_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = mask;
    icon = CreateIconIndirect(&ii);

    SelectObject(dc, old_font);
    SelectObject(dc, old_bitmap);
    DeleteObject(font);
    DeleteObject(brush);
    DeleteObject(color);
    DeleteObject(mask);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);
    return icon;
}

static HICON GetTrayIconForLayout(const LayoutDef *layout) {
    if (layout == NULL) {
        return g_icon_unknown;
    }
    if (wcscmp(layout->badge, L"EN") == 0) {
        return g_icon_en;
    }
    if (wcscmp(layout->badge, L"RU") == 0) {
        return g_icon_ru;
    }
    if (wcscmp(layout->badge, L"UK") == 0) {
        return g_icon_uk;
    }
    return g_icon_unknown;
}

static void ApplyDefaultGuiFont(HWND control) {
    SendMessageW(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

static void SetCaptureStatusText(const wchar_t *text) {
    if (g_settings_controls.status_value != NULL) {
        SetWindowTextW(g_settings_controls.status_value, text);
    }
}

static void UpdateSettingsWindowState(void) {
    wchar_t selected_text[128];
    wchar_t lastword_text[128];

    if (g_settings_window == NULL) {
        return;
    }

    FormatHotkeyBinding(&g_pending_settings.selected_hotkey, selected_text, sizeof(selected_text) / sizeof(selected_text[0]));
    FormatHotkeyBinding(&g_pending_settings.lastword_hotkey, lastword_text, sizeof(lastword_text) / sizeof(lastword_text[0]));

    if (g_settings_controls.logging_checkbox != NULL) {
        Button_SetCheck(g_settings_controls.logging_checkbox, g_pending_settings.logging_enabled ? BST_CHECKED : BST_UNCHECKED);
    }

    if (g_settings_controls.sound_checkbox != NULL) {
        Button_SetCheck(g_settings_controls.sound_checkbox, g_pending_settings.sound_enabled ? BST_CHECKED : BST_UNCHECKED);
    }

    if (g_settings_controls.selected_value != NULL) {
        SetWindowTextW(g_settings_controls.selected_value, selected_text);
    }

    if (g_settings_controls.lastword_value != NULL) {
        SetWindowTextW(g_settings_controls.lastword_value, lastword_text);
    }

    switch (g_capture_target) {
    case CAPTURE_SELECTED:
        SetCaptureStatusText(L"Press a new shortcut for selected text. Esc cancels capture.");
        break;
    case CAPTURE_LASTWORD:
        SetCaptureStatusText(L"Press a new shortcut for last word. Esc cancels capture.");
        break;
    default:
        SetCaptureStatusText(L"Use Record to capture a new shortcut. Save applies the changes.");
        break;
    }
}

static void ResetPendingHotkeyToDefault(CaptureTarget target) {
    if (target == CAPTURE_SELECTED) {
        g_pending_settings.selected_hotkey.vk = DEFAULT_SELECTED_HOTKEY_VK;
        g_pending_settings.selected_hotkey.modifiers = DEFAULT_SELECTED_HOTKEY_MODIFIERS;
    } else if (target == CAPTURE_LASTWORD) {
        g_pending_settings.lastword_hotkey.vk = DEFAULT_LASTWORD_HOTKEY_VK;
        g_pending_settings.lastword_hotkey.modifiers = DEFAULT_LASTWORD_HOTKEY_MODIFIERS;
    }
    UpdateSettingsWindowState();
}

static void ClearPendingHotkey(CaptureTarget target) {
    HotkeyBinding empty_binding = { 0 };

    if (target == CAPTURE_SELECTED) {
        g_pending_settings.selected_hotkey = empty_binding;
    } else if (target == CAPTURE_LASTWORD) {
        g_pending_settings.lastword_hotkey = empty_binding;
    }
    UpdateSettingsWindowState();
}

static void StartHotkeyCapture(CaptureTarget target) {
    g_capture_target = target;
    UpdateSettingsWindowState();
}

static void CancelHotkeyCapture(void) {
    g_capture_target = CAPTURE_NONE;
    UpdateSettingsWindowState();
}

static void CommitCapturedHotkey(UINT vk, UINT modifiers) {
    HotkeyBinding binding;

    binding.vk = vk;
    binding.modifiers = modifiers;
    NormalizeHotkeyBinding(&binding);

    if (g_capture_target == CAPTURE_SELECTED) {
        g_pending_settings.selected_hotkey = binding;
    } else if (g_capture_target == CAPTURE_LASTWORD) {
        g_pending_settings.lastword_hotkey = binding;
    }

    g_capture_target = CAPTURE_NONE;
    UpdateSettingsWindowState();
}

static void CreateSettingsControls(HWND hwnd) {
    int x = SETTINGS_MARGIN;
    int width = 430;
    HWND control;

    control = CreateWindowExW(0, L"BUTTON", L"Enable switch sound", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, 14, width, 22, hwnd, (HMENU)(INT_PTR)IDC_SETTINGS_SOUND, g_instance, NULL);
    g_settings_controls.sound_checkbox = control;
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Enable debug log", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, 40, width, 22, hwnd, (HMENU)(INT_PTR)IDC_SETTINGS_LOGGING, g_instance, NULL);
    g_settings_controls.logging_checkbox = control;
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Selected Text Hotkey", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, 70, width, 78, hwnd, NULL, g_instance, NULL);
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        x + 12, 95, 190, 24, hwnd, (HMENU)(INT_PTR)IDC_SELECTED_VALUE, g_instance, NULL);
    g_settings_controls.selected_value = control;
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Record", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 212, 95, 64, 24, hwnd, (HMENU)(INT_PTR)IDC_SELECTED_RECORD, g_instance, NULL);
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 284, 95, 56, 24, hwnd, (HMENU)(INT_PTR)IDC_SELECTED_CLEAR, g_instance, NULL);
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Reset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 348, 95, 60, 24, hwnd, (HMENU)(INT_PTR)IDC_SELECTED_RESET, g_instance, NULL);
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Last Word Hotkey", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, 156, width, 78, hwnd, NULL, g_instance, NULL);
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        x + 12, 181, 190, 24, hwnd, (HMENU)(INT_PTR)IDC_LASTWORD_VALUE, g_instance, NULL);
    g_settings_controls.lastword_value = control;
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Record", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 212, 181, 64, 24, hwnd, (HMENU)(INT_PTR)IDC_LASTWORD_RECORD, g_instance, NULL);
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 284, 181, 56, 24, hwnd, (HMENU)(INT_PTR)IDC_LASTWORD_CLEAR, g_instance, NULL);
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Reset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 348, 181, 60, 24, hwnd, (HMENU)(INT_PTR)IDC_LASTWORD_RESET, g_instance, NULL);
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, 244, width, 34, hwnd, (HMENU)(INT_PTR)IDC_SETTINGS_STATUS, g_instance, NULL);
    g_settings_controls.status_value = control;
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        x + 270, 284, 70, 26, hwnd, (HMENU)(INT_PTR)IDC_SETTINGS_SAVE, g_instance, NULL);
    ApplyDefaultGuiFont(control);

    control = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 348, 284, 82, 26, hwnd, (HMENU)(INT_PTR)IDC_SETTINGS_CANCEL, g_instance, NULL);
    ApplyDefaultGuiFont(control);
}

static void PositionWindowNearTray(HWND hwnd) {
    RECT work_area;
    RECT window_rect;
    int x;
    int y;

    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0)) {
        work_area.left = 0;
        work_area.top = 0;
        work_area.right = GetSystemMetrics(SM_CXSCREEN);
        work_area.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    if (!GetWindowRect(hwnd, &window_rect)) {
        return;
    }

    x = work_area.right - (window_rect.right - window_rect.left) - 18;
    y = work_area.bottom - (window_rect.bottom - window_rect.top) - 18;

    if (x < work_area.left) x = work_area.left + 18;
    if (y < work_area.top) y = work_area.top + 18;

    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
}

static LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    switch (message) {
    case WM_CREATE:
        CreateSettingsControls(hwnd);
        UpdateSettingsWindowState();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SELECTED_RECORD:
            StartHotkeyCapture(CAPTURE_SELECTED);
            return 0;
        case IDC_SELECTED_CLEAR:
            ClearPendingHotkey(CAPTURE_SELECTED);
            return 0;
        case IDC_SELECTED_RESET:
            ResetPendingHotkeyToDefault(CAPTURE_SELECTED);
            return 0;
        case IDC_LASTWORD_RECORD:
            StartHotkeyCapture(CAPTURE_LASTWORD);
            return 0;
        case IDC_LASTWORD_CLEAR:
            ClearPendingHotkey(CAPTURE_LASTWORD);
            return 0;
        case IDC_LASTWORD_RESET:
            ResetPendingHotkeyToDefault(CAPTURE_LASTWORD);
            return 0;
        case IDC_SETTINGS_SAVE:
            g_pending_settings.sound_enabled = Button_GetCheck(g_settings_controls.sound_checkbox) == BST_CHECKED;
            g_pending_settings.logging_enabled = Button_GetCheck(g_settings_controls.logging_checkbox) == BST_CHECKED;
            g_settings = g_pending_settings;
            SaveSettings(&g_settings);
            DestroyWindow(hwnd);
            return 0;
        case IDC_SETTINGS_CANCEL:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_NCDESTROY:
        ZeroMemory(&g_settings_controls, sizeof(g_settings_controls));
        g_capture_target = CAPTURE_NONE;
        g_settings_window = NULL;
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void OpenSettingsWindow(void) {
    static BOOL class_registered = FALSE;
    WNDCLASSEXW wc;
    RECT rect;

    if (g_settings_window != NULL) {
        PositionWindowNearTray(g_settings_window);
        ShowWindow(g_settings_window, SW_SHOWNORMAL);
        SetForegroundWindow(g_settings_window);
        return;
    }

    if (!class_registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = SettingsWindowProc;
        wc.hInstance = g_instance;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hIcon = g_icon_unknown;
        wc.hIconSm = g_icon_unknown;
        wc.lpszClassName = SETTINGS_CLASS_NAME;
        if (!RegisterClassExW(&wc)) {
            return;
        }
        class_registered = TRUE;
    }

    g_pending_settings = g_settings;
    g_capture_target = CAPTURE_NONE;

    rect.left = 0;
    rect.top = 0;
    rect.right = 460;
    rect.bottom = 352;
    AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, WS_EX_APPWINDOW);

    g_settings_window = CreateWindowExW(
        WS_EX_APPWINDOW,
        SETTINGS_CLASS_NAME,
        L"KeyboardSwitcher Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        g_instance,
        NULL);

    if (g_settings_window == NULL) {
        return;
    }

    UpdateSettingsWindowState();
    PositionWindowNearTray(g_settings_window);
    ShowWindow(g_settings_window, SW_SHOWNORMAL);
    UpdateWindow(g_settings_window);
    SetForegroundWindow(g_settings_window);
}

BOOL HandleSettingsHotkeyCapture(const KBDLLHOOKSTRUCT *kbd, WPARAM wParam) {
    if (g_capture_target == CAPTURE_NONE) {
        return FALSE;
    }

    if (IsKeyDownMessage(wParam)) {
        if (kbd->vkCode == VK_ESCAPE) {
            CancelHotkeyCapture();
            return TRUE;
        }

        if (!IsModifierVirtualKey((UINT)kbd->vkCode)) {
            CommitCapturedHotkey((UINT)kbd->vkCode, CaptureCurrentModifiers());
            return TRUE;
        }
    }

    return TRUE;
}

void NotifySettingsSoundToggled(void) {
    if (g_settings_window != NULL) {
        g_pending_settings.sound_enabled = g_settings.sound_enabled;
        UpdateSettingsWindowState();
    }
}

void CreateTrayIcons(void) {
    g_icon_en = CreateBadgeIcon(L"EN", RGB(255, 255, 255));
    g_icon_ru = CreateBadgeIcon(L"RU", RGB(255, 255, 255));
    g_icon_uk = CreateBadgeIcon(L"UK", RGB(255, 255, 255));
    g_icon_unknown = CreateBadgeIcon(L"??", RGB(255, 255, 255));
}

void DestroyTrayIcons(void) {
    if (g_icon_en != NULL) DestroyIcon(g_icon_en);
    if (g_icon_ru != NULL) DestroyIcon(g_icon_ru);
    if (g_icon_uk != NULL) DestroyIcon(g_icon_uk);
    if (g_icon_unknown != NULL) DestroyIcon(g_icon_unknown);
    g_icon_en = NULL;
    g_icon_ru = NULL;
    g_icon_uk = NULL;
    g_icon_unknown = NULL;
}

BOOL AddTrayIcon(HWND hwnd) {
    ZeroMemory(&g_tray, sizeof(g_tray));
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = hwnd;
    g_tray.uID = TRAY_UID;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = WMAPP_TRAYICON;
    g_tray.hIcon = g_icon_unknown;
    wcscpy(g_tray.szTip, APP_NAME);

    if (!Shell_NotifyIconW(NIM_ADD, &g_tray)) {
        return FALSE;
    }

    g_tray.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
    UpdateTrayLayoutIndicator(TRUE);
    return TRUE;
}

void RemoveTrayIcon(void) {
    if (g_tray.hWnd != NULL) {
        Shell_NotifyIconW(NIM_DELETE, &g_tray);
        ZeroMemory(&g_tray, sizeof(g_tray));
    }
}

void UpdateTrayLayoutIndicator(BOOL force) {
    const LayoutDef *layout = GetCurrentLayoutDef();
    const wchar_t *badge = layout ? layout->badge : L"??";

    if (!force && wcscmp(g_last_badge, badge) == 0) {
        return;
    }

    g_tray.uFlags = NIF_ICON | NIF_TIP;
    g_tray.hIcon = GetTrayIconForLayout(layout);
    swprintf(g_tray.szTip, sizeof(g_tray.szTip) / sizeof(g_tray.szTip[0]), L"%s [%s]", APP_NAME, badge);
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
    wcsncpy(g_last_badge, badge, (sizeof(g_last_badge) / sizeof(g_last_badge[0])) - 1);
    g_last_badge[(sizeof(g_last_badge) / sizeof(g_last_badge[0])) - 1] = L'\0';
}

void ShowTrayMenu(void) {
    HMENU menu;
    POINT pt;
    UINT sound_flags = MF_STRING;

    menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }

    if (g_settings.sound_enabled) {
        sound_flags |= MF_CHECKED;
    }

    AppendMenuW(menu, MF_STRING, MENU_OPEN_SETTINGS, L"Settings...");
    AppendMenuW(menu, sound_flags, MENU_TOGGLE_SOUND, L"Enable Switch Sound");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, MENU_EXIT_APP, L"Exit");
    GetCursorPos(&pt);
    SetForegroundWindow(g_main_window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, g_main_window, NULL);
    DestroyMenu(menu);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_LAYOUT, LAYOUT_TIMER_INTERVAL_MS, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_LAYOUT) {
            UpdateTrayLayoutIndicator(FALSE);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == MENU_EXIT_APP) {
            DestroyWindow(hwnd);
        } else if (LOWORD(wParam) == MENU_OPEN_SETTINGS) {
            OpenSettingsWindow();
        } else if (LOWORD(wParam) == MENU_TOGGLE_SOUND) {
            g_settings.sound_enabled = !g_settings.sound_enabled;
            SaveSettings(&g_settings);
            NotifySettingsSoundToggled();
        }
        return 0;

    case WMAPP_TRAYICON:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            ShowTrayMenu();
        }
        return 0;

    case WMAPP_TRANSFORM:
    {
        HANDLE thread;
        if (InterlockedCompareExchange(&g_transform_running, 1, 0) != 0) {
            return 0;
        }

        thread = CreateThread(NULL, 0, TransformWorkerThread, (LPVOID)(INT_PTR)wParam, 0, NULL);
        if (thread != NULL) {
            CloseHandle(thread);
        } else {
            LogDebug(L"Failed to start transform worker. Win32=%lu.", GetLastError());
            InterlockedExchange(&g_transform_running, 0);
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_LAYOUT);
        RemoveTrayIcon();
        RemoveKeyboardHook();
        RemoveMouseHook();
        DestroyTrayIcons();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}
