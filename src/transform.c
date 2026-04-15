#include "app.h"

static const LayoutDef *g_installed_layouts[16] = { 0 };
static HKL g_installed_hkls[16] = { 0 };
static size_t g_installed_layout_count = 0;

static const KeyPair EN_KEYS[] = {
    { L'`', L'~' }, { L'1', L'!' }, { L'2', L'@' }, { L'3', L'#' }, { L'4', L'$' },
    { L'5', L'%' }, { L'6', L'^' }, { L'7', L'&' }, { L'8', L'*' }, { L'9', L'(' },
    { L'0', L')' }, { L'-', L'_' }, { L'=', L'+' },
    { L'q', L'Q' }, { L'w', L'W' }, { L'e', L'E' }, { L'r', L'R' }, { L't', L'T' },
    { L'y', L'Y' }, { L'u', L'U' }, { L'i', L'I' }, { L'o', L'O' }, { L'p', L'P' },
    { L'[', L'{' }, { L']', L'}' }, { L'\\', L'|' },
    { L'a', L'A' }, { L's', L'S' }, { L'd', L'D' }, { L'f', L'F' }, { L'g', L'G' },
    { L'h', L'H' }, { L'j', L'J' }, { L'k', L'K' }, { L'l', L'L' }, { L';', L':' },
    { L'\'', L'"' },
    { L'z', L'Z' }, { L'x', L'X' }, { L'c', L'C' }, { L'v', L'V' }, { L'b', L'B' },
    { L'n', L'N' }, { L'm', L'M' }, { L',', L'<' }, { L'.', L'>' }, { L'/', L'?' }
};

static const KeyPair EN_GB_KEYS[] = {
    { L'`', L'\x00AC' }, { L'1', L'!' }, { L'2', L'"' }, { L'3', L'\x00A3' }, { L'4', L'$' },
    { L'5', L'%' }, { L'6', L'^' }, { L'7', L'&' }, { L'8', L'*' }, { L'9', L'(' },
    { L'0', L')' }, { L'-', L'_' }, { L'=', L'+' },
    { L'q', L'Q' }, { L'w', L'W' }, { L'e', L'E' }, { L'r', L'R' }, { L't', L'T' },
    { L'y', L'Y' }, { L'u', L'U' }, { L'i', L'I' }, { L'o', L'O' }, { L'p', L'P' },
    { L'[', L'{' }, { L']', L'}' }, { L'#', L'~' },
    { L'a', L'A' }, { L's', L'S' }, { L'd', L'D' }, { L'f', L'F' }, { L'g', L'G' },
    { L'h', L'H' }, { L'j', L'J' }, { L'k', L'K' }, { L'l', L'L' }, { L';', L':' },
    { L'\'', L'@' },
    { L'z', L'Z' }, { L'x', L'X' }, { L'c', L'C' }, { L'v', L'V' }, { L'b', L'B' },
    { L'n', L'N' }, { L'm', L'M' }, { L',', L'<' }, { L'.', L'>' }, { L'/', L'?' }
};

static const KeyPair RU_KEYS[] = {
    { L'\x0451', L'\x0401' }, { L'1', L'!' }, { L'2', L'"' }, { L'3', L'\x2116' }, { L'4', L';' },
    { L'5', L'%' }, { L'6', L':' }, { L'7', L'?' }, { L'8', L'*' }, { L'9', L'(' },
    { L'0', L')' }, { L'-', L'_' }, { L'=', L'+' },
    { L'\x0439', L'\x0419' }, { L'\x0446', L'\x0426' }, { L'\x0443', L'\x0423' }, { L'\x043A', L'\x041A' }, { L'\x0435', L'\x0415' },
    { L'\x043D', L'\x041D' }, { L'\x0433', L'\x0413' }, { L'\x0448', L'\x0428' }, { L'\x0449', L'\x0429' }, { L'\x0437', L'\x0417' },
    { L'\x0445', L'\x0425' }, { L'\x044A', L'\x042A' }, { L'\\', L'/' },
    { L'\x0444', L'\x0424' }, { L'\x044B', L'\x042B' }, { L'\x0432', L'\x0412' }, { L'\x0430', L'\x0410' }, { L'\x043F', L'\x041F' },
    { L'\x0440', L'\x0420' }, { L'\x043E', L'\x041E' }, { L'\x043B', L'\x041B' }, { L'\x0434', L'\x0414' }, { L'\x0436', L'\x0416' },
    { L'\x044D', L'\x042D' },
    { L'\x044F', L'\x042F' }, { L'\x0447', L'\x0427' }, { L'\x0441', L'\x0421' }, { L'\x043C', L'\x041C' }, { L'\x0438', L'\x0418' },
    { L'\x0442', L'\x0422' }, { L'\x044C', L'\x042C' }, { L'\x0431', L'\x0411' }, { L'\x044E', L'\x042E' }, { L'.', L',' }
};

static const KeyPair UK_KEYS[] = {
    { L'\'', L'\x20B4' }, { L'1', L'!' }, { L'2', L'"' }, { L'3', L'\x2116' }, { L'4', L';' },
    { L'5', L'%' }, { L'6', L':' }, { L'7', L'?' }, { L'8', L'*' }, { L'9', L'(' },
    { L'0', L')' }, { L'-', L'_' }, { L'=', L'+' },
    { L'\x0439', L'\x0419' }, { L'\x0446', L'\x0426' }, { L'\x0443', L'\x0423' }, { L'\x043A', L'\x041A' }, { L'\x0435', L'\x0415' },
    { L'\x043D', L'\x041D' }, { L'\x0433', L'\x0413' }, { L'\x0448', L'\x0428' }, { L'\x0449', L'\x0429' }, { L'\x0437', L'\x0417' },
    { L'\x0445', L'\x0425' }, { L'\x0457', L'\x0407' }, { L'\\', L'/' },
    { L'\x0444', L'\x0424' }, { L'\x0456', L'\x0406' }, { L'\x0432', L'\x0412' }, { L'\x0430', L'\x0410' }, { L'\x043F', L'\x041F' },
    { L'\x0440', L'\x0420' }, { L'\x043E', L'\x041E' }, { L'\x043B', L'\x041B' }, { L'\x0434', L'\x0414' }, { L'\x0436', L'\x0416' },
    { L'\x0454', L'\x0404' },
    { L'\x044F', L'\x042F' }, { L'\x0447', L'\x0427' }, { L'\x0441', L'\x0421' }, { L'\x043C', L'\x041C' }, { L'\x0438', L'\x0418' },
    { L'\x0442', L'\x0422' }, { L'\x044C', L'\x042C' }, { L'\x0431', L'\x0411' }, { L'\x044E', L'\x042E' }, { L'.', L',' }
};

static const LayoutDef ALL_LAYOUTS[] = {
    { L"en",    L"EN", L"English",    L"latin",    0x0409, RGB(36, 82, 160), EN_KEYS,    sizeof(EN_KEYS) / sizeof(EN_KEYS[0]) },
    { L"en-gb", L"EN", L"English UK", L"latin",    0x0809, RGB(36, 82, 160), EN_GB_KEYS, sizeof(EN_GB_KEYS) / sizeof(EN_GB_KEYS[0]) },
    { L"ru",    L"RU", L"Russian",    L"cyrillic", 0x0419, RGB(168, 44, 44), RU_KEYS,    sizeof(RU_KEYS) / sizeof(RU_KEYS[0]) },
    { L"uk",    L"UK", L"Ukrainian",  L"cyrillic", 0x0422, RGB(0, 116, 143), UK_KEYS,    sizeof(UK_KEYS) / sizeof(UK_KEYS[0]) }
};

static wchar_t *DupWideString(const wchar_t *value) {
    size_t length;
    wchar_t *copy;

    if (value == NULL) {
        return NULL;
    }

    length = wcslen(value);
    copy = (wchar_t *)malloc((length + 1) * sizeof(wchar_t));
    if (copy == NULL) {
        return NULL;
    }

    wcscpy(copy, value);
    return copy;
}

static const LayoutDef *FindLayoutByCode(const wchar_t *code) {
    size_t i;

    for (i = 0; i < sizeof(ALL_LAYOUTS) / sizeof(ALL_LAYOUTS[0]); ++i) {
        if (wcscmp(ALL_LAYOUTS[i].code, code) == 0) {
            return &ALL_LAYOUTS[i];
        }
    }

    return NULL;
}

static const LayoutDef *FindLayoutByLangId(WORD lang_id) {
    size_t i;

    for (i = 0; i < sizeof(ALL_LAYOUTS) / sizeof(ALL_LAYOUTS[0]); ++i) {
        if (ALL_LAYOUTS[i].lang_id == lang_id) {
            return &ALL_LAYOUTS[i];
        }
    }

    return NULL;
}

static BOOL TryOpenClipboardWithRetry(HWND owner) {
    int attempt;

    for (attempt = 0; attempt < CLIPBOARD_RETRY_COUNT; ++attempt) {
        if (OpenClipboard(owner)) {
            return TRUE;
        }
        Sleep(CLIPBOARD_RETRY_DELAY_MS);
    }

    return FALSE;
}

static BOOL TryGetClipboardTextBackup(ClipboardTextBackup *backup) {
    HANDLE handle;
    const wchar_t *locked;

    backup->has_text = FALSE;
    backup->text = NULL;

    if (!TryOpenClipboardWithRetry(g_main_window)) {
        return FALSE;
    }

    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        CloseClipboard();
        return TRUE;
    }

    handle = GetClipboardData(CF_UNICODETEXT);
    if (handle == NULL) {
        CloseClipboard();
        return TRUE;
    }

    locked = (const wchar_t *)GlobalLock(handle);
    if (locked != NULL) {
        backup->text = DupWideString(locked);
        backup->has_text = backup->text != NULL;
        GlobalUnlock(handle);
    }

    CloseClipboard();
    return TRUE;
}

static BOOL TryClearClipboard(void) {
    BOOL ok;

    if (!TryOpenClipboardWithRetry(g_main_window)) {
        return FALSE;
    }

    ok = EmptyClipboard();
    CloseClipboard();
    return ok;
}

static BOOL TryReadClipboardText(wchar_t **text) {
    HANDLE handle;
    const wchar_t *locked;

    *text = NULL;
    if (!TryOpenClipboardWithRetry(g_main_window)) {
        return FALSE;
    }

    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        CloseClipboard();
        return TRUE;
    }

    handle = GetClipboardData(CF_UNICODETEXT);
    if (handle == NULL) {
        CloseClipboard();
        return TRUE;
    }

    locked = (const wchar_t *)GlobalLock(handle);
    if (locked != NULL) {
        *text = DupWideString(locked);
        GlobalUnlock(handle);
    }

    CloseClipboard();
    return TRUE;
}

static BOOL TryWriteClipboardText(const wchar_t *text) {
    HGLOBAL global;
    wchar_t *buffer;
    size_t bytes;

    if (!TryOpenClipboardWithRetry(g_main_window)) {
        return FALSE;
    }

    if (!EmptyClipboard()) {
        CloseClipboard();
        return FALSE;
    }

    bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    global = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (global == NULL) {
        CloseClipboard();
        return FALSE;
    }

    buffer = (wchar_t *)GlobalLock(global);
    if (buffer == NULL) {
        GlobalFree(global);
        CloseClipboard();
        return FALSE;
    }

    memcpy(buffer, text, bytes);
    GlobalUnlock(global);

    if (SetClipboardData(CF_UNICODETEXT, global) == NULL) {
        GlobalFree(global);
        CloseClipboard();
        return FALSE;
    }

    CloseClipboard();
    return TRUE;
}

static DWORD WINAPI RestoreClipboardThread(LPVOID param) {
    RestoreClipboardContext *ctx = (RestoreClipboardContext *)param;

    if (ctx->delay_ms > 0) {
        Sleep(ctx->delay_ms);
    }

    if (ctx->has_text && ctx->text != NULL) {
        TryWriteClipboardText(ctx->text);
    } else {
        TryClearClipboard();
    }

    LogDebug(L"Clipboard text restored.");
    free(ctx->text);
    free(ctx);
    return 0;
}

static void QueueRestoreClipboardText(const ClipboardTextBackup *backup, DWORD delay_ms) {
    RestoreClipboardContext *ctx;
    HANDLE thread;

    ctx = (RestoreClipboardContext *)malloc(sizeof(RestoreClipboardContext));
    if (ctx == NULL) {
        return;
    }

    ctx->has_text = backup->has_text;
    ctx->text = backup->text ? DupWideString(backup->text) : NULL;
    ctx->delay_ms = delay_ms;

    thread = CreateThread(NULL, 0, RestoreClipboardThread, ctx, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
        return;
    }

    free(ctx->text);
    free(ctx);
}

static BOOL WaitForClipboardChange(DWORD initial_sequence, DWORD timeout_ms) {
    DWORD started = GetTickCount();

    while ((GetTickCount() - started) < timeout_ms) {
        if (GetClipboardSequenceNumber() != initial_sequence) {
            return TRUE;
        }
        Sleep(CLIPBOARD_POLL_DELAY_MS);
    }

    return FALSE;
}

static BOOL SendVirtualKey(WORD vk, BOOL key_up) {
    INPUT input;

    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = key_up ? KEYEVENTF_KEYUP : 0;
    return SendInput(1, &input, sizeof(input)) == 1;
}

static BOOL SendCtrlChord(WORD vk) {
    if (!SendVirtualKey(VK_CONTROL, FALSE)) return FALSE;
    Sleep(10);
    if (!SendVirtualKey(vk, FALSE)) {
        SendVirtualKey(VK_CONTROL, TRUE);
        return FALSE;
    }
    Sleep(10);
    if (!SendVirtualKey(vk, TRUE)) {
        SendVirtualKey(VK_CONTROL, TRUE);
        return FALSE;
    }
    Sleep(10);
    return SendVirtualKey(VK_CONTROL, TRUE);
}

static BOOL IsEnglishLayout(const LayoutDef *layout) {
    if (layout == NULL) {
        return FALSE;
    }

    return wcscmp(layout->code, L"en") == 0 || wcscmp(layout->code, L"en-gb") == 0;
}

static const LayoutDef *FindInstalledLayoutByCode(const wchar_t *code) {
    size_t i;

    for (i = 0; i < g_installed_layout_count; ++i) {
        if (wcscmp(g_installed_layouts[i]->code, code) == 0) {
            return g_installed_layouts[i];
        }
    }

    return NULL;
}

static const LayoutDef *GetFallbackCycleLayout(const LayoutDef *source_layout) {
    const LayoutDef *target = NULL;

    if (source_layout == NULL) {
        return NULL;
    }

    if (IsEnglishLayout(source_layout)) {
        target = FindInstalledLayoutByCode(L"ru");
        if (target == NULL) {
            target = FindInstalledLayoutByCode(L"uk");
        }
    } else if (wcscmp(source_layout->code, L"ru") == 0) {
        target = FindInstalledLayoutByCode(L"uk");
        if (target == NULL) {
            target = FindInstalledLayoutByCode(L"en");
        }
        if (target == NULL) {
            target = FindInstalledLayoutByCode(L"en-gb");
        }
    } else if (wcscmp(source_layout->code, L"uk") == 0) {
        target = FindInstalledLayoutByCode(L"en");
        if (target == NULL) {
            target = FindInstalledLayoutByCode(L"en-gb");
        }
        if (target == NULL) {
            target = FindInstalledLayoutByCode(L"ru");
        }
    }

    if (target != NULL) {
        return target;
    }

    if (g_installed_layout_count < 2) {
        return NULL;
    }

    for (size_t i = 0; i < g_installed_layout_count; ++i) {
        if (wcscmp(g_installed_layouts[i]->code, source_layout->code) == 0) {
            return g_installed_layouts[(i + 1) % g_installed_layout_count];
        }
    }

    return g_installed_layouts[0];
}

static const LayoutDef *GetNextCycleLayout(const LayoutDef *source_layout) {
    if (source_layout == NULL) {
        return NULL;
    }

    if (RefreshInstalledLayouts() && g_installed_layout_count >= 2) {
        for (size_t i = 0; i < g_installed_layout_count; ++i) {
            if (wcscmp(g_installed_layouts[i]->code, source_layout->code) == 0) {
                return g_installed_layouts[(i + g_installed_layout_count - 1) % g_installed_layout_count];
            }
        }
    }

    return GetFallbackCycleLayout(source_layout);
}

static BOOL SendKeyTap(WORD vk) {
    if (!SendVirtualKey(vk, FALSE)) {
        return FALSE;
    }

    Sleep(6);
    return SendVirtualKey(vk, TRUE);
}

static BOOL SendBackspaces(int count) {
    int i;

    for (i = 0; i < count; ++i) {
        if (!SendKeyTap(VK_BACK)) {
            return FALSE;
        }
        Sleep(4);
    }

    return TRUE;
}

static BOOL SendUnicodeChar(wchar_t ch) {
    INPUT inputs[2];

    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = ch;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

static BOOL SendUnicodeText(const wchar_t *text) {
    size_t i;

    for (i = 0; text[i] != L'\0'; ++i) {
        if (!SendUnicodeChar(text[i])) {
            return FALSE;
        }
    }

    return TRUE;
}

static const wchar_t *GetCharCategory(wchar_t ch) {
    if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z')) {
        return L"latin";
    }
    if (ch >= 0x0400 && ch <= 0x052F) {
        return L"cyrillic";
    }
    return L"other";
}

static BOOL DetectSourceTargetLayouts(const wchar_t *text, const LayoutDef **source, const LayoutDef **target) {
    size_t i;
    int latin = 0;
    int cyrillic = 0;
    const wchar_t *dominant = L"";

    *source = NULL;
    *target = NULL;

    if (g_installed_layout_count == 0) {
        RefreshInstalledLayouts();
    }

    if (g_installed_layout_count == 0) {
        return FALSE;
    }

    for (i = 0; text[i] != L'\0'; ++i) {
        const wchar_t *category = GetCharCategory(text[i]);
        if (wcscmp(category, L"latin") == 0) {
            ++latin;
        } else if (wcscmp(category, L"cyrillic") == 0) {
            ++cyrillic;
        }
    }

    if (latin > cyrillic) {
        dominant = L"latin";
    } else if (cyrillic > latin) {
        dominant = L"cyrillic";
    }

    if (dominant[0] == L'\0') {
        if (g_installed_layout_count >= 2) {
            *source = g_installed_layouts[0];
            *target = g_installed_layouts[1];
            return TRUE;
        }
        return FALSE;
    }

    for (i = 0; i < g_installed_layout_count; ++i) {
        if (wcscmp(g_installed_layouts[i]->category, dominant) == 0) {
            *source = g_installed_layouts[i];
            break;
        }
    }

    if (*source == NULL) {
        return FALSE;
    }

    for (i = 0; i < g_installed_layout_count; ++i) {
        if (wcscmp(g_installed_layouts[i]->code, (*source)->code) != 0 &&
            wcscmp(g_installed_layouts[i]->category, (*source)->category) != 0) {
            *target = g_installed_layouts[i];
            break;
        }
    }

    if (*target == NULL) {
        for (i = 0; i < g_installed_layout_count; ++i) {
            if (wcscmp(g_installed_layouts[i]->code, (*source)->code) != 0) {
                *target = g_installed_layouts[i];
                break;
            }
        }
    }

    return *source != NULL && *target != NULL;
}

static BOOL ResolveSelectedTextLayouts(const wchar_t *text, const LayoutDef **source, const LayoutDef **target) {
    const LayoutDef *current_layout = GetCurrentLayoutDef();
    const LayoutDef *next_layout = GetNextCycleLayout(current_layout);

    if (current_layout != NULL && next_layout != NULL) {
        *source = current_layout;
        *target = next_layout;
        return TRUE;
    }

    return DetectSourceTargetLayouts(text, source, target);
}

static BOOL MapCharBetweenLayouts(const LayoutDef *source, const LayoutDef *target, wchar_t ch, wchar_t *mapped) {
    size_t i;
    size_t limit = source->key_count < target->key_count ? source->key_count : target->key_count;

    for (i = 0; i < limit; ++i) {
        if (source->keys[i].normal == ch) {
            *mapped = target->keys[i].normal;
            return TRUE;
        }
        if (source->keys[i].shifted == ch) {
            *mapped = target->keys[i].shifted;
            return TRUE;
        }
    }

    return FALSE;
}

static wchar_t *ConvertTextDynamic(const wchar_t *text, const LayoutDef *source, const LayoutDef *target) {
    size_t i;
    size_t length = wcslen(text);
    wchar_t *result = (wchar_t *)malloc((length + 1) * sizeof(wchar_t));

    if (result == NULL) {
        return NULL;
    }

    for (i = 0; i < length; ++i) {
        wchar_t mapped;
        if (MapCharBetweenLayouts(source, target, text[i], &mapped)) {
            result[i] = mapped;
        } else {
            result[i] = text[i];
        }
    }

    result[length] = L'\0';
    return result;
}

static BOOL SwitchToLayoutByCode(const wchar_t *code) {
    size_t i;
    HWND foreground = GetForegroundWindow();

    if (foreground == NULL) {
        return FALSE;
    }

    RefreshInstalledLayouts();
    for (i = 0; i < g_installed_layout_count; ++i) {
        if (wcscmp(g_installed_layouts[i]->code, code) == 0) {
            PostMessageW(foreground, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)g_installed_hkls[i]);
            return TRUE;
        }
    }

    return FALSE;
}

BOOL RefreshInstalledLayouts(void) {
    int count;
    HKL *buffer;
    int actual;
    int i;
    size_t unique_count;

    ZeroMemory((void *)g_installed_layouts, sizeof(g_installed_layouts));
    ZeroMemory(g_installed_hkls, sizeof(g_installed_hkls));
    g_installed_layout_count = 0;

    count = GetKeyboardLayoutList(0, NULL);
    if (count <= 0) {
        return FALSE;
    }

    buffer = (HKL *)malloc((size_t)count * sizeof(HKL));
    if (buffer == NULL) {
        return FALSE;
    }

    actual = GetKeyboardLayoutList(count, buffer);
    unique_count = 0;

    for (i = 0; i < actual && unique_count < 16; ++i) {
        WORD lang_id = LOWORD((DWORD_PTR)buffer[i]);
        const LayoutDef *layout = FindLayoutByLangId(lang_id);
        size_t j;
        BOOL exists = FALSE;

        if (layout == NULL) {
            continue;
        }

        for (j = 0; j < unique_count; ++j) {
            if (wcscmp(g_installed_layouts[j]->code, layout->code) == 0) {
                exists = TRUE;
                break;
            }
        }

        if (!exists) {
            g_installed_layouts[unique_count] = layout;
            g_installed_hkls[unique_count] = buffer[i];
            ++unique_count;
        }
    }

    g_installed_layout_count = unique_count;
    free(buffer);
    return g_installed_layout_count > 0;
}

const LayoutDef *GetCurrentLayoutDef(void) {
    HWND foreground;
    DWORD thread_id;
    HKL hkl;
    WORD lang_id;

    foreground = GetForegroundWindow();
    if (foreground == NULL) {
        return NULL;
    }

    thread_id = GetWindowThreadProcessId(foreground, NULL);
    hkl = GetKeyboardLayout(thread_id);
    lang_id = LOWORD((DWORD_PTR)hkl);
    return FindLayoutByLangId(lang_id);
}

BOOL TransformSelectedText(void) {
    ClipboardTextBackup backup;
    DWORD initial_sequence;
    wchar_t *copied;
    wchar_t *converted;
    const LayoutDef *source;
    const LayoutDef *target;

    ZeroMemory(&backup, sizeof(backup));
    copied = NULL;
    converted = NULL;
    source = NULL;
    target = NULL;

    if (!TryGetClipboardTextBackup(&backup)) {
        LogDebug(L"Clipboard backup failed.");
        return FALSE;
    }

    LogDebug(L"Clipboard backup ok. HasText=%d.", backup.has_text ? 1 : 0);

    if (!TryClearClipboard()) {
        LogDebug(L"Failed to clear clipboard.");
        free(backup.text);
        return FALSE;
    }

    initial_sequence = GetClipboardSequenceNumber();
    if (!SendCtrlChord(VK_C_KEY)) {
        LogDebug(L"Send Ctrl+C failed.");
        QueueRestoreClipboardText(&backup, 0);
        free(backup.text);
        return FALSE;
    }

    LogDebug(L"Ctrl+C sent.");

    if (!WaitForClipboardChange(initial_sequence, CLIPBOARD_WAIT_TIMEOUT_MS)) {
        LogDebug(L"Clipboard did not change after Ctrl+C.");
        QueueRestoreClipboardText(&backup, 0);
        free(backup.text);
        return FALSE;
    }

    if (!TryReadClipboardText(&copied) || copied == NULL || copied[0] == L'\0') {
        LogDebug(L"No selected text was copied.");
        QueueRestoreClipboardText(&backup, 0);
        free(backup.text);
        free(copied);
        return FALSE;
    }

    LogDebug(L"Copied text: %ls", copied);

    if (!ResolveSelectedTextLayouts(copied, &source, &target)) {
        LogDebug(L"Could not detect source/target layouts.");
        QueueRestoreClipboardText(&backup, 0);
        free(backup.text);
        free(copied);
        return FALSE;
    }

    converted = ConvertTextDynamic(copied, source, target);
    if (converted == NULL) {
        QueueRestoreClipboardText(&backup, 0);
        free(backup.text);
        free(copied);
        return FALSE;
    }

    LogDebug(L"Converted %ls -> %ls (%ls -> %ls).", copied, converted, source->code, target->code);

    if (!TryWriteClipboardText(converted)) {
        LogDebug(L"Failed to write converted text.");
        QueueRestoreClipboardText(&backup, 0);
        free(backup.text);
        free(copied);
        free(converted);
        return FALSE;
    }

    if (!SendCtrlChord(VK_V_KEY)) {
        LogDebug(L"Send Ctrl+V failed.");
        QueueRestoreClipboardText(&backup, 0);
        free(backup.text);
        free(copied);
        free(converted);
        return FALSE;
    }

    Sleep(PASTE_SETTLE_DELAY_MS);
    SwitchToLayoutByCode(target->code);
    QueueRestoreClipboardText(&backup, PASTE_SETTLE_DELAY_MS);
    UpdateTrayLayoutIndicator(TRUE);

    free(backup.text);
    free(copied);
    free(converted);
    return TRUE;
}

BOOL TransformLastWord(void) {
    wchar_t original[LAST_WORD_MAX];
    wchar_t *converted = NULL;
    const LayoutDef *source;
    const LayoutDef *target;
    HWND foreground;
    int length;
    size_t converted_length;

    if (g_last_word.length <= 0 || g_last_word.source_layout == NULL) {
        return FALSE;
    }

    foreground = GetForegroundWindow();
    if (foreground == NULL || foreground != g_last_word.hwnd) {
        ResetLastWordTracker();
        return FALSE;
    }

    length = g_last_word.length;
    wcsncpy(original, g_last_word.text, LAST_WORD_MAX - 1);
    original[LAST_WORD_MAX - 1] = L'\0';

    source = g_last_word.source_layout;
    target = GetNextCycleLayout(source);
    if (target == NULL) {
        LogDebug(L"Last word target layout was not found.");
        return FALSE;
    }

    converted = ConvertTextDynamic(original, source, target);
    if (converted == NULL) {
        return FALSE;
    }

    if (!SendBackspaces(length)) {
        LogDebug(L"Failed to erase last word.");
        free(converted);
        return FALSE;
    }

    if (!SendUnicodeText(converted)) {
        LogDebug(L"Failed to type converted last word.");
        free(converted);
        return FALSE;
    }

    Sleep(10);
    SwitchToLayoutByCode(target->code);
    UpdateTrayLayoutIndicator(TRUE);
    LogDebug(L"Last word converted: %ls -> %ls (%ls -> %ls).", original, converted, source->code, target->code);

    converted_length = wcslen(converted);
    if (converted_length >= LAST_WORD_MAX) {
        converted_length = LAST_WORD_MAX - 1;
    }

    g_last_word.hwnd = foreground;
    g_last_word.source_layout = target;
    g_last_word.length = (int)converted_length;
    wcsncpy(g_last_word.text, converted, LAST_WORD_MAX - 1);
    g_last_word.text[LAST_WORD_MAX - 1] = L'\0';

    free(converted);
    return TRUE;
}
