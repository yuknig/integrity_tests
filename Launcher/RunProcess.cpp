#include "Process.h"
#include "ProcessUtils.h"
#include <atlsecurity.h>
#include <array>

ATL::CSid GetLogonSid(const HANDLE& token) {
    DWORD dw_length = 0;

    // Get required buffer size and allocate the TOKEN_GROUPS buffer.
    if (!GetTokenInformation(token,       // handle to the access token
                             TokenGroups, // get information about the token's groups 
                             nullptr,     // pointer to TOKEN_GROUPS buffer
                             0,           // size of buffer
                             &dw_length)) // receives required buffer size
    {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            throw std::runtime_error("GetTokenInformation() failed");
    }

    ATL::CHeapPtr<TOKEN_GROUPS, CLocalAllocator> token_groups;
    CHECK(token_groups.AllocateBytes(dw_length));

    // Get the token group information from the access token.
    CHECK(GetTokenInformation(token,        // handle to the access token
                              TokenGroups,  // get information about the token's groups 
                              token_groups, // pointer to TOKEN_GROUPS buffer
                              dw_length,    // size of buffer
                              &dw_length)); // receives required buffer size

    // Loop through the groups to find the logon SID
    for (DWORD tok_num = 0; tok_num < token_groups->GroupCount; ++tok_num) {
        const auto& group = token_groups->Groups[tok_num];
        if ((group.Attributes & SE_GROUP_LOGON_ID) != SE_GROUP_LOGON_ID)
            continue;

        // Found the logon SID; make a copy of it
        auto sid_length = GetLengthSid(group.Sid);
        ATL::CHeapPtr<SID, CLocalAllocator> sid;
        CHECK(sid.AllocateBytes(sid_length));

        CHECK(CopySid(sid_length, sid, group.Sid));
        return ATL::CSid(*sid);
    }
    throw std::runtime_error("Logon Sid not found");
}

void AddAceToWindowStation(const Winsta& winsta, const ATL::CSid& sid) {
    //TODO: fix issue with 'old' UI style

    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;

    ATL::CHeapPtr<char, CLocalAllocator> sd, sd_new;

    // get size needed to hold DACL
    DWORD buf_size = 0;
    if (!GetUserObjectSecurity(static_cast<HWINSTA>(winsta), &si, &sd, buf_size, &buf_size) || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        CHECK(sd.AllocateBytes(buf_size));
        CHECK(sd_new.AllocateBytes(buf_size));
    }

    // Obtain the DACL for the window station
    CHECK(GetUserObjectSecurity(static_cast<HWINSTA>(winsta), &si, sd, buf_size, &buf_size));

    // Create a new DACL
    CHECK(InitializeSecurityDescriptor(sd_new, SECURITY_DESCRIPTOR_REVISION));

    BOOL dacl_present = FALSE;
    BOOL dacl_exists  = FALSE;
    PACL acl = nullptr;
    // Get the DACL from the security descriptor
    CHECK(GetSecurityDescriptorDacl(sd, &dacl_present, &acl, &dacl_exists));

    // Initialize the ACL
    ACL_SIZE_INFORMATION acl_size_info = {};
    acl_size_info.AclBytesInUse = sizeof(ACL);
    if (acl)
        CHECK(GetAclInformation(acl, &acl_size_info, sizeof(ACL_SIZE_INFORMATION), AclSizeInformation));

    // Compute the size of the new ACL
    DWORD new_acl_size = acl_size_info.AclBytesInUse +
        (2 * sizeof(ACCESS_ALLOWED_ACE)) + (2 * sid.GetLength()) -
        (2 * sizeof(DWORD));
    ATL::CHeapPtr<ACL, CLocalAllocator> new_acl;
    CHECK(new_acl.AllocateBytes(new_acl_size));
    memset(new_acl.m_pData, 0, new_acl_size);

    // Initialize the new DACL
    CHECK(InitializeAcl(new_acl, new_acl_size, ACL_REVISION));

    // If DACL is present, copy it to a new DACL
    if (dacl_present) {
        // Copy the ACEs to the new ACL
        const DWORD ace_count = acl_size_info.AceCount;
        for (DWORD ace_num = 0; ace_num < ace_count; ++ace_num) {

            LPVOID temp_ace;
            // Get an ACE
            CHECK(GetAce(acl, ace_num, &temp_ace));
            // Add the ACE to the new ACL
            CHECK(AddAce(new_acl, ACL_REVISION, MAXDWORD, temp_ace, ((PACE_HEADER)temp_ace)->AceSize));
        }
    }

    // Add the first ACE to the window station
    ATL::CHeapPtr<ACCESS_ALLOWED_ACE, CLocalAllocator> ace;
    DWORD ace_size = sizeof(ACCESS_ALLOWED_ACE) + sid.GetLength() - sizeof(DWORD);
    CHECK(ace.AllocateBytes(ace_size));
    memset(ace.m_pData, 0, ace_size);

    ace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
    ace->Header.AceFlags = CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE;
    ace->Header.AceSize = static_cast<WORD>(ace_size);
    ace->Mask = GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL;

    CHECK(CopySid(sid.GetLength(), &ace->SidStart, const_cast<SID*>(sid.GetPSID())));
    CHECK(AddAce(new_acl, ACL_REVISION, MAXDWORD, ace, ace->Header.AceSize));

    // Add the second ACE to the window station.
    const ACCESS_MASK WINSTA_ALL = (WINSTA_ENUMDESKTOPS | WINSTA_READATTRIBUTES | \
        WINSTA_ACCESSCLIPBOARD | WINSTA_CREATEDESKTOP | \
        WINSTA_WRITEATTRIBUTES | WINSTA_ACCESSGLOBALATOMS | \
        WINSTA_EXITWINDOWS | WINSTA_ENUMERATE | WINSTA_READSCREEN | \
        STANDARD_RIGHTS_REQUIRED);

    ace->Header.AceFlags = NO_PROPAGATE_INHERIT_ACE;
    ace->Mask = WINSTA_ALL;

    CHECK(AddAce(new_acl, ACL_REVISION, MAXDWORD, ace, ace->Header.AceSize));
    // Set a new DACL for the security descriptor.
    CHECK(SetSecurityDescriptorDacl(sd_new, TRUE, new_acl, FALSE));
    CHECK(SetUserObjectSecurity(static_cast<HWINSTA>(winsta), &si, sd_new));
}

void ShellExecAsUser(const HANDLE& token,
                     const wchar_t* user,
                     const wchar_t* dir,
                     const wchar_t* app_name,
                     const wchar_t* app_params) {

    Winsta winsta_save = GetProcessWindowStation();
    if (!winsta_save)
        throw std::runtime_error("GetProcessWindowStation() failed");

    Winsta winsta = OpenWindowStationW(L"winsta0",                  // the interactive window station 
                                       FALSE,                       // handle is not inheritable
                                       READ_CONTROL | WRITE_DAC);   // rights to read/write the DACL
    if (!winsta) 
        throw std::runtime_error("OpenWindowStationW() failed");

    CHECK(SetProcessWindowStation(static_cast<HWINSTA>(winsta)));

    Desk desk = OpenDesktop(L"default",    // the interactive window station 
                            0,             // no interaction with other desktop processes
                            FALSE,         // handle is not inheritable
                            READ_CONTROL | // request the rights to read and write the DACL
                            WRITE_DAC |
                            DESKTOP_WRITEOBJECTS |
                            DESKTOP_READOBJECTS);

    CHECK(SetProcessWindowStation(static_cast<HWINSTA>(winsta_save)));

    if (!desk)
        throw std::runtime_error("desk is null");

    CSid sid = GetLogonSid(token);

    AddAceToWindowStation(winsta, sid);

    // Impersonate client to ensure access to executable file
    CHECK(ImpersonateLoggedOnUser(token));

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = L"runas";
    sei.lpFile = app_name;
    sei.lpParameters = app_params;
    sei.lpDirectory = dir;
    sei.nShow = SW_NORMAL;
    sei.hInstApp = NULL;
    CHECK(ShellExecuteExW(&sei));
}

void CreateProcAs(const HANDLE& token,
                  const wchar_t* cmd) {

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    EnvironmentBlock env;
    CHECK(CreateEnvironmentBlock(&env, token, FALSE));

    CHECK(CreateProcessAsUserW(static_cast<HANDLE>(token), nullptr, const_cast<wchar_t*>(cmd), nullptr, nullptr,
                               FALSE, CREATE_UNICODE_ENVIRONMENT, static_cast<HANDLE>(env), nullptr, &si, &pi));

    WaitForInputIdle(pi.hProcess, 1000);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
}
