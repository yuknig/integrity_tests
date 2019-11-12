#include "Logon.h"
#include <iostream>

LogonRes::LogonRes(LogonRes&& rhs) {
    this->operator=(std::move(rhs));
}

LogonRes& LogonRes::operator=(LogonRes&& rhs) {
    m_user = rhs.m_user;
    rhs.m_user.zero_memory();

    m_domain = rhs.m_domain;
    rhs.m_domain.zero_memory();

    m_password = rhs.m_password;
    rhs.m_password.zero_memory();

    m_logon_token = std::move(rhs.m_logon_token);
    return *this;
}


std::unique_ptr<LogonRes> LogonInteractively() {
    std::cout << "Please enter admin user credentials...\n";

    const size_t logon_attempts = 5;
    for (size_t attempt = 0; attempt < logon_attempts; ++attempt) {
        // enter user credentails
        LogonRes res = {};
        SecureArray<wchar_t, CREDUI_MAX_USERNAME_LENGTH + 1> user_domain;

        const wchar_t* target = L"this console app";
        DWORD dw_res = CredUICmdLinePromptForCredentialsW(target,
                                                          NULL,
                                                          0,
                                                          user_domain.data(), static_cast<DWORD>(user_domain.size()),
                                                          res.m_password.data(), static_cast<DWORD>(res.m_password.size()),
                                                          NULL,
                                                          CREDUI_FLAGS_GENERIC_CREDENTIALS |
                                                          CREDUI_FLAGS_EXCLUDE_CERTIFICATES |
                                                          CREDUI_FLAGS_DO_NOT_PERSIST |
                                                          CREDUI_FLAGS_EXPECT_CONFIRMATION);
        if (NO_ERROR != dw_res) {
            std::cout << "credentials are not valid\n";
            break;
        }

        // split DOMAIN/user string to User and Password
        dw_res = CredUIParseUserNameW(user_domain.data(), res.m_user.data(), static_cast<ULONG>(res.m_user.size()), res.m_domain.data(), static_cast<ULONG>(res.m_domain.size()));
        if (NOERROR != dw_res) {
            std::cout << "failed to parse domain\\user account name\n";
            continue;
        }
        user_domain.zero_memory();

        BOOL logged_on = LogonUserW(res.m_user.data(), res.m_domain.data(), res.m_password.data(),
            /*LOGON32_LOGON_BATCH*/ LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &res.m_logon_token);

        CredUIConfirmCredentialsW(target, logged_on); // returns ERROR_NOT_FOUND
        if (logged_on) {
            std::cout << "Logged on successfully\n";
            return std::make_unique<LogonRes>(std::move(res));
        }
        else
            std::cout << "Failed to logon\n";
    }
    return {};
}
