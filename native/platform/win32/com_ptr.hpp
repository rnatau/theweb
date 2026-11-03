#pragma once
#include <windows.h>
#include <objbase.h>
#include <utility>
#include <stdexcept>

namespace web::windows {
template <class T> class ComPtr {
public:
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) noexcept :
        value_(std::exchange(other.value_, nullptr)) {}
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset();
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }
    ~ComPtr() {
        reset();
    }
    T* get() const {
        return value_;
    }
    T* operator->() const {
        return value_;
    }
    explicit operator bool() const {
        return value_ != nullptr;
    }
    T** put() {
        reset();
        return &value_;
    }
    void reset() {
        if (value_)
            std::exchange(value_, nullptr)->Release();
    }

private:
    T* value_ = nullptr;
};
class ComApartment {
public:
    ComApartment() {
        const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(result))
            throw std::runtime_error("Cannot initialize audio COM apartment");
    }
    ~ComApartment() {
        CoUninitialize();
    }
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
};
} // namespace web::windows
