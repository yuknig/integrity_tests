#pragma once
#include <array>
#include <windows.h>

template <typename T, size_t Size>
class SecureArray final {
public:
    SecureArray() {
        zero_memory();
    }

    ~SecureArray() {
        zero_memory();
    }

    void zero_memory() {
        SecureZeroMemory(data(), size() * sizeof(T));
    }

    T* data() {
        return m_data.data();
    }

    size_t size() const {
        return m_data.size();
    }

private:
    std::array<T, Size> m_data;
};
