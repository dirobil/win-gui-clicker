#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")

// --- Идентификаторы элементов управления ---
#define IDC_COMBO_CLICK_TYPE    101
#define IDC_EDIT_HOTKEY_START   102
#define IDC_EDIT_HOTKEY_STOP    103
#define IDC_SLIDER_DELAY        104
#define IDC_EDIT_DELAY          105
#define IDC_LABEL_STATUS        106
#define IDC_BTN_RECORD_START    107
#define IDC_BTN_RECORD_STOP     108

// --- Глобальные переменные ---
volatile bool g_running = false;
volatile bool g_recording_start = false;
volatile bool g_recording_stop = false;
int g_click_type = 0; // 0:LBtn, 1:RBtn, 2:MBtn, 3:Space, 4:E
int g_delay = 100;

// Хранение горячих клавиш (модификаторы + код клавиши)
DWORD g_start_mod = 0;
BYTE g_start_key = 'J';
DWORD g_stop_mod = 0;
BYTE g_stop_key = 'K';

// Хук
HHOOK g_hook = NULL;
HWND g_hwnd = NULL;
HANDLE g_thread = NULL;

// --- Функция потока для эмуляции кликов ---
DWORD WINAPI ClickThread(LPVOID lpParam) {
    while (g_running) {
        INPUT input[2] = {0};
        
        // Подготовка событий нажатия и отпускания
        switch (g_click_type) {
            case 0: // ЛКМ
                input[0].type = INPUT_MOUSE; input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                input[1].type = INPUT_MOUSE; input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                break;
            case 1: // ПКМ
                input[0].type = INPUT_MOUSE; input[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                input[1].type = INPUT_MOUSE; input[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                break;
            case 2: // СКМ
                input[0].type = INPUT_MOUSE; input[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                input[1].type = INPUT_MOUSE; input[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
                break;
            case 3: // Space
                input[0].type = INPUT_KEYBOARD; input[0].ki.wVk = VK_SPACE;
                input[1].type = INPUT_KEYBOARD; input[1].ki.wVk = VK_SPACE; input[1].ki.dwFlags = KEYEVENTF_KEYUP;
                break;
            case 4: // E
                input[0].type = INPUT_KEYBOARD; input[0].ki.wVk = 'E';
                input[1].type = INPUT_KEYBOARD; input[1].ki.wVk = 'E'; input[1].ki.dwFlags = KEYEVENTF_KEYUP;
                break;
        }

        // Отправка событий
        SendInput(2, input, sizeof(INPUT));
        
        // Задержка между кликами
        Sleep(g_delay);
    }
    return 0;
}

// --- Обновление статуса ---
void UpdateStatus() {
    HWND hStatus = GetDlgItem(g_hwnd, IDC_LABEL_STATUS);
    if (hStatus) {
        SetWindowTextW(hStatus, g_running ? L"Status: RUNNING" : L"Status: STOPPED");
    }
}

// --- Преобразование кода клавиши в строку ---
void GetHotkeyString(DWORD mod, BYTE key, wchar_t* buffer, int size) {
    buffer[0] = 0;
    if (mod & MOD_ALT) wcscat_s(buffer, size, L"Alt+");
    if (mod & MOD_CONTROL) wcscat_s(buffer, size, L"Ctrl+");
    if (mod & MOD_SHIFT) wcscat_s(buffer, size, L"Shift+");
    if (mod & MOD_WIN) wcscat_s(buffer, size, L"Win+");
    
    wchar_t keyStr[64] = {0};
    // Получаем имя клавиши
    UINT scanCode = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
    if (!GetKeyNameTextW(scanCode << 16, keyStr, 64)) {
        // Если имя не найдено, пробуем получить символ или код
        SHORT ascii = MapVirtualKeyW(key, MAPVK_VK_TO_CHAR) & 0xFFFF;
        if (ascii >= 32 && ascii < 127) {
            keyStr[0] = (wchar_t)towupper(ascii);
            keyStr[1] = L'\0';
        } else {
            swprintf_s(keyStr, L"VK_%d", key);
        }
    }
    wcscat_s(buffer, size, keyStr);
}

// --- Обновление полей ввода горячих клавиш ---
void UpdateHotkeyDisplays() {
    wchar_t buffer[64];
    
    HWND hEditStart = GetDlgItem(g_hwnd, IDC_EDIT_HOTKEY_START);
    if (hEditStart) {
        GetHotkeyString(g_start_mod, g_start_key, buffer, 64);
        SetWindowTextW(hEditStart, buffer);
    }
    
    HWND hEditStop = GetDlgItem(g_hwnd, IDC_EDIT_HOTKEY_STOP);
    if (hEditStop) {
        GetHotkeyString(g_stop_mod, g_stop_key, buffer, 64);
        SetWindowTextW(hEditStop, buffer);
    }
}

// --- Низкоуровневый хук клавиатуры ---
LRESULT CALLBACK KeyboardHook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        BYTE vk = (BYTE)p->vkCode;

        // Игнорируем повторные нажатия (автоповтор), реагируем только на первое
        if (p->flags & LLKHF_INJECTED) return CallNextHookEx(g_hook, nCode, wParam, lParam);

        // Сбор текущих модификаторов
        DWORD currentMod = 0;
        if (GetAsyncKeyState(VK_MENU) & 0x8000) currentMod |= MOD_ALT;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) currentMod |= MOD_CONTROL;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) currentMod |= MOD_SHIFT;
        if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) currentMod |= MOD_WIN;

        // Если мы в режиме записи горячей клавиши
        if (g_recording_start || g_recording_stop) {
            // Не считаем чистые модификаторы за клавишу
            if (vk == VK_MENU || vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_LWIN || vk == VK_RWIN) {
                return CallNextHookEx(g_hook, nCode, wParam, lParam);
            }

            if (g_recording_start) {
                g_start_mod = currentMod;
                g_start_key = vk;
                g_recording_start = false;
                UpdateHotkeyDisplays();
                SetFocus(g_hwnd); // Возвращаем фокус окну
                return -1; // Блокируем передачу этого нажатия дальше
            } else if (g_recording_stop) {
                g_stop_mod = currentMod;
                g_stop_key = vk;
                g_recording_stop = false;
                UpdateHotkeyDisplays();
                SetFocus(g_hwnd);
                return -1;
            }
        } 
        // Если НЕ в режиме записи, проверяем запуск/остановку
        else {
            bool isStart = (currentMod == g_start_mod && vk == g_start_key);
            bool isStop = (currentMod == g_stop_mod && vk == g_stop_key);

            if (isStart) {
                if (!g_running) {
                    g_running = true;
                    g_thread = CreateThread(NULL, 0, ClickThread, NULL, 0, NULL);
                    UpdateStatus();
                }
                return -1; // Блокируем, чтобы игра не получила нажатие
            } else if (isStop) {
                if (g_running) {
                    g_running = false;
                    if (g_thread) {
                        WaitForSingleObject(g_thread, INFINITE);
                        CloseHandle(g_thread);
                        g_thread = NULL;
                    }
                    UpdateStatus();
                }
                return -1;
            }
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

// --- Процедура окна ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
            InitCommonControlsEx(&icex);
            
            // Тип клика
            CreateWindowW(L"STATIC", L"Action:", WS_CHILD | WS_VISIBLE, 10, 10, 80, 20, hwnd, NULL, NULL, NULL);
            HWND hCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 
                                        100, 10, 150, 150, hwnd, (HMENU)IDC_COMBO_CLICK_TYPE, NULL, NULL);
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Left Mouse Button");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Right Mouse Button");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Middle Mouse Button");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Space Key");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"E Key");
            SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
            
            // Задержка
            CreateWindowW(L"STATIC", L"Delay (ms):", WS_CHILD | WS_VISIBLE, 10, 50, 80, 20, hwnd, NULL, NULL, NULL);
            HWND hSlider = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_TOOLTIPS,
                                         100, 50, 200, 30, hwnd, (HMENU)IDC_SLIDER_DELAY, NULL, NULL);
            SendMessageW(hSlider, TBM_SETRANGEMIN, 0, 1);
            SendMessageW(hSlider, TBM_SETRANGEMAX, 0, 10000);
            SendMessageW(hSlider, TBM_SETPOS, 1, 100);
            
            HWND hEditDelay = CreateWindowW(L"EDIT", L"100", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                            320, 50, 60, 25, hwnd, (HMENU)IDC_EDIT_DELAY, NULL, NULL);
            
            // Старт
            CreateWindowW(L"STATIC", L"Start Hotkey:", WS_CHILD | WS_VISIBLE, 10, 90, 80, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"J", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                          100, 90, 100, 25, hwnd, (HMENU)IDC_EDIT_HOTKEY_START, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Record", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          210, 90, 70, 25, hwnd, (HMENU)IDC_BTN_RECORD_START, NULL, NULL);
            
            // Стоп
            CreateWindowW(L"STATIC", L"Stop Hotkey:", WS_CHILD | WS_VISIBLE, 10, 130, 80, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"K", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                          100, 130, 100, 25, hwnd, (HMENU)IDC_EDIT_HOTKEY_STOP, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Record", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          210, 130, 70, 25, hwnd, (HMENU)IDC_BTN_RECORD_STOP, NULL, NULL);
            
            // Статус
            CreateWindowW(L"STATIC", L"Status: STOPPED", WS_CHILD | WS_VISIBLE | SS_CENTER,
                          10, 170, 200, 20, hwnd, (HMENU)IDC_LABEL_STATUS, NULL, NULL);
            
            // Установка хука
            g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, NULL, 0);
            if (!g_hook) {
                MessageBoxW(hwnd, L"Failed to install keyboard hook!", L"Error", MB_ICONERROR);
            }
            
            UpdateHotkeyDisplays();
            return 0;
        }
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_COMBO_CLICK_TYPE:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        g_click_type = (int)SendMessageW((HWND)lParam, CB_GETCURSEL, 0, 0);
                    }
                    break;
                    
                case IDC_EDIT_DELAY:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        wchar_t buf[16];
                        GetWindowTextW((HWND)lParam, buf, 16);
                        int val = _wtoi(buf);
                        if (val < 1) val = 1;
                        if (val > 10000) val = 10000;
                        g_delay = val;
                        SendMessageW(GetDlgItem(hwnd, IDC_SLIDER_DELAY), TBM_SETPOS, 1, val);
                    }
                    break;
                    
                case IDC_BTN_RECORD_START:
                    g_recording_start = true;
                    g_recording_stop = false;
                    SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_HOTKEY_START), L"Press keys...");
                    // Не меняем фокус насильно, чтобы пользователь мог нажать комбинацию сразу
                    break;
                    
                case IDC_BTN_RECORD_STOP:
                    g_recording_stop = true;
                    g_recording_start = false;
                    SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_HOTKEY_STOP), L"Press keys...");
                    break;
            }
            return 0;
            
        case WM_HSCROLL:
            if ((HWND)lParam == GetDlgItem(hwnd, IDC_SLIDER_DELAY)) {
                int pos = (int)SendMessageW((HWND)lParam, TBM_GETPOS, 0, 0);
                g_delay = pos;
                
                wchar_t buf[16];
                _itow_s(pos, buf, 10);
                SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_DELAY), buf);
            }
            return 0;
            
        case WM_DESTROY:
            g_running = false;
            if (g_thread) {
                WaitForSingleObject(g_thread, INFINITE);
                CloseHandle(g_thread);
            }
            if (g_hook) UnhookWindowsHookEx(g_hook);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"AutoClickerClass";
    
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    RegisterClassW(&wc);
    
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Advanced AutoClicker",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 260,
        NULL, NULL, hInstance, NULL
    );
    
    if (hwnd == NULL) return 0;
    
    g_hwnd = hwnd;
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
