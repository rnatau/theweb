#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "swap_presenter.hpp"
#include "com_ptr.hpp"
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>
#include <cstring>
#include <sstream>
#include <chrono>

namespace web {
namespace {
void check(HRESULT result) {
    if (FAILED(result)) {
        std::ostringstream text;
        text << "DirectX presentation failed (0x" << std::hex << unsigned(result) << ')';
        throw std::runtime_error(text.str());
    }
}
// Compile once using the Windows system component. A missing compiler follows
// the same visible GDI fallback as a missing graphics device, without adding a
// loader dependency or a redistributable to either shipping executable.
windows::ComPtr<ID3DBlob> compile(const char* source, const char* entry, const char* target) {
    struct Compiler {
        HMODULE module = LoadLibraryExW(L"d3dcompiler_47.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        ~Compiler() {
            if (module)
                FreeLibrary(module);
        }
    };
    static Compiler compiler; // Returned blobs retain this module's COM vtables.
    const auto function = compiler.module ? reinterpret_cast<pD3DCompile>(GetProcAddress(compiler.module, "D3DCompile")) : nullptr;
    if (!function)
        throw std::runtime_error("Windows DirectX shader compiler is unavailable");
    windows::ComPtr<ID3DBlob> shader, errors;
    const auto result = function(source, std::strlen(source), "table", nullptr, nullptr, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, shader.put(), errors.put());
    if (FAILED(result) && errors)
        throw std::runtime_error(std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()));
    check(result);
    return shader;
}
constexpr char table_shader[] = R"(
cbuffer Parameters : register(b0) {
    int4 view;
    int4 native; // width, height, exact sampling, crisp DMD
    int4 layout; // native DMD x, y, pitch, dot width
    int4 dots;   // native dot height, output dot width, height, pitch
    int4 position; // output DMD x, y, nudge x, y
};
Texture2D<float4> image : register(t0);
Texture2D<float4> original : register(t1);
SamplerState filtering : register(s0);
float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 p = float2((id << 1) & 2, id & 2);
    return float4(p * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 pixel(float4 screen : SV_Position) : SV_Target {
    int2 p = int2(screen.xy);
    if (any(p < view.xy) || any(p >= view.xy + view.zw))
        return float4(0, 0, 0, 1);
    p = view.xy + clamp(p - view.xy - position.zw, int2(0, 0), view.zw - 1);
    int2 d = p - position.xy;
    if (native.w && all(d >= 0) && all(d < int2(128, 32) * dots.w)) {
        int2 within = d % dots.w;
        if (all(within < dots.yz)) {
            int2 source = layout.xy + d / dots.w * layout.z + within * int2(layout.w, dots.x) / dots.yz;
            return float4(original.Load(int3(source, 0)).rgb, 1);
        }
    }
    float2 uv = (float2(p - view.xy) + 0.5) / view.zw;
    return float4(image.SampleLevel(filtering, uv, 0).rgb, 1);
}
)";
class DirectxPresenter final : public SwapPresenter {
    windows::ComPtr<ID3D11Device> device_;
    windows::ComPtr<ID3D11DeviceContext> context_;
    windows::ComPtr<IDXGISwapChain2> swap_;
    windows::ComPtr<ID3D11Texture2D> upload_, last_buffer_;
    HWND window_ = nullptr;
    HANDLE ready_ = nullptr;
    int width_ = 0, height_ = 0;
    bool capture_frames_ = false;
    bool frame_acquired_ = false;
    std::chrono::steady_clock::time_point readiness_wait_{};
    Bitmap captured_;
    struct Texture {
        windows::ComPtr<ID3D11Texture2D> image;
        windows::ComPtr<ID3D11ShaderResourceView> view;
        int width = 0, height = 0;
        void reset() {
            view.reset();
            image.reset();
            width = height = 0;
        }
    } native_, artwork_, overlay_;
    windows::ComPtr<ID3D11VertexShader> vertex_;
    windows::ComPtr<ID3D11PixelShader> pixel_;
    windows::ComPtr<ID3D11Buffer> parameters_;
    windows::ComPtr<ID3D11SamplerState> linear_, point_;
    struct Parameters {
        std::array<int, 4> view{}, native{}, layout{}, dots{}, position{};
    };
    void pipeline() {
        if (vertex_)
            return;
        const auto vs = compile(table_shader, "vertex", "vs_4_0");
        const auto ps = compile(table_shader, "pixel", "ps_4_0");
        check(device_->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, vertex_.put()));
        check(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, pixel_.put()));
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(Parameters);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        check(device_->CreateBuffer(&desc, nullptr, parameters_.put()));
        D3D11_SAMPLER_DESC sampler{};
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        check(device_->CreateSamplerState(&sampler, linear_.put()));
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        check(device_->CreateSamplerState(&sampler, point_.put()));
    }
    void texture(Texture& texture, int width, int height, bool dynamic) {
        if (texture.width != width || texture.height != height) {
            texture.reset();
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = UINT(width);
            desc.Height = UINT(height);
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
            check(device_->CreateTexture2D(&desc, nullptr, texture.image.put()));
            check(device_->CreateShaderResourceView(texture.image.get(), nullptr, texture.view.put()));
            texture.width = width;
            texture.height = height;
        }
    }
    void upload(Texture& texture, int width, int height, std::span<const std::uint32_t> pixels) {
        if (width <= 0 || height <= 0 || pixels.size() != std::size_t(width) * height)
            throw std::invalid_argument("Invalid native DirectX texture");
        this->texture(texture, width, height, true);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context_->Map(texture.image.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        for (int row = 0; row < height; ++row)
            std::memcpy(static_cast<std::uint8_t*>(mapped.pData) + std::size_t(row) * mapped.RowPitch, pixels.data() + std::size_t(row) * width, std::size_t(width) * sizeof(std::uint32_t));
        context_->Unmap(texture.image.get(), 0);
    }
    void draw(const Parameters& values, const Texture& image, const Texture& backdrop, PresentationRect viewport) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context_->Map(parameters_.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        std::memcpy(mapped.pData, &values, sizeof(values));
        context_->Unmap(parameters_.get(), 0);
        auto* buffer = parameters_.get();
        context_->PSSetConstantBuffers(0, 1, &buffer);
        ID3D11ShaderResourceView* resources[]{image.view.get(), backdrop.view.get()};
        context_->PSSetShaderResources(0, 2, resources);
        auto* sampler = values.native[2] ? point_.get() : linear_.get();
        context_->PSSetSamplers(0, 1, &sampler);
        const D3D11_VIEWPORT area{float(viewport.x), float(viewport.y), float(viewport.width), float(viewport.height), 0, 1};
        context_->RSSetViewports(1, &area);
        context_->Draw(3, 0);
        ID3D11ShaderResourceView* empty[2]{};
        context_->PSSetShaderResources(0, 2, empty);
    }
    void initialize(HWND window, int width, int height) {
        reset();
        constexpr D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        auto result =
            D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, UINT(std::size(levels)), D3D11_SDK_VERSION, device_.put(), nullptr, context_.put());
        if (FAILED(result))
            check(D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                levels,
                UINT(std::size(levels)),
                D3D11_SDK_VERSION,
                device_.put(),
                nullptr,
                context_.put()));
        windows::ComPtr<IDXGIDevice> dxgi;
        windows::ComPtr<IDXGIAdapter> adapter;
        windows::ComPtr<IDXGIFactory2> factory;
        check(device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgi.put())));
        check(dxgi->GetAdapter(adapter.put()));
        check(adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(factory.put())));
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = UINT(width);
        desc.Height = UINT(height);
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        windows::ComPtr<IDXGISwapChain1> chain;
        check(factory->CreateSwapChainForHwnd(device_.get(), window, &desc, nullptr, nullptr, chain.put()));
        check(chain->QueryInterface(__uuidof(IDXGISwapChain2), reinterpret_cast<void**>(swap_.put())));
        check(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
        check(swap_->SetMaximumFrameLatency(1));

        window_ = window;
        resize(width, height);
    }
    void resize(int width, int height) {
        frame_acquired_ = false;
        readiness_wait_ = {};
        last_buffer_.reset();
        upload_.reset();
        context_->ClearState();
        context_->Flush();
        if (width_ || height_)
            check(swap_->ResizeBuffers(2, UINT(width), UINT(height), DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
        if (ready_)
            CloseHandle(std::exchange(ready_, nullptr));
        ready_ = swap_->GetFrameLatencyWaitableObject();
        if (!ready_)
            throw std::runtime_error("DirectX frame readiness is unavailable");
        width_ = width;
        height_ = height;
    }

public:
    ~DirectxPresenter() override {
        reset();
    }
    void reset() override {
        last_buffer_.reset();
        upload_.reset();
        native_.reset();
        artwork_.reset();
        overlay_.reset();
        vertex_.reset();
        pixel_.reset();
        parameters_.reset();
        linear_.reset();
        point_.reset();
        swap_.reset();
        if (context_) {
            context_->ClearState();
            context_->Flush();
        }
        context_.reset();
        device_.reset();
        if (ready_)
            CloseHandle(std::exchange(ready_, nullptr));
        window_ = nullptr;
        frame_acquired_ = false;
        readiness_wait_ = {};
        width_ = height_ = 0;
    }
    void present(HWND window, int width, int height, std::span<const std::uint32_t> pixels, bool synchronize) override {
        if (width <= 0 || height <= 0 || pixels.size() != std::size_t(width) * height)
            throw std::invalid_argument("Invalid DirectX presentation dimensions");
        submit(window, width, height, synchronize, [&] {
            if (!upload_) {
                D3D11_TEXTURE2D_DESC desc{};
                desc.Width = UINT(width);
                desc.Height = UINT(height);
                desc.MipLevels = desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                check(device_->CreateTexture2D(&desc, nullptr, upload_.put()));
            }
            context_->UpdateSubresource(upload_.get(), 0, nullptr, pixels.data(), UINT(width * sizeof(std::uint32_t)), 0);
            context_->CopyResource(last_buffer_.get(), upload_.get());
        });
    }
    bool present_table(HWND window, int width, int height, const PresentationFrame& frame, std::span<const PresentationLayer> layers, bool synchronize) override {
        submit(window, width, height, synchronize, [&] {
            pipeline();
            const auto& image = frame.screen;
            upload(native_, image.width, image.height, image.pixels);
            if (frame.dots) {
                if (!frame.display_backdrop || frame.display_backdrop->width != frame.display_layout.width() || frame.display_backdrop->height != frame.display_layout.height())
                    throw std::invalid_argument("Invalid DirectX display backdrop");
                const auto& backdrop = *frame.display_backdrop;
                texture(artwork_, image.width, image.height, false);
                context_->CopyResource(artwork_.image.get(), native_.image.get());
                const auto layout = frame.display_layout;
                const D3D11_BOX box{UINT(layout.x), UINT(layout.y), 0, UINT(layout.x + backdrop.width), UINT(layout.y + backdrop.height), 1};
                context_->UpdateSubresource(artwork_.image.get(), 0, &box, backdrop.pixels.data(), UINT(backdrop.width * sizeof(std::uint32_t)), 0);
            }
            windows::ComPtr<ID3D11RenderTargetView> target;
            check(device_->CreateRenderTargetView(last_buffer_.get(), nullptr, target.put()));
            auto* render_target = target.get();
            context_->OMSetRenderTargets(1, &render_target, nullptr);
            context_->IASetInputLayout(nullptr);
            context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context_->VSSetShader(vertex_.get(), nullptr, 0);
            context_->PSSetShader(pixel_.get(), nullptr, 0);
            Parameters values;
            const auto view = frame.viewport;
            values.view = {view.x, view.y, view.width, view.height};
            values.native = {image.width, image.height, integer_presentation(view, image.width, image.height), bool(frame.dots)};
            values.position[2] = MulDiv(frame.nudge.x, view.width, image.width);
            values.position[3] = MulDiv(frame.nudge.y, view.height, image.height);
            if (frame.dots) {
                const auto dots = *frame.dots;
                const auto layout = frame.display_layout;
                values.layout = {layout.x, layout.y, layout.spacing, layout.dot_width};
                values.dots = {layout.dot_height, dots.dot_width, dots.dot_height, dots.pitch};
                values.position[0] = dots.x;
                values.position[1] = dots.y;
            }
            draw(values, frame.dots ? artwork_ : native_, native_, {0, 0, width, height});
            for (const auto& layer : layers) {
                const auto area = layer.bounds;
                upload(overlay_, area.width, area.height, layer.pixels);
                Parameters overlay;
                overlay.view = {area.x, area.y, area.width, area.height};
                overlay.native = {area.width, area.height, 1, 0};
                draw(overlay, overlay_, native_, area);
            }
            context_->OMSetRenderTargets(0, nullptr, nullptr);
        });
        return true;
    }
    HANDLE frame_ready_handle() const override {
        return ready_;
    }
    void frame_ready() override {
        frame_acquired_ = true;
        readiness_wait_ = {};
    }

private:
    template <class Draw> void submit(HWND window, int width, int height, bool synchronize, Draw draw) {
        for (unsigned attempt = 0;; ++attempt) {
            try {
                if (!swap_ || window != window_)
                    initialize(window, width, height);
                else if (width != width_ || height != height_)
                    resize(width, height);
                // Occluded/hidden diagnostic windows must not wait for scanout.
                const auto visibility = swap_->Present(0, DXGI_PRESENT_TEST);
                check(visibility);
                if (synchronize && !frame_acquired_ && visibility != DXGI_STATUS_OCCLUDED && IsWindowVisible(window)) {
                    const auto result = WaitForSingleObject(ready_, 100);
                    if (result == WAIT_FAILED)
                        throw std::runtime_error("Cannot wait for DirectX frame readiness");
                }
                frame_acquired_ = false;
                last_buffer_.reset();
                check(swap_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(last_buffer_.put())));
                draw();
                if (capture_frames_)
                    captured_ = readback();
                check(swap_->Present(synchronize ? 1 : 0, 0));
                readiness_wait_ = {};
                return;
            } catch (...) {
                reset();
                if (attempt)
                    throw; // One device recreation, then the host falls back.
            }
        }
    }

public:
    void capture_frames(bool value) override {
        capture_frames_ = value;
        captured_ = {};
    }
    bool acquire_frame() override {
        if (!ready_ || frame_acquired_)
            return true;
        const auto result = WaitForSingleObject(ready_, 0);
        if (result == WAIT_FAILED)
            throw std::runtime_error("Cannot acquire DirectX frame readiness");
        if (result != WAIT_OBJECT_0) {
            const auto now = std::chrono::steady_clock::now();
            if (readiness_wait_ == std::chrono::steady_clock::time_point{})
                readiness_wait_ = now;
            else if (now - readiness_wait_ > std::chrono::milliseconds(100))
                throw std::runtime_error("DirectX frame readiness exceeded 100 ms");
            return false;
        }
        readiness_wait_ = {};
        return frame_acquired_ = result == WAIT_OBJECT_0;
    }
    Bitmap capture() override {
        if (captured_.pixels.empty())
            throw std::logic_error("GPU frame capture was not enabled");
        return captured_;
    }

private:
    Bitmap readback() {
        if (!last_buffer_)
            throw std::logic_error("No DirectX frame to capture");
        D3D11_TEXTURE2D_DESC desc{};
        last_buffer_->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = desc.MiscFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        windows::ComPtr<ID3D11Texture2D> staging;
        check(device_->CreateTexture2D(&desc, nullptr, staging.put()));
        context_->CopyResource(staging.get(), last_buffer_.get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context_->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
        Bitmap result;
        try {
            result.width = width_;
            result.height = height_;
            result.pixels.resize(std::size_t(width_) * height_);
            for (int y = 0; y < height_; ++y)
                std::memcpy(
                    result.pixels.data() + std::size_t(y) * width_,
                    static_cast<const std::uint8_t*>(mapped.pData) + std::size_t(y) * mapped.RowPitch,
                    std::size_t(width_) * sizeof(std::uint32_t));
        } catch (...) {
            context_->Unmap(staging.get(), 0);
            throw;
        }
        context_->Unmap(staging.get(), 0);
        return result;
    }
};
} // namespace
std::unique_ptr<SwapPresenter> make_swap_presenter() {
    return std::make_unique<DirectxPresenter>();
}
} // namespace web
