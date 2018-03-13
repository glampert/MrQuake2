//
// ModelStore.cpp
//  Generic 3D models/geometry loading and caching for all renderer back-ends.
//

#include "ModelStore.hpp"
#include "TextureStore.hpp"

#include <algorithm>

// Quake includes
extern "C"
{
#include "common/q_common.h"
#include "common/q_files.h"
} // extern "C"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////

// Verbose debugging/logging
constexpr bool kVerboseModelStore = true;

///////////////////////////////////////////////////////////////////////////////

ModelStore::ModelStore(TextureStore & tex_store)
    : m_tex_store{ tex_store }
{
    m_models_cache.reserve(kModelPoolSize);
    MemTagsTrackAlloc(m_models_cache.capacity() * sizeof(ModelInstance *), MemTag::kRenderer);

    GameInterface::Printf("ModelStore instance created.");
}

///////////////////////////////////////////////////////////////////////////////

ModelStore::~ModelStore()
{
    // Anchor the vtable to this file.
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::DestroyAllLoadedModels()
{
    m_world_model = nullptr;

    for (ModelInstance * mdl : m_models_cache)
    {
        DestroyModel(mdl);
    }
    m_models_cache.clear();
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::BeginRegistration(const char * const map_name)
{
    FASTASSERT(map_name != nullptr && map_name[0] != '\0');

    GameInterface::Printf("==== ModelStore::BeginRegistration '%s' ====", map_name);
    ++m_registration_num;

    LoadWorldModel(map_name);
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::EndRegistration()
{
    GameInterface::Printf("==== ModelStore::EndRegistration ====");

    int num_removed = 0;
    auto remove_pred = [this, &num_removed](ModelInstance * mdl)
    {
        if (mdl->reg_num != m_registration_num)
        {
            DestroyModel(mdl);
            ++num_removed;
            return true;
        }
        return false;
    };

    // "erase_if"
    auto first = m_models_cache.begin();
    auto last  = m_models_cache.end();
    m_models_cache.erase(std::remove_if(first, last, remove_pred), last);

    GameInterface::Printf("Freed %i unused models.", num_removed);
}

///////////////////////////////////////////////////////////////////////////////

const ModelInstance * ModelStore::Find(const char * const name, const ModelType mt)
{
    FASTASSERT(name != nullptr && name[0] != '\0');
    FASTASSERT(mt != ModelType::kCount);

    // Inline models are handled differently:
    if (name[0] == '*')
    {
        return FindInlineModel(name);
    }

    // Search the currently loaded models; compare by hash.
    const std::uint32_t name_hash = PathName::CalcHash(name);
    for (ModelInstance * mdl : m_models_cache)
    {
        // If name and type match, we are done.
        const bool type_match = (mt == ModelType::kAny) || (mdl->type == mt);
        if (type_match && name_hash == mdl->name.Hash())
        {
            if (kVerboseModelStore)
            {
                GameInterface::Printf("Model '%s' already in cache.", name);
            }

            mdl->reg_num = m_registration_num;
            ReferenceAllTextures(*mdl); // Ensure textures have the most current registration number
            return mdl;
        }
    }

    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const ModelInstance * ModelStore::FindOrLoad(const char * const name, const ModelType mt)
{
    // Lookup the cache first:
    const ModelInstance * mdl = Find(name, mt);

    // Load 'n cache new if not found:
    if (mdl == nullptr)
    {
        if (ModelInstance * new_mdl = LoadNewModel(name))
        {
            m_models_cache.push_back(new_mdl); // Put in the cache
            mdl = new_mdl;

            if (kVerboseModelStore)
            {
                GameInterface::Printf("Loaded model '%s'...", name);
            }
        }
    }

    return mdl;
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::LoadWorldModel(const char * const map_name)
{
    char fullname[1024];
    std::snprintf(fullname, sizeof(fullname), "maps/%s.bsp", map_name);

    // Free the previous map if we are loading a new one:
    if ((m_world_model != nullptr) && (std::strcmp(m_world_model->name.CStr(), fullname) != 0))
    {
        if (kVerboseModelStore)
        {
            GameInterface::Printf("Unloading current map '%s'...", m_world_model->name.CStr());
        }

        auto erase_iter = std::find(m_models_cache.begin(), m_models_cache.end(), m_world_model);
        FASTASSERT(erase_iter != m_models_cache.end());
        m_models_cache.erase(erase_iter);

        DestroyModel(const_cast<ModelInstance *>(m_world_model));
        m_world_model = nullptr;
    }

    // Load/reference the world map:
    m_world_model = FindOrLoad(fullname, ModelType::kBrush);
    if (m_world_model == nullptr)
    {
        GameInterface::Errorf("ModelStore: Unable to load level map '%s'!", fullname);
    }
}

///////////////////////////////////////////////////////////////////////////////

ModelInstance * ModelStore::FindInlineModel(const char * const name)
{
    // TODO
    return CreateModel(name, ModelType::kBrush, m_registration_num);
}

///////////////////////////////////////////////////////////////////////////////

ModelInstance * ModelStore::LoadNewModel(const char * const name)
{
    GameInterface::FS::ScopedFile file{ name };
    if (!file.IsLoaded())
    {
        GameInterface::Printf("WARNING: Unable to find model '%s'! Failed to open file.", name);
        return nullptr;
    }

    const std::uint32_t id = *static_cast<const std::uint32_t *>(file.data_ptr);

    // Figure out the file type:
    ModelType mdl_type;
    switch (id)
    {
    case IDBSPHEADER    : mdl_type = ModelType::kBrush;    break;
    case IDSPRITEHEADER : mdl_type = ModelType::kSprite;   break;
    case IDALIASHEADER  : mdl_type = ModelType::kAliasMD2; break;
    default : GameInterface::Errorf("ModelStore: Unknown file id (0x%X) for '%s'!", id, name);
    } // switch

    // Call the appropriate loader:
    ModelInstance * new_model = CreateModel(name, mdl_type, m_registration_num);
    switch (id)
    {
    case IDBSPHEADER :
        LoadBrushModel(m_tex_store, *new_model, file.data_ptr, file.length);
        break;
    case IDSPRITEHEADER :
        LoadSpriteModel(m_tex_store, *new_model, file.data_ptr, file.length);
        break;
    case IDALIASHEADER :
        LoadAliasMD2Model(m_tex_store, *new_model, file.data_ptr, file.length);
        break;
    } // switch

    return new_model;
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::ReferenceAllTextures(ModelInstance & mdl)
{
    switch (mdl.type)
    {
    case ModelType::kBrush :
        {
            for (int i = 0; i < mdl.data.num_texinfos; ++i)
            {
                if (mdl.data.texinfos[i].teximage != nullptr)
                {
                    // Update to current registration num - no need to do a Find()
                    auto * tex = const_cast<TextureImage *>(mdl.data.texinfos[i].teximage);
                    tex->reg_num = m_tex_store.RegistrationNum();
                }
            }
            break;
        }
    case ModelType::kSprite :
        {
            auto * p_sprite = mdl.hunk.ViewBaseAs<dsprite_t>();
            FASTASSERT(p_sprite != nullptr);
            for (int i = 0; i < p_sprite->numframes; ++i)
            {
                mdl.data.skins[i] = m_tex_store.FindOrLoad(p_sprite->frames[i].name, TextureType::kSprite);
            }
            break;
        }
    case ModelType::kAliasMD2 :
        {
            auto * p_md2 = mdl.hunk.ViewBaseAs<dmdl_t>();
            FASTASSERT(p_md2 != nullptr);
            for (int i = 0; i < p_md2->num_skins; ++i)
            {
                auto * skin_name = reinterpret_cast<const char *>(p_md2) + p_md2->ofs_skins + (i * MAX_SKINNAME);
                mdl.data.skins[i] = m_tex_store.FindOrLoad(skin_name, TextureType::kSkin);
            }
            mdl.data.num_frames = p_md2->num_frames;
            break;
        }
    default :
        GameInterface::Errorf("ModelStore: Bad model type for '%s'", mdl.name.CStr());
    } // switch
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
