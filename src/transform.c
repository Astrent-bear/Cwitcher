#include "app.h"

#define MAX_INSTALLED_LAYOUTS 32

static const LayoutDef *g_installed_layouts[MAX_INSTALLED_LAYOUTS] = { 0 };
static HKL g_installed_hkls[MAX_INSTALLED_LAYOUTS] = { 0 };
static size_t g_installed_layout_count = 0;

#include "layout_data.inc"

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

static const wchar_t *GetTokenOrEmpty(const wchar_t *token) {
    return token != NULL ? token : L"";
}

static size_t GetTokenLength(const wchar_t *token) {
    return wcslen(GetTokenOrEmpty(token));
}

static size_t GetTokenPrefixMatchLength(const wchar_t *text, const wchar_t *token) {
    size_t length;

    token = GetTokenOrEmpty(token);
    length = wcslen(token);
    if (length == 0) {
        return 0;
    }

    if (wcsncmp(text, token, length) == 0) {
        return length;
    }

    return 0;
}

static BOOL FindLayoutTokenMatch(const LayoutDef *layout, const wchar_t *text, size_t *key_index, BOOL *use_shifted, size_t *match_length) {
    size_t i;
    size_t best_length = 0;
    size_t best_index = 0;
    BOOL best_shifted = FALSE;

    for (i = 0; i < layout->key_count; ++i) {
        size_t normal_length = GetTokenPrefixMatchLength(text, layout->keys[i].normal);
        if (normal_length > best_length) {
            best_length = normal_length;
            best_index = i;
            best_shifted = FALSE;
        }

        size_t shifted_length = GetTokenPrefixMatchLength(text, layout->keys[i].shifted);
        if (shifted_length > best_length) {
            best_length = shifted_length;
            best_index = i;
            best_shifted = TRUE;
        }
    }

    if (best_length == 0) {
        return FALSE;
    }

    *key_index = best_index;
    *use_shifted = best_shifted;
    *match_length = best_length;
    return TRUE;
}

static int ScoreTextForLayout(const wchar_t *text, const LayoutDef *layout) {
    size_t index = 0;
    int score = 0;

    while (text[index] != L'\0') {
        size_t key_index;
        size_t match_length;
        BOOL use_shifted;

        if (FindLayoutTokenMatch(layout, text + index, &key_index, &use_shifted, &match_length)) {
            score += (int)match_length;
            index += match_length;
        } else {
            ++index;
        }
    }

    return score;
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

static BOOL DetectSourceTargetLayouts(const wchar_t *text, const LayoutDef **source, const LayoutDef **target) {
    size_t i;
    int best_score = -1;

    *source = NULL;
    *target = NULL;

    if (g_installed_layout_count == 0) {
        RefreshInstalledLayouts();
    }

    if (g_installed_layout_count == 0) {
        return FALSE;
    }

    for (i = 0; i < g_installed_layout_count; ++i) {
        int score = ScoreTextForLayout(text, g_installed_layouts[i]);
        if (score > best_score) {
            best_score = score;
            *source = g_installed_layouts[i];
        }
    }

    if (*source == NULL || best_score <= 0) {
        return FALSE;
    }

    *target = GetNextCycleLayout(*source);

    if (*target == NULL || wcscmp((*target)->code, (*source)->code) == 0) {
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

static const wchar_t *MapTokenBetweenLayouts(const LayoutDef *source, const LayoutDef *target, const wchar_t *text, size_t *consumed_length) {
    size_t key_index;
    size_t match_length;
    BOOL use_shifted;
    const wchar_t *mapped;

    if (!FindLayoutTokenMatch(source, text, &key_index, &use_shifted, &match_length)) {
        return NULL;
    }

    *consumed_length = match_length;
    mapped = use_shifted ? target->keys[key_index].shifted : target->keys[key_index].normal;
    if (GetTokenLength(mapped) == 0) {
        return use_shifted ? source->keys[key_index].shifted : source->keys[key_index].normal;
    }

    return mapped;
}

static wchar_t *ConvertTextDynamic(const wchar_t *text, const LayoutDef *source, const LayoutDef *target) {
    size_t index = 0;
    size_t written = 0;
    size_t capacity = (wcslen(text) * 4) + 16;
    wchar_t *result = (wchar_t *)malloc(capacity * sizeof(wchar_t));

    if (result == NULL) {
        return NULL;
    }

    while (text[index] != L'\0') {
        size_t consumed_length = 0;
        const wchar_t *mapped = MapTokenBetweenLayouts(source, target, text + index, &consumed_length);
        size_t mapped_length;

        if (mapped == NULL || consumed_length == 0) {
            if (written + 2 > capacity) {
                size_t new_capacity = capacity * 2;
                wchar_t *expanded = (wchar_t *)realloc(result, new_capacity * sizeof(wchar_t));
                if (expanded == NULL) {
                    free(result);
                    return NULL;
                }
                result = expanded;
                capacity = new_capacity;
            }

            result[written++] = text[index++];
        } else {
            mapped = GetTokenOrEmpty(mapped);
            mapped_length = wcslen(mapped);
            if (written + mapped_length + 1 > capacity) {
                size_t new_capacity = capacity;
                while (written + mapped_length + 1 > new_capacity) {
                    new_capacity *= 2;
                }

                wchar_t *expanded = (wchar_t *)realloc(result, new_capacity * sizeof(wchar_t));
                if (expanded == NULL) {
                    free(result);
                    return NULL;
                }
                result = expanded;
                capacity = new_capacity;
            }

            memcpy(result + written, mapped, mapped_length * sizeof(wchar_t));
            written += mapped_length;
            index += consumed_length;
        }
    }

    result[written] = L'\0';
    return result;
}

static BOOL BuildSelfTestReportPath(wchar_t *path, size_t capacity) {
    wchar_t *slash;

    if (GetModuleFileNameW(NULL, path, (DWORD)capacity) == 0) {
        return FALSE;
    }

    slash = wcsrchr(path, L'\\');
    if (slash == NULL) {
        return FALSE;
    }

    slash[1] = L'\0';
    return SUCCEEDED(StringCchCatW(path, capacity, L"layout-selftest-report.txt"));
}

static void WriteSelfTestLine(FILE *file, const wchar_t *format, ...) {
    wchar_t buffer[1024];
    va_list args;

    if (file == NULL) {
        return;
    }

    va_start(args, format);
    vswprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), format, args);
    va_end(args);

    fputws(buffer, file);
    fputws(L"\n", file);
}

static BOOL TokensEqual(const wchar_t *left, const wchar_t *right) {
    return wcscmp(GetTokenOrEmpty(left), GetTokenOrEmpty(right)) == 0;
}

static void ReportLayoutDuplicateTokens(FILE *report, const LayoutDef *layout, int *warning_count, int *detail_count) {
    size_t i;
    size_t j;

    for (i = 0; i < layout->key_count; ++i) {
        for (j = i + 1; j < layout->key_count; ++j) {
            if (GetTokenLength(layout->keys[i].normal) > 0 &&
                TokensEqual(layout->keys[i].normal, layout->keys[j].normal)) {
                ++(*warning_count);
                if (*detail_count < 256) {
                    WriteSelfTestLine(report,
                        L"[warn] %ls has duplicate normal token %ls at keys %zu and %zu.",
                        layout->code, layout->keys[i].normal, i, j);
                    ++(*detail_count);
                }
            }

            if (GetTokenLength(layout->keys[i].shifted) > 0 &&
                TokensEqual(layout->keys[i].shifted, layout->keys[j].shifted)) {
                ++(*warning_count);
                if (*detail_count < 256) {
                    WriteSelfTestLine(report,
                        L"[warn] %ls has duplicate shifted token %ls at keys %zu and %zu.",
                        layout->code, layout->keys[i].shifted, i, j);
                    ++(*detail_count);
                }
            }
        }
    }
}

static int CountTokenOccurrencesInLayout(const LayoutDef *layout, const wchar_t *token) {
    size_t i;
    int count = 0;

    if (GetTokenLength(token) == 0) {
        return 0;
    }

    for (i = 0; i < layout->key_count; ++i) {
        if (TokensEqual(layout->keys[i].normal, token)) {
            ++count;
        }
        if (TokensEqual(layout->keys[i].shifted, token)) {
            ++count;
        }
    }

    return count;
}

static void ReportLayoutTokenIssues(FILE *report, const LayoutDef *layout, int *fatal_count, int *warning_count, int *detail_count) {
    size_t i;

    if (layout->key_count == 0) {
        ++(*fatal_count);
        if (*detail_count < 256) {
            WriteSelfTestLine(report, L"[fatal] %ls has no keys.", layout->code);
            ++(*detail_count);
        }
    }

    for (i = 0; i < layout->key_count; ++i) {
        if (layout->keys[i].normal == NULL || layout->keys[i].shifted == NULL) {
            ++(*fatal_count);
            if (*detail_count < 256) {
                WriteSelfTestLine(report, L"[fatal] %ls has a null token pointer at key index %zu.", layout->code, i);
                ++(*detail_count);
            }
            continue;
        }

        if (GetTokenLength(layout->keys[i].normal) == 0) {
            ++(*fatal_count);
            if (*detail_count < 256) {
                WriteSelfTestLine(report, L"[fatal] %ls has an empty normal token at key index %zu.", layout->code, i);
                ++(*detail_count);
            }
        }

        if (GetTokenLength(layout->keys[i].shifted) == 0) {
            ++(*warning_count);
            if (*detail_count < 256) {
                WriteSelfTestLine(report, L"[warn] %ls has an empty shifted token at key index %zu.", layout->code, i);
                ++(*detail_count);
            }
        }
    }

    ReportLayoutDuplicateTokens(report, layout, warning_count, detail_count);
}

static void ReportRoundTripIssues(FILE *report, const LayoutDef *source, const LayoutDef *target, int *fatal_count, int *warning_count, int *detail_count) {
    size_t i;
    size_t limit = source->key_count < target->key_count ? source->key_count : target->key_count;

    for (i = 0; i < limit; ++i) {
        int slot;

        for (slot = 0; slot < 2; ++slot) {
            const wchar_t *source_token = slot == 0 ? source->keys[i].normal : source->keys[i].shifted;
            const wchar_t *expected_token = slot == 0 ? target->keys[i].normal : target->keys[i].shifted;
            BOOL ambiguous_source_token;
            wchar_t *converted;
            wchar_t *roundtrip;

            if (GetTokenLength(source_token) == 0) {
                continue;
            }

            ambiguous_source_token = CountTokenOccurrencesInLayout(source, source_token) > 1;

            converted = ConvertTextDynamic(source_token, source, target);
            if (converted == NULL) {
                ++(*fatal_count);
                if (*detail_count < 256) {
                    WriteSelfTestLine(report, L"[fatal] Conversion allocation failed for %ls -> %ls.", source->code, target->code);
                    ++(*detail_count);
                }
                return;
            }

            if (GetTokenLength(expected_token) == 0) {
                expected_token = source_token;
            }

            if (!ambiguous_source_token && wcscmp(converted, expected_token) != 0) {
                ++(*warning_count);
                if (*detail_count < 256) {
                    WriteSelfTestLine(report,
                        L"[warn] %ls -> %ls exact token mismatch at key %zu slot %d: got %ls, expected %ls.",
                        source->code, target->code, i, slot, converted, expected_token);
                    ++(*detail_count);
                }
                free(converted);
                continue;
            }

            roundtrip = ConvertTextDynamic(converted, target, source);
            if (roundtrip == NULL) {
                ++(*fatal_count);
                if (*detail_count < 256) {
                    WriteSelfTestLine(report, L"[fatal] Round-trip allocation failed for %ls -> %ls.", source->code, target->code);
                    ++(*detail_count);
                }
                free(converted);
                return;
            }

            if (!ambiguous_source_token && wcscmp(roundtrip, source_token) != 0) {
                ++(*warning_count);
                if (*detail_count < 256) {
                    WriteSelfTestLine(report,
                        L"[warn] %ls -> %ls round-trip changed %ls into %ls.",
                        source->code, target->code, source_token, roundtrip);
                    ++(*detail_count);
                }
            }

            free(converted);
            free(roundtrip);
        }
    }
}

int RunLayoutSelfTest(void) {
    wchar_t report_path[MAX_PATH];
    FILE *report = NULL;
    int fatal_count = 0;
    int warning_count = 0;
    int detail_count = 0;
    size_t i;
    size_t j;

    if (BuildSelfTestReportPath(report_path, sizeof(report_path) / sizeof(report_path[0]))) {
        report = _wfopen(report_path, L"w, ccs=UTF-8");
    }

    WriteSelfTestLine(report, L"Cwitcher layout self-test");
    WriteSelfTestLine(report, L"Supported layouts: %zu", sizeof(ALL_LAYOUTS) / sizeof(ALL_LAYOUTS[0]));

    for (i = 0; i < sizeof(ALL_LAYOUTS) / sizeof(ALL_LAYOUTS[0]); ++i) {
        ReportLayoutTokenIssues(report, &ALL_LAYOUTS[i], &fatal_count, &warning_count, &detail_count);
    }

    for (i = 0; i < sizeof(ALL_LAYOUTS) / sizeof(ALL_LAYOUTS[0]); ++i) {
        for (j = 0; j < sizeof(ALL_LAYOUTS) / sizeof(ALL_LAYOUTS[0]); ++j) {
            if (i == j) {
                continue;
            }

            ReportRoundTripIssues(report, &ALL_LAYOUTS[i], &ALL_LAYOUTS[j], &fatal_count, &warning_count, &detail_count);
        }
    }

    WriteSelfTestLine(report, L"Summary: fatal=%d warning=%d", fatal_count, warning_count);
    if (report != NULL) {
        fclose(report);
    }

    if (fatal_count > 0) {
        return 1;
    }

    return warning_count > 0 ? 2 : 0;
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

    for (i = 0; i < actual && unique_count < MAX_INSTALLED_LAYOUTS; ++i) {
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
