#pragma once
#include "HandleWrap.h"

enum class IntegrityLevel {
    Other = 0,
    Low,
    Medium,
    High
};

class IntegrityScope {
public:
    IntegrityScope(IntegrityLevel level);
    ~IntegrityScope();
    HANDLE GetToken() const;

private:
    void ApplyIntegrity(IntegrityLevel level);

private:
    Handle m_token;
};

