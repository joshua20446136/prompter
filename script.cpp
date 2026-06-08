#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <process.h>
#include <stdint.h>
#include "vosk_api.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "libvosk.lib")

using namespace std;

// 【不乱码核心】中文统一使用 UTF-8 字符串
vector<string> script = {
    "尊敬的各位领导、各位来宾，大家好。",
    "今天非常荣幸能够站在这里进行演讲。",
    "我今天演讲的主题是科技创新与未来发展。",
    "科技是第一生产力，创新是引领发展的动力。",
    "面对新时代，我们必须坚持自主创新。",
    "让我们携手共进，创造更加美好的明天。",
    "我的演讲完毕，谢谢大家。"
};

#define ID_TEXT 1001
HWND hText;
HFONT hFont;
int currentLine = -1;
VoskModel* model;
VoskRecognizer* rec;

int matchScore(const string& a, const string& b) {
    int s = 0;
    for (char c : b) if (a.find(c) != string::npos) s++;
    return s;
}

int findBestMatch(const string& text) {
    int best = 0, m = 0;
    for (int i=0;i<script.size();i++) {
        int sc = matchScore(script[i], text);
        if (sc>m) {m=sc;best=i;}
    }
    return best;
}

void UpdateUI() {
    string buf;
    for (int i=0;i<script.size();i++) {
        if (i==currentLine) buf += "-> " + script[i] + "\r\n\r\n";
        else buf += "   " + script[i] + "\r\n\r\n";
    }
    SetWindowTextA(hText, buf.c_str());
}

void __cdecl VoiceThread(void*) {
    WAVEFORMATEX wf{};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = 16000;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 2;

    HWAVEIN hwi;
    waveInOpen(&hwi, WAVE_MAPPER, &wf, 0,0,CALLBACK_NULL);
    waveInStart(hwi);

    char buf[4096];
    while (1) {
        WAVEHDR wh{};
        wh.lpData = buf;
        wh.dwBufferLength = sizeof(buf);
        waveInPrepareHeader(hwi, &wh, sizeof(WAVEHDR));
        waveInAddBuffer(hwi, &wh, sizeof(WAVEHDR));
        Sleep(100);

        vosk_recognizer_accept_waveform(rec, (const char*)buf, sizeof(buf));
        string res = vosk_recognizer_result(rec);
        size_t p = res.find("\"text\":\"");
        if (p != string::npos) {
            p+=8;
            size_t e = res.find("\"",p);
            string txt = res.substr(p,e-p);
            if (!txt.empty()) {
                currentLine = findBestMatch(txt);
                UpdateUI();
            }
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch(msg) {
        case WM_CREATE:
            hText = CreateWindowA("EDIT", "", ES_MULTILINE | ES_READONLY | WS_CHILD | WS_VISIBLE,
                20, 20, 900, 600, hWnd, (HMENU)ID_TEXT, 0, 0);

            // 【不乱码】使用宋体 / 微软雅黑，兼容中文
            hFont = CreateFontA(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                GB2312_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY,
                DEFAULT_PITCH, "SimHei");

            SendMessage(hText, WM_SETFONT, (WPARAM)hFont, TRUE);
            UpdateUI();

            model = vosk_model_new("model");
            rec = vosk_recognizer_new(model, 16000.0);
            _beginthread(VoiceThread, 0, 0);
            break;

        case WM_DESTROY:
            DeleteObject(hFont);
            vosk_recognizer_free(rec);
            vosk_model_free(model);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcA(hWnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "SPEECH";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0, "SPEECH", "演讲提词器",
        WS_OVERLAPPEDWINDOW, 100, 100, 960, 700, NULL, NULL, hInst, 0);

    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
