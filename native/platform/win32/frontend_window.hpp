#pragma once
#include "../../web.hpp"
#include <memory>
#include <optional>

namespace web {
class DiagnosticLog;
// Owned modal canvas with fixed native/2x pixels and a focus-aware 60 Hz clock.
// Frontend controllers supply their image, input and inspection probes.
class FrontendWindow {
public:
    FrontendWindow(Bitmap canvas, std::wstring class_name, std::wstring title);
    virtual ~FrontendWindow();
    FrontendWindow(const FrontendWindow&) = delete;
    FrontendWindow& operator=(const FrontendWindow&) = delete;
    void run(void* owner);

protected:
    Bitmap& canvas();
    void* window() const;
    void refresh();
    void close();
    void caption(std::wstring);
    void fullscreen(bool);
    bool fullscreen() const;
    void gamepad(bool);
    void diagnostics(DiagnosticLog*);
    virtual bool capture_gamepad(std::uint64_t) {
        return false;
    }
    virtual void tick() = 0;
    virtual void key(unsigned) = 0;
    virtual void blur() {}
    virtual void activity(bool) {}
    virtual std::optional<std::intptr_t> probe(unsigned) const {
        return {};
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace web
