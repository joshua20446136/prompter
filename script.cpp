#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <process.h>
#include <stdint.h>
#include <fstream>
#include "vosk_api.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "libvosk.lib")

using namespace std;

vector<string> script;
HWND hText;
HFONT hFont;
int currentLine = -1;
VoskModel* model;
VoskRecognizer* rec;

void LoadScript() {
    ifstream f("script.txt");
    string line;
    while (getline(f, line)) {
        if (!line.empty()) script.push_back(line);
    }
}

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
            hText = CreateWindowA("EDIT","",ES_MULTILINE|ES_READONLY|WS_CHILD|WS_VISIBLE,
                                 20,20,900,600,hWnd,(HMENU)1001,0,0);
            
            hFont = CreateFontA(32,0,0,0,FW_NORMAL,0,0,0,
                                GB2312_CHARSET,
                                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY,
                                DEFAULT_PITCH,"SimHei");
            
            SendMessage(hText, WM_SETFONT, (WPARAM)hFont, 1);
            LoadScript();
            UpdateUI();

            model = vosk_model_new("model");
            rec = vosk_recognizer_new(model, 16000.0);
            _beginthread(VoiceThread, 0, 0);
            break;

        case WM_DESTROY:
            DeleteObject(hFont);
            PostQuitMessage(0);
            break;

        default: return DefWindowProcA(hWnd,msg,wp,lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "SPEECH";
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0,"SPEECH","Speech Prompter",
                                WS_OVERLAPPEDWINDOW,100,100,960,700,0,0,hInst,0);
    ShowWindow(hWnd,nShow);
    UpdateWindow(hWnd);

    MSG msg;
    while(GetMessageA(&msg,0,0,0)){
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
