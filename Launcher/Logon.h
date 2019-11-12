#pragma once
#include "SecureArray.h"
#include "HandleWrap.h"
#include <memory>
#include <wincred.h>

struct LogonRes {
    SecureArray<wchar_t, CREDUI_MAX_USERNAME_LENGTH + 1> m_user;
    SecureArray<wchar_t, CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1> m_domain;
    SecureArray<wchar_t, CREDUI_MAX_PASSWORD_LENGTH + 1> m_password;
    Handle m_logon_token;

    LogonRes() = default;

    LogonRes(LogonRes&& rhs);
    LogonRes& operator=(LogonRes&& rhs);

    LogonRes(LogonRes&) = delete;
    LogonRes& operator=(LogonRes&) = delete;
};

std::unique_ptr<LogonRes> LogonInteractively();
