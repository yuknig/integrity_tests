#include "Integrity.h"
#include "ProcessUtils.h"
#include <atlsecurity.h>

IntegrityScope::IntegrityScope(IntegrityLevel level) {
    if (level != IntegrityLevel::Low &&
        level != IntegrityLevel::Medium &&
        level != IntegrityLevel::High)
        throw std::invalid_argument(std::string("wrong integrity level") + IntegrityEnumToString(level));

    Handle cur_token;
    CHECK(OpenProcessToken(GetCurrentProcess(),
                           TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY,
                           &cur_token));
    CHECK(DuplicateTokenEx(static_cast<HANDLE>(cur_token), 0, 0, SecurityImpersonation, TokenPrimary, &m_token));
    ApplyIntegrity(level);
    CHECK(ImpersonateLoggedOnUser(static_cast<HANDLE>(m_token)));
}

IntegrityScope::~IntegrityScope() {
    RevertToSelf();
}

HANDLE IntegrityScope::GetToken() const {
    return static_cast<HANDLE>(m_token);
}

void IntegrityScope::ApplyIntegrity(IntegrityLevel integrity) {
    ATL::CHeapPtr<SID, CLocalAllocator> sid;
    {
        DWORD sid_size = SECURITY_MAX_SID_SIZE;
        CHECK(sid.AllocateBytes(sid_size));
        WELL_KNOWN_SID_TYPE sid_type = IntegrityEnumToWellKnownSidType(integrity);
        CHECK(CreateWellKnownSid(sid_type, nullptr, sid, &sid_size));
    }

    TOKEN_MANDATORY_LABEL tml = {};
    tml.Label.Attributes = SE_GROUP_INTEGRITY;
    tml.Label.Sid = sid;
    CHECK(SetTokenInformation(static_cast<HANDLE>(m_token), TokenIntegrityLevel, &tml, sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(&*sid)));
}
