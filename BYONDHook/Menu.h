#pragma once
#include <windows.h>

#define APOFEOS_TITLE_W L"\u0410\u043f\u043e\u0444\u0435\u043e\u0437"
#define APOFEOS_NAME_U8 reinterpret_cast<const char*>(u8"\u0410\u043f\u043e\u0444\u0435\u043e\u0437")
#define APOFEOS_UPPER_U8 reinterpret_cast<const char*>(u8"\u0410\u041f\u041e\u0424\u0415\u041e\u0417")

void StartMenuThread(const char* iniPath);

unsigned long ReadVolumeFromConfig(const char* iniPath);
void WriteVolumeToConfig(const char* iniPath, unsigned long volume);