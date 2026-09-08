#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")

// Control IDs
#define IDC_COMBO_CLICK_TYPE 101
#define IDC_EDIT_HOTKEY_START 102
#define IDC_EDIT_HOTKEY_STOP 103
#define IDC_SLIDER_DELAY 104
#define IDC_EDIT_DELAY 105
#define IDC_LABEL_STATUS 106
#define IDC_BTN_RECORD_START 107
#define IDC_BTN_RECORD_STOP 108

// Global variables
bool g_running = false;
bool g_recording_start = false;
bool g_recording_stop = false;
int g_click_type = 0; // 0: Left, 1: Right, 2: Middle
int g_delay = 100;
UINT g_hotkey_start_id = 1;
UINT g_hotkey_stop_id = 2;
DWORD g_hotkey_start_mod = 0;
BYTE g_hotkey_start_key = 'J';
DWORD g_hotkey_stop_mod = 0;
BYTE g_hotkey_stop_key = 'K';

HWND g_hwnd = NULL;
HANDLE g_thread = NULL;

// Thread function for autoclicking
DWORD WINAPI ClickThread(LPVOID lpParam) {
    while (g_running) {
        INPUT input = {0};
        input.type = INPUT_MOUSE;

        switch (g_click_type) {
            case 0: // Left
                input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                SendInput(1, &input, sizeof(INPUT));
                input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(1, &input, sizeof(INPUT));
                break;
            case 1: // Right
                input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                SendInput(1, &input, sizeof(INPUT));
                input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                SendInput(1, &input, sizeof(INPUT));
                break;
            case 2: // Middle
                input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                SendInput(1, &input, sizeof(INPUT));
                input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
                SendInput(1, &input, sizeof(INPUT));
                break;
        }
        Sleep(g_delay);
    }
    return 0;
}

void UpdateStatus() {
    HWND hStatus = GetDlgItem(g_hwnd, IDC_LABEL_STATUS);
    if (hStatus) {
        SetWindowTextW(hStatus, g_running ? L"Status: RUNNING" : L"Status: STOPPED");
    }
}

void RegisterHotKeys() {
    UnregisterHotKey(g_hwnd, g_hotkey_start_id);
    UnregisterHotKey(g_hwnd, g_hotkey_stop_id);
    
    RegisterHotKey(g_hwnd, g_hotkey_start_id, g_hotkey_start_mod, g_hotkey_start_key);
    RegisterHotKey(g_hwnd, g_hotkey_stop_id, g_hotkey_stop_mod, g_hotkey_stop_key);
}

wchar_t* GetHotkeyString(DWORD mod, BYTE key, wchar_t* buffer, int size) {
    buffer[0] = 0;
    if (mod & MOD_ALT) wcscat_s(buffer, size, L"Alt+");
    if (mod & MOD_CONTROL) wcscat_s(buffer, size, L"Ctrl+");
    if (mod & MOD_SHIFT) wcscat_s(buffer, size, L"Shift+");
    if (mod & MOD_WIN) wcscat_s(buffer, size, L"Win+");
    
    wchar_t keyStr[2] = {0};
    keyStr[0] = MapVirtualKeyW(key, MAPVK_VK_TO_CHAR) & 0xFFFF;
    if (keyStr[0] == 0) {
        // Fallback for special keys
        switch(key) {
            case VK_F1: wcscpy_s(keyStr, 2, L"F1"); break;
            case VK_F2: wcscpy_s(keyStr, 2, L"F2"); break;
            case VK_F3: wcscpy_s(keyStr, 2, L"F3"); break;
            case VK_F4: wcscpy_s(keyStr, 2, L"F4"); break;
            case VK_F5: wcscpy_s(keyStr, 2, L"F5"); break;
            case VK_F6: wcscpy_s(keyStr, 2, L"F6"); break;
            case VK_F7: wcscpy_s(keyStr, 2, L"F7"); break;
            case VK_F8: wcscpy_s(keyStr, 2, L"F8"); break;
            case VK_F9: wcscpy_s(keyStr, 2, L"F9"); break;
            case VK_F10: wcscpy_s(keyStr, 2, L"F10"); break;
            case VK_F11: wcscpy_s(keyStr, 2, L"F11"); break;
            case VK_F12: wcscpy_s(keyStr, 2, L"F12"); break;
            default: 
                GetKeyNameTextW(MapVirtualKeyW(key, MAPVK_VK_TO_VSC) << 16, keyStr, 10);
                break;
        }
    } else {
        keyStr[0] = towupper(keyStr[0]);
    }
    wcscat_s(buffer, size, keyStr);
    return buffer;
}

void UpdateHotkeyDisplays() {
    wchar_t buffer[64];
    
    HWND hEditStart = GetDlgItem(g_hwnd, IDC_EDIT_HOTKEY_START);
    if (hEditStart) {
        GetHotkeyString(g_hotkey_start_mod, g_hotkey_start_key, buffer, 64);
        SetWindowTextW(hEditStart, buffer);
    }
    
    HWND hEditStop = GetDlgItem(g_hwnd, IDC_EDIT_HOTKEY_STOP);
    if (hEditStop) {
        GetHotkeyString(g_hotkey_stop_mod, g_hotkey_stop_key, buffer, 64);
        SetWindowTextW(hEditStop, buffer);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            InitCommonControlsEx(&((INITCOMMONCONTROLSEX){sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES}));
            
            // Click Type Combo
            CreateWindowW(L"STATIC", L"Click Type:", WS_CHILD | WS_VISIBLE, 10, 10, 80, 20, hwnd, NULL, NULL, NULL);
            HWND hCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 
                                        100, 10, 150, 100, hwnd, (HMENU)IDC_COMBO_CLICK_TYPE, NULL, NULL);
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Left Mouse Button");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Right Mouse Button");
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Middle Mouse Button");
            SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
            
            // Delay Slider
            CreateWindowW(L"STATIC", L"Delay (ms):", WS_CHILD | WS_VISIBLE, 10, 50, 80, 20, hwnd, NULL, NULL, NULL);
            HWND hSlider = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_TOOLTIPS,
                                         100, 50, 200, 30, hwnd, (HMENU)IDC_SLIDER_DELAY, NULL, NULL);
            SendMessageW(hSlider, TBM_SETRANGEMIN, 0, 1);
            SendMessageW(hSlider, TBM_SETRANGEMAX, 0, 10000);
            SendMessageW(hSlider, TBM_SETPOS, 1, 100);
            
            // Delay Edit
            HWND hEditDelay = CreateWindowW(L"EDIT", L"100", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                            320, 50, 60, 25, hwnd, (HMENU)IDC_EDIT_DELAY, NULL, NULL);
            
            // Hotkey Start
            CreateWindowW(L"STATIC", L"Start Hotkey:", WS_CHILD | WS_VISIBLE, 10, 90, 80, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"J", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                          100, 90, 100, 25, hwnd, (HMENU)IDC_EDIT_HOTKEY_START, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Record", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          210, 90, 70, 25, hwnd, (HMENU)IDC_BTN_RECORD_START, NULL, NULL);
            
            // Hotkey Stop
            CreateWindowW(L"STATIC", L"Stop Hotkey:", WS_CHILD | WS_VISIBLE, 10, 130, 80, 20, hwnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"K", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                          100, 130, 100, 25, hwnd, (HMENU)IDC_EDIT_HOTKEY_STOP, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Record", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          210, 130, 70, 25, hwnd, (HMENU)IDC_BTN_RECORD_STOP, NULL, NULL);
            
            // Status Label
            CreateWindowW(L"STATIC", L"Status: STOPPED", WS_CHILD | WS_VISIBLE | SS_CENTER,
                          10, 170, 200, 20, hwnd, (HMENU)IDC_LABEL_STATUS, NULL, NULL);
            
            RegisterHotKeys();
            UpdateHotkeyDisplays();
            return 0;
            
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
                    SetFocus(hwnd);
                    break;
                    
                case IDC_BTN_RECORD_STOP:
                    g_recording_stop = true;
                    g_recording_start = false;
                    SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_HOTKEY_STOP), L"Press keys...");
                    SetFocus(hwnd);
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
            
        case WM_HOTKEY:
            if (wParam == g_hotkey_start_id) {
                if (!g_running) {
                    g_running = true;
                    g_thread = CreateThread(NULL, 0, ClickThread, NULL, 0, NULL);
                    UpdateStatus();
                }
            } else if (wParam == g_hotkey_stop_id) {
                if (g_running) {
                    g_running = false;
                    if (g_thread) {
                        WaitForSingleObject(g_thread, INFINITE);
                        CloseHandle(g_thread);
                        g_thread = NULL;
                    }
                    UpdateStatus();
                }
            }
            return 0;
            
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (g_recording_start || g_recording_stop) {
                DWORD mod = 0;
                if (GetAsyncKeyState(VK_MENU) & 0x8000) mod |= MOD_ALT;
                if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mod |= MOD_SHIFT;
                if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) mod |= MOD_WIN;
                
                BYTE key = (BYTE)wParam;
                
                // Prevent modifier-only hotkeys
                if (key == VK_MENU || key == VK_CONTROL || key == VK_SHIFT || key == VK_LWIN || key == VK_RWIN) {
                    return 0;
                }
                
                if (g_recording_start) {
                    g_hotkey_start_mod = mod;
                    g_hotkey_start_key = key;
                    g_recording_start = false;
                    RegisterHotKeys();
                    UpdateHotkeyDisplays();
                } else if (g_recording_stop) {
                    g_hotkey_stop_mod = mod;
                    g_hotkey_stop_key = key;
                    g_recording_stop = false;
                    RegisterHotKeys();
                    UpdateHotkeyDisplays();
                }
            }
            return 0;
            
        case WM_DESTROY:
            g_running = false;
            if (g_thread) {
                WaitForSingleObject(g_thread, INFINITE);
                CloseHandle(g_thread);
            }
            UnregisterHotKey(hwnd, g_hotkey_start_id);
            UnregisterHotKey(hwnd, g_hotkey_stop_id);
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
        0, CLASS_NAME, L"AutoClicker",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 250,
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
