#pragma once
#include "RenderManager.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace inn::thumbnail {
inline constexpr std::uint32_t resolution=512;
inline constexpr const char* cacheVersion="food-gallery-v4-material-lighting512";
inline void Configure(MeshRenderingFrameworkAPI::Internal::IMesh* mesh,std::string_view model) {
    auto source=RenderManager::meshes.at(mesh);
    RenderManager::ClearLights(mesh);
    RenderManager::AddLight(mesh,-0.60f,0.70f,0.65f,1.0f,0.95f,0.86f,2.35f);
    RenderManager::AddLight(mesh,0.65f,0.50f,0.15f,0.75f,0.83f,1.0f,0.50f);
    RenderManager::AddLight(mesh,0.20f,-0.65f,0.65f,1.0f,0.88f,0.72f,0.60f);
    RenderManager::SetExposure(mesh,1.35f);
    for(auto& part:source->parts) {
        bool packed=false;
        for(auto& texture:part.texturePaths) {
            auto normalized=texture;
            std::transform(normalized.begin(),normalized.end(),normalized.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            if(normalized.find("_rmaos")!=std::string::npos){packed=true;texture.clear();}
        }
        if(packed) {
            // Packed roughness/metalness/AO data is not an emissive/SSS color map.
            // The retained renderer uses a conventional specular model, so use
            // conservative thumbnail-only parameters rather than false SSS.
            part.specularEnabled=true;
            part.specularStrength=0.14f;
            part.glossiness=36.f;
        }
        part.environmentScale=std::min(part.environmentScale,0.25f);
        part.emissiveEnabled=false;
    }
    const bool bowl=model.find("stew")!=std::string_view::npos||model.find("soup")!=std::string_view::npos||model.find("bowl")!=std::string_view::npos;
    const bool drink=model.find("wine")!=std::string_view::npos||model.find("mead")!=std::string_view::npos||model.find("beer")!=std::string_view::npos;
    mesh->rotation.SetEulerAnglesXYZ(bowl?0.f:drink?0.30f:0.20f,0.f,0.f);
    mesh->mustUpdate=true;
}
}
