#include "Menu.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>
#include <conio.h>
#include <windows.h>
#include <cstdlib>
#include <cstdio>

std::string g_IniPath = "C:\\BYOND\\config.ini";

static const int MAX_HISTORY = 100;

static std::string GetHistoryPath()
{
    std::string path = g_IniPath.empty() ? std::string("C:\\BYOND\\config.ini") : g_IniPath;
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos)
        return "C:\\BYOND\\cid_history.ini";
    return path.substr(0, pos + 1) + "cid_history.ini";
}

unsigned long ReadVolumeFromConfig(const char* iniPath)
{
    const char* path = (iniPath && iniPath[0]) ? iniPath : "C:\\BYOND\\config.ini";
    char buf[32] = { 0 };
    GetPrivateProfileStringA("Settings", "Volume", "94545", buf, sizeof(buf), path);
    unsigned long v = strtoul(buf, nullptr, 10);
    if (v == 0)
        v = 94545;
    return v;
}

void WriteVolumeToConfig(const char* iniPath, unsigned long volume)
{
    const char* path = (iniPath && iniPath[0]) ? iniPath : "C:\\BYOND\\config.ini";
    char buf[32];
    sprintf_s(buf, sizeof(buf), "%lu", volume);
    WritePrivateProfileStringA("Settings", "Volume", buf, path);
}

void SetColor(int color)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

int GetSetting(const char* section, const char* key, int defaultVal)
{
    return GetPrivateProfileIntA(section, key, defaultVal, g_IniPath.c_str());
}

void SetSetting(const char* section, const char* key, int value)
{
    std::string valStr = std::to_string(value);
    WritePrivateProfileStringA(section, key, valStr.c_str(), g_IniPath.c_str());
}

void ClearConsole()
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(h, &csbi))
        return;

    DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written = 0;
    COORD home{ 0, 0 };

    FillConsoleOutputCharacterA(h, ' ', cells, home, &written);
    FillConsoleOutputAttribute(h, csbi.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(h, home);
}

static bool ContainsId(const std::vector<unsigned long>& ids, unsigned long id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

static std::vector<unsigned long> ReadHistory(const std::string& path)
{
    std::vector<unsigned long> ids;

    char buf[16] = { 0 };
    GetPrivateProfileStringA("History", "Count", "0", buf, sizeof(buf), path.c_str());

    int count = atoi(buf);
    if (count < 0) count = 0;
    if (count > 1000) count = 1000;

    for (int i = 1; i <= count; ++i)
    {
        char key[16];
        sprintf_s(key, sizeof(key), "ID%d", i);

        char val[32] = { 0 };
        GetPrivateProfileStringA("History", key, "", val, sizeof(val), path.c_str());

        if (val[0])
        {
            unsigned long v = strtoul(val, nullptr, 10);
            if (v != 0)
                ids.push_back(v);
        }
    }

    return ids;
}

static void WriteHistory(const std::string& path, std::vector<unsigned long> ids)
{
    if (ids.size() > static_cast<size_t>(MAX_HISTORY))
    {
        size_t removeCount = ids.size() - static_cast<size_t>(MAX_HISTORY);
        ids.erase(ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(removeCount));
    }

    char buf[16];
    sprintf_s(buf, sizeof(buf), "%d", static_cast<int>(ids.size()));
    WritePrivateProfileStringA("History", "Count", buf, path.c_str());

    for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    {
        char key[16];
        char val[32];

        sprintf_s(key, sizeof(key), "ID%d", i + 1);
        sprintf_s(val, sizeof(val), "%lu", ids[i]);

        WritePrivateProfileStringA("History", key, val, path.c_str());
    }
}

static void AddHistoryId(unsigned long id)
{
    std::string path = GetHistoryPath();
    std::vector<unsigned long> ids = ReadHistory(path);

    if (!ContainsId(ids, id))
    {
        ids.push_back(id);
        WriteHistory(path, ids);
    }
}

static void ClearHistory()
{
    std::string path = GetHistoryPath();

    WritePrivateProfileStringA("History", "Count", "0", path.c_str());

    for (int i = 1; i <= MAX_HISTORY; ++i)
    {
        char key[16];
        sprintf_s(key, sizeof(key), "ID%d", i);
        WritePrivateProfileStringA("History", key, NULL, path.c_str());
    }
}

static unsigned long GenerateUniqueVolume()
{
    std::string historyPath = GetHistoryPath();
    std::vector<unsigned long> avoid = ReadHistory(historyPath);

    unsigned long current = ReadVolumeFromConfig(g_IniPath.c_str());
    if (!ContainsId(avoid, current))
        avoid.push_back(current);

    static std::mt19937 gen(
        static_cast<unsigned>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
            )
    );

    std::uniform_int_distribution<unsigned long> dist(1UL, 0xFFFFFFFFUL);

    for (int i = 0; i < 20000; ++i)
    {
        unsigned long candidate = dist(gen);
        if (!ContainsId(avoid, candidate))
            return candidate;
    }

    unsigned long long raw =
        static_cast<unsigned long long>(
            std::chrono::system_clock::now().time_since_epoch().count()
            ) ^ GetTickCount64();

    unsigned long start = static_cast<unsigned long>(raw);
    if (start == 0)
        start = 1;

    for (unsigned long i = 0; i < 0x10000UL; ++i)
    {
        unsigned long candidate = start + i;
        if (candidate == 0)
            candidate = 1;

        if (!ContainsId(avoid, candidate))
            return candidate;
    }

    return 1;
}

void DrawMenu()
{
    ClearConsole();

    int spoofCID = GetSetting("Hooks", "GetVolumeInformationA", 1);
    int noAds = GetSetting("Hooks", "IsByondMember", 1);
    int blockLinks = GetSetting("Hooks", "BrowseLink", 0);
    int logChat = GetSetting("Settings", "LogToHtml", 0);
    int logCommands = GetSetting("Hooks", "CommandEvent", 1);
    int spoofOs = GetSetting("Hooks", "GetVersionExW", 1);

    unsigned long volume = ReadVolumeFromConfig(g_IniPath.c_str());
    std::vector<unsigned long> hist = ReadHistory(GetHistoryPath());

    SetColor(11);
    std::cout << "==================================================\n";
    std::cout << "          " << APOFEOS_UPPER_U8 << " // BYOND HOOK\n";
    std::cout << "==================================================\n\n";

    SetColor(7);
    std::cout << " Current Volume ID : ";
    SetColor(14);
    std::cout << volume << "\n";

    SetColor(7);
    std::cout << " History size      : " << hist.size() << "\n\n";

    SetColor(7);
    std::cout << " [1] Spoof CID (Volume Serial)   : ";
    SetColor(spoofCID ? 10 : 12);
    std::cout << (spoofCID ? "[ON]" : "[OFF]") << "\n";

    SetColor(7);
    std::cout << " [2] Bypass Ads (IsByondMember)  : ";
    SetColor(noAds ? 10 : 12);
    std::cout << (noAds ? "[ON]" : "[OFF]") << "\n";

    SetColor(7);
    std::cout << " [3] Block Ad Links              : ";
    SetColor(blockLinks ? 10 : 12);
    std::cout << (blockLinks ? "[ON]" : "[OFF]") << "\n";

    SetColor(7);
    std::cout << " [4] Log Chat to Console         : ";
    SetColor(logChat ? 10 : 12);
    std::cout << (logChat ? "[ON]" : "[OFF]") << "\n";

    SetColor(7);
    std::cout << " [5] Log Commands to Console     : ";
    SetColor(logCommands ? 10 : 12);
    std::cout << (logCommands ? "[ON]" : "[OFF]") << "\n";

    SetColor(7);
    std::cout << " [6] Spoof OS Version            : ";
    SetColor(spoofOs ? 10 : 12);
    std::cout << (spoofOs ? "[ON]" : "[OFF]") << "\n\n";

    SetColor(11);
    std::cout << " [G] Generate new CID (skips history)\n";
    std::cout << " [C] Clear CID history\n";

    SetColor(14);
    std::cout << " [0] Close Menu (or ESC)\n";
    std::cout << "==================================================\n";

    if (!hist.empty())
    {
        SetColor(7);
        std::cout << "\n Recent CIDs: ";
        SetColor(14);

        int start = (static_cast<int>(hist.size()) > 5)
            ? static_cast<int>(hist.size()) - 5
            : 0;

        for (int i = start; i < static_cast<int>(hist.size()); ++i)
        {
            std::cout << hist[i];
            if (i + 1 < static_cast<int>(hist.size()))
                std::cout << ", ";
        }

        std::cout << "\n";
    }

    SetColor(11);
    std::cout << "\n Press key: ";
    SetColor(7);
}

void MenuLoop()
{
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    Sleep(3000);

    bool menuOpen = false;

    while (true)
    {
        if (GetAsyncKeyState(VK_INSERT) & 1)
        {
            menuOpen = !menuOpen;

            if (menuOpen)
            {
                HWND hConsole = GetConsoleWindow();
                if (hConsole)
                {
                    ShowWindow(hConsole, SW_SHOW);
                    SetForegroundWindow(hConsole);
                    SetConsoleTitleW(APOFEOS_TITLE_W);
                }

                DrawMenu();
            }
        }

        if (menuOpen)
        {
            if (_kbhit())
            {
                char key = _getch();

                if (key == '0' || key == 27)
                {
                    menuOpen = false;
                }
                else if (key >= '1' && key <= '6')
                {
                    if (key == '1')
                    {
                        int v = GetSetting("Hooks", "GetVolumeInformationA", 1);
                        SetSetting("Hooks", "GetVolumeInformationA", !v);
                    }
                    if (key == '2')
                    {
                        int v = GetSetting("Hooks", "IsByondMember", 1);
                        SetSetting("Hooks", "IsByondMember", !v);
                    }
                    if (key == '3')
                    {
                        int v = GetSetting("Hooks", "BrowseLink", 0);
                        SetSetting("Hooks", "BrowseLink", !v);
                    }
                    if (key == '4')
                    {
                        int v = GetSetting("Settings", "LogToHtml", 0);
                        SetSetting("Settings", "LogToHtml", !v);
                    }
                    if (key == '5')
                    {
                        int v = GetSetting("Hooks", "CommandEvent", 1);
                        SetSetting("Hooks", "CommandEvent", !v);
                    }
                    if (key == '6')
                    {
                        int v = GetSetting("Hooks", "GetVersionExW", 1);
                        SetSetting("Hooks", "GetVersionExW", !v);
                    }

                    DrawMenu();
                }
                else if (key == 'g' || key == 'G')
                {
                    unsigned long oldVolume = ReadVolumeFromConfig(g_IniPath.c_str());
                    AddHistoryId(oldVolume);

                    unsigned long newVolume = GenerateUniqueVolume();
                    WriteVolumeToConfig(g_IniPath.c_str(), newVolume);

                    DrawMenu();
                }
                else if (key == 'c' || key == 'C')
                {
                    ClearHistory();
                    DrawMenu();
                }
            }
        }

        Sleep(50);
    }
}

void StartMenuThread(const char* iniPath)
{
    if (iniPath && iniPath[0])
        g_IniPath = iniPath;

    std::thread(MenuLoop).detach();
}