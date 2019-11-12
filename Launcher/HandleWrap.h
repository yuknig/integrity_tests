#pragma once
#include <cassert>
#include <Windows.h>
#include <UserEnv.h>

template <typename HandleT, BOOL(__stdcall * DeleterFuncT)(HandleT)>
class HandleWrap final {
    using ThisType = HandleWrap<HandleT, DeleterFuncT>;
public:
    HandleWrap() = default;

    HandleWrap(HandleT h)
        : m_h(h)
    {
        assert(m_h);
    }

    HandleWrap(const ThisType&) = delete;
    ThisType& operator=(const ThisType&) = delete;

    HandleWrap(ThisType&& rhs) {
        this->operator=(std::move(rhs));
    }

    ~HandleWrap() {
        DeleteHandle();
    }

    ThisType& operator=(ThisType&& rhs) {
        DeleteHandle();
        m_h = rhs.m_h;
        rhs.m_h = NULL;
        return *this;
    }

    explicit operator HandleT() const {
        return m_h;
    }

    operator bool() const {
        return (m_h != NULL);
    }

    HandleT* operator&() {
        return &m_h;
    }

private:
    void DeleteHandle() {
        if (m_h) {
            DeleterFuncT(m_h);
            m_h = NULL;
        }
    }

private:
    HandleT m_h {NULL};
};

using Handle  = HandleWrap<HANDLE,  CloseHandle>; // use CHandle from atlbase.h?
using Winsta  = HandleWrap<HWINSTA, CloseWindowStation>;
using Desk    = HandleWrap<HDESK,   CloseDesktop>;
using EnvironmentBlock = HandleWrap<HANDLE, DestroyEnvironmentBlock>;
