#include "ProcessUtils.h"
#include "HandleWrap.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <atlmem.h>
#include <windows.h>

std::wstring ToUnicode(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), const_cast<wchar_t*>(wstr.data()), size_needed);
    return wstr;
}

std::string GetLastErrorStr() {
    const DWORD dw = GetLastError();
    ATL::CHeapPtr<char, CLocalAllocator> buf;

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   dw,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPSTR)&buf.m_pData,
                   0, NULL);

    std::string msg(buf.m_pData ? buf.m_pData : "unknown");
    return msg;
}

void LogErrAndThrow(const char* statement, const char* func, const char* file, int line, DWORD err_code) {
    std::ostringstream oss;
    oss << "ERROR: check '" << statement << "'";
    oss << " failed in function " << func << " at " << file << "@" << std::to_string(line);
    oss << " with error " << (err_code) << ": '" << GetLastErrorStr() << "'";
    std::cout << oss.str() << std::endl;
    throw std::runtime_error(oss.str());
}

std::wstring GetCurrentExePath() {
    std::array<wchar_t, MAX_PATH> path_array = {};
    CHECK(GetModuleFileNameW(NULL, path_array.data(), static_cast<DWORD>(path_array.size())));
    return path_array.data();
}

DWORD GetCurrentIntegrityRid() {
    // Known integrity RIDS
    // SECURITY_MANDATORY_UNTRUSTED_RID              0x00000000 Untrusted.
    // SECURITY_MANDATORY_LOW_RID                    0x00001000 Low integrity.
    // SECURITY_MANDATORY_MEDIUM_RID                 0x00002000 Medium integrity.
    // SECURITY_MANDATORY_MEDIUM_PLUS_RID            SECURITY_MANDATORY_MEDIUM_RID + 0x100 Medium high integrity.
    // SECURITY_MANDATORY_HIGH_RID                   0X00003000 High integrity.
    // SECURITY_MANDATORY_SYSTEM_RID                 0x00004000 System integrity.
    // SECURITY_MANDATORY_PROTECTED_PROCESS_RID      0x00005000 Protected process.

    Handle token;
    CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token));

    DWORD size_intgty_lvl = 0;
    CHECK(!GetTokenInformation(static_cast<HANDLE>(token), TokenIntegrityLevel, NULL, size_intgty_lvl, &size_intgty_lvl) &&
           GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    ATL::CHeapPtr<TOKEN_GROUPS, CLocalAllocator> intgty_lvl;
    CHECK(intgty_lvl.AllocateBytes(size_intgty_lvl));
    auto* tml = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(&*intgty_lvl);

    DWORD return_length = 0;
    CHECK(GetTokenInformation(static_cast<HANDLE>(token), TokenIntegrityLevel, tml, size_intgty_lvl, &return_length));
    DWORD result = *GetSidSubAuthority(tml->Label.Sid,
                    (DWORD)(UCHAR)(*GetSidSubAuthorityCount(tml->Label.Sid) - 1));
    return result;
}

IntegrityLevel IntegrityRidToIntegrityEnum(DWORD rid) {
    switch (rid) {
        case SECURITY_MANDATORY_UNTRUSTED_RID:
        case SECURITY_MANDATORY_LOW_RID:
            return IntegrityLevel::Low;

        case SECURITY_MANDATORY_MEDIUM_RID:
        case SECURITY_MANDATORY_MEDIUM_PLUS_RID:
            return IntegrityLevel::Medium;

        case SECURITY_MANDATORY_HIGH_RID:
            return IntegrityLevel::High;

        default:
            return IntegrityLevel::Other;
    }
}

IntegrityLevel GetCurrentIntegrity() {
    return IntegrityRidToIntegrityEnum(GetCurrentIntegrityRid());
}

std::string IntegrityEnumToString(IntegrityLevel level) {
    switch (level) {
        case IntegrityLevel::High:
            return "high";
        case IntegrityLevel::Medium:
            return "medium";
        case IntegrityLevel::Low:
            return "low";
        case IntegrityLevel::Other:
            return "other";
        default:
            return std::string("unknown(") + std::to_string(static_cast<int>(level)) + ")";
    }
}

IntegrityLevel IntegrityStringToEnum(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), tolower);

    if (str == "high")
        return IntegrityLevel::High;
    if (str == "medium")
        return IntegrityLevel::Medium;
    if (str == "low")
        return IntegrityLevel::Low;
    return IntegrityLevel::Other;
}

WELL_KNOWN_SID_TYPE IntegrityEnumToWellKnownSidType(IntegrityLevel integrity) {
    switch (integrity) {
        case IntegrityLevel::Low:
            return WinLowLabelSid;
        case IntegrityLevel::Medium:
            return WinMediumLabelSid;
        case IntegrityLevel::High:
            return WinHighLabelSid;
        default:
            throw std::invalid_argument(std::string("unsupported integrity: ") + IntegrityEnumToString(integrity));
    }
}

std::string IntegrityRidToString(DWORD rid) {
    switch (rid) {
        case SECURITY_MANDATORY_UNTRUSTED_RID:
            return "untrusted";
        case SECURITY_MANDATORY_LOW_RID:
            return "low";
        case SECURITY_MANDATORY_MEDIUM_RID:
            return "medium";
        case SECURITY_MANDATORY_MEDIUM_PLUS_RID:
            return "medium plus";
        case SECURITY_MANDATORY_HIGH_RID:
            return "high";
        case SECURITY_MANDATORY_SYSTEM_RID:
            return "system";
        case SECURITY_MANDATORY_PROTECTED_PROCESS_RID:
            return "protected process";
        default:
            return "unknown";
    }
}

bool IsAdministrator(HANDLE token) {
    TOKEN_ELEVATION elevation{};
    DWORD cb_size = sizeof(elevation);
    CHECK(GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &cb_size));

    return (0 != elevation.TokenIsElevated);
}

bool IsAdministrator() {
    Handle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;

    return IsAdministrator(static_cast<HANDLE>(token));
}

void LogIntegrity() {
    const bool is_admin = IsAdministrator();
    const IntegrityLevel integrity = GetCurrentIntegrity();
    std::cout << "Is administrator: " << (is_admin ? "yes" : "no") << std::endl;
    std::cout << "Integrity: " << IntegrityRidToString(GetCurrentIntegrityRid()) << std::endl;
}
