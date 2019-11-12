#pragma once
#include <string>
#include "Integrity.h"
#include <windows.h>

std::wstring ToUnicode(const std::string& str);

void LogErrAndThrow(const char* statement, const char* func, const char* file, int line, DWORD err_code);

#define CHECK(cond) do { if(!(cond)) \
    LogErrAndThrow(#cond, __func__, __FILE__, __LINE__, GetLastError()); \
    } while (0)

std::wstring GetCurrentExePath();
IntegrityLevel GetCurrentIntegrity();
IntegrityLevel IntegrityStringToEnum(std::string str);
std::string IntegrityEnumToString(IntegrityLevel level);
WELL_KNOWN_SID_TYPE IntegrityEnumToWellKnownSidType(IntegrityLevel level);

bool IsAdministrator();
void LogIntegrity();
