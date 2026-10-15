std::string Normalize(std::string s,std::string_view prefix) {
    std::replace(s.begin(),s.end(),'/','\\');
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(!s.starts_with(prefix))s.insert(0,prefix);return s;
}
std::uint64_t AssetHash(const std::string& path) {
    if(auto it=assetHashes.find(path);it!=assetHashes.end())return it->second;
    auto hash=food::Hash(path);RE::BSResourceNiBinaryStream stream(path);
    if(stream.good()&&stream.stream&&stream.stream->totalSize) {
        std::array<char,65536> bytes;std::uint64_t remaining=stream.stream->totalSize;
        while(remaining) {
            auto size=static_cast<std::size_t>(std::min<std::uint64_t>(remaining,bytes.size()));
            if(!stream.read(bytes.data(),static_cast<std::uint32_t>(size))){hash=food::Hash("unreadable",hash);break;}
            hash=food::Hash(std::string_view(bytes.data(),size),hash);remaining-=size;
        }
    }else hash=food::Hash("missing",hash);
    return assetHashes[path]=hash;
}
std::string Thumbnail(RE::AlchemyItem* alch,const std::string& path) {
    struct ComScope {
        HRESULT result{CoInitializeEx(nullptr,COINIT_MULTITHREADED)};
        ~ComScope(){if(SUCCEEDED(result))CoUninitialize();}
    } com;
    if(FAILED(com.result)&&com.result!=RPC_E_CHANGED_MODE)throw std::runtime_error("图片编码器初始化失败");
    if(auto it=imageCache.find(alch->GetFormID());it!=imageCache.end()&&std::filesystem::exists(it->second))return it->second;
    constexpr auto resolution=inn::thumbnail::resolution;
    auto mesh=RenderManager::AddByNifPAth(path.c_str(),resolution,resolution);
    if(!mesh)throw std::runtime_error("模型解析失败");
    struct Cleanup { MeshRenderingFrameworkAPI::Internal::IMesh* mesh; ~Cleanup(){RenderManager::Delete(mesh);} } cleanup{mesh};
    auto source=RenderManager::meshes.at(mesh);
    // The TXST record order differs from the NIF shader texture order.
    for(std::uint32_t i=0;i<alch->numAlternateTextures;++i) {
        auto& alternate=alch->alternateTextures[i];auto tx=alternate.textureSet;if(!tx)continue;
        for(auto& part:source->parts)if(part.shapeName==Normalize(alternate.name3D.c_str(),"")) {
            constexpr std::array<std::size_t,8> slots{0,1,5,2,3,4,6,7};
            for(std::size_t t=0;t<8;++t) {
                auto texture=tx->textures[t].textureName.c_str();
                auto slot=slots[t];
                if(t==7&&tx->flags.any(RE::BGSTextureSet::Flag::kHasModelSpaceNormalMap))slot=8;
                if(texture&&texture[0])part.texturePaths[slot]=Normalize(texture,"textures\\");
            }
            part.modelSpaceNormals=tx->flags.any(RE::BGSTextureSet::Flag::kHasModelSpaceNormalMap);
        }
    }
    auto hash=food::Hash(inn::thumbnail::cacheVersion,AssetHash(Normalize(path,"meshes\\")));
    for(auto& part:source->parts) {
        hash=food::Hash(part.shapeName,hash);
        hash=food::Hash(part.modelSpaceNormals?"model-space":"tangent-space",hash);
        for(std::size_t slot=0;slot<part.texturePaths.size();++slot)if(!part.texturePaths[slot].empty()) {
            hash=food::Hash(std::to_string(slot),hash);
            hash=food::Hash(std::to_string(AssetHash(Normalize(part.texturePaths[slot],"textures\\"))),hash);
        }
    }
    auto output=fmt::format("{}/{:016x}.png",cacheDir,hash);
    if(std::filesystem::exists(output)&&std::filesystem::file_size(output)>100)return imageCache[alch->GetFormID()]=output;
    if(!rendererReady) {
        auto r=RE::BSGraphics::Renderer::GetSingleton();
        auto d=r?reinterpret_cast<ID3D11Device*>(r->GetRuntimeData().forwarder):nullptr;
        rendererReady=d&&RenderManager::Init(d,nullptr);
    }
    if(!rendererReady)throw std::runtime_error("预览渲染器初始化失败");
    auto normalized=Normalize(path,"meshes\\");
    inn::thumbnail::Configure(mesh,normalized);
    auto target=new RenderTarget();target->width=resolution;target->height=resolution;target->numReferences=1;
    RenderManager::renderTarget[RenderTarget::GetKey(resolution,resolution)]=target;
    std::filesystem::create_directories(cacheDir);
    if(!RenderManager::Render(mesh)||!RenderManager::Save(mesh,output.c_str()))throw std::runtime_error("缩略图生成失败");
    logger::info("Thumbnail form={:08X} model={} cache={}",alch->GetFormID(),path,output);
    return imageCache[alch->GetFormID()]=output;
}
void QueueThumbnail() {
    if(thumbQueued.exchange(true))return;
    int token=session.token,index=-1;
    if(session.tab==3) {
        // Generate the visible meals first, starting with the focused card.
        // These are the same rows/models used by the individual dish pages.
        for(int turn=0;turn<2&&index<0;++turn)
            for(auto key:BundleItems((session.bundleHighlighted+turn)%2)) {
                if(!key)continue;
                auto row=std::find_if(session.items.begin(),session.items.end(),[key](const Item& item){return inn::RowKey(item)==key&&!item.attempted;});
                if(row!=session.items.end()){index=static_cast<int>(row-session.items.begin());break;}
            }
    } else {
        if(session.highlighted>=0&&session.highlighted<static_cast<int>(session.items.size())&&!session.items[session.highlighted].attempted)index=session.highlighted;
        if(index<0)for(std::size_t i=0;i<session.items.size();++i)if(!session.items[i].attempted){index=static_cast<int>(i);break;}
    }
    if(index<0){thumbQueued=false;return;}
    session.items[index].attempted=true;auto id=session.items[index].id;auto path=session.items[index].model;
    SKSE::GetTaskInterface()->AddTask([token,index,id,path]{
        {std::lock_guard lock(gate);if(session.token!=token||session.phase!=food::Phase::open){thumbQueued=false;return;}}
        std::string image,error;
        try {
            if(purgeAssets.exchange(false)){assetHashes.clear();imageCache.clear();}
            auto a=RE::TESForm::LookupByID<RE::AlchemyItem>(id);
            if(a&&!path.empty())image=Thumbnail(a,path);else error="没有可预览的模型";
        }catch(const std::exception& e){error=e.what();logger::warn("Thumbnail failed {:08X}: {}",id,error);}
        std::lock_guard lock(gate);
        if(session.token==token&&index<static_cast<int>(session.items.size())){session.items[index].image=image;session.items[index].error=error;}
        thumbQueued=false;
    });
}
