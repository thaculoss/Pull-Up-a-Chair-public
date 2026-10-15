#pragma once
#include <windows.h>
#include <DirectXTex.h>
#include <algorithm>
#include <array>
#include <string>

namespace inn {
struct ThumbnailBounds {
    std::array<float,4> uv{0,0,1,1};
    float aspect{1};
    bool cropped{};
};

// Remove transparent thumbnail margins at draw time. Cache this small result
// on the menu item; keep the rendered PNG and shared texture unmodified.
inline ThumbnailBounds ReadThumbnailBounds(const std::string& path) noexcept {
    ThumbnailBounds result;
    const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return result;
    struct ComScope {
        bool owned;
        ~ComScope(){if(owned)CoUninitialize();}
    } scope{SUCCEEDED(initialized)};
    try {
        const auto length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.data(),static_cast<int>(path.size()),nullptr,0);
        if(length<=0)return result;
        std::wstring filename(length,L'\0');
        if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.data(),static_cast<int>(path.size()),filename.data(),length))return result;
        DirectX::ScratchImage source,converted;
        if(FAILED(DirectX::LoadFromWICFile(filename.c_str(),DirectX::WIC_FLAGS_FORCE_RGB,nullptr,source)))return result;
        const DirectX::Image* pixels=source.GetImage(0,0,0);
        if(!pixels||!pixels->width||!pixels->height||pixels->width>8192||pixels->height>8192)return result;
        result.aspect=static_cast<float>(pixels->width)/static_cast<float>(pixels->height);
        if(pixels->format!=DXGI_FORMAT_R8G8B8A8_UNORM&&pixels->format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
            if(FAILED(DirectX::Convert(*pixels,DXGI_FORMAT_R8G8B8A8_UNORM,DirectX::TEX_FILTER_DEFAULT,0.f,converted)))return result;
            pixels=converted.GetImage(0,0,0);
            if(!pixels)return result;
        }
        std::size_t left=pixels->width,top=pixels->height,right=0,bottom=0;
        for(std::size_t y=0;y<pixels->height;++y)for(std::size_t x=0;x<pixels->width;++x) {
            if(pixels->pixels[y*pixels->rowPitch+x*4+3]<=8)continue;
            left=std::min(left,x);top=std::min(top,y);
            right=std::max(right,x+1);bottom=std::max(bottom,y+1);
        }
        if(left>=right||top>=bottom)return result;
        constexpr std::size_t padding=4;
        left=left>padding?left-padding:0;top=top>padding?top-padding:0;
        right=std::min(right+padding,pixels->width);bottom=std::min(bottom+padding,pixels->height);
        const auto width=static_cast<float>(pixels->width),height=static_cast<float>(pixels->height);
        result.uv={left/width,top/height,right/width,bottom/height};
        result.aspect=static_cast<float>(right-left)/static_cast<float>(bottom-top);
        result.cropped=true;
    } catch(...) { /* A missing/corrupt preview keeps its full-image fallback. */ }
    return result;
}
}
