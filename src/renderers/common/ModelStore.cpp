//
// ModelStore.cpp
//  Generic 3D models/geometry loading and caching for all renderer back-ends.
//

#include "ModelStore.hpp"
#include "TextureStore.hpp"

// Quake includes
#include "common/q_common.h"
#include "common/q_files.h"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////

// Verbose debugging/logging
constexpr bool kVerboseModelStore = true;

///////////////////////////////////////////////////////////////////////////////

void ModelStore::Init(TextureStore & tex_store)
{
    MRQ2_ASSERT(m_tex_store == nullptr);
    m_tex_store = &tex_store;

    // First page in the pool will contain the inlines.
    for (int m = 0; m < kModelPoolSize; ++m)
    {
        char name[128]; // Give default names to the inline models
        sprintf_s(name, "inline_model_%i", m);

        auto * mdl = CreateModel(name, ModelType::kBrush, /* reg_num = */0u, /* inline_mdl = */true);
        m_inline_models.push_back(mdl);
    }

    GameInterface::Printf("ModelStore initialized.");
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::Shutdown()
{
    DestroyAllLoadedModels();
    m_models_cache.clear();
    m_inline_models.clear();
    m_models_pool.Drain();
    m_registration_num = 0;
    m_tex_store = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

ModelInstance * ModelStore::CreateModel(const char * name, ModelType mt, std::uint32_t regn, bool inline_mdl)
{
    auto * mdl = m_models_pool.Allocate();
    Construct(mdl, name, mt, regn, inline_mdl);
    return mdl;
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::DestroyModel(ModelInstance * mdl)
{
    Destroy(mdl);
    m_models_pool.Deallocate(mdl);
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::DestroyAllLoadedModels()
{
    m_world_model = nullptr;

    for (ModelInstance * mdl : m_inline_models)
    {
        DestroyModel(mdl);
    }
    m_inline_models.clear();

    for (ModelInstance * mdl : m_models_cache)
    {
        DestroyModel(mdl);
    }
    m_models_cache.clear();
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::BeginRegistration(const char * const map_name)
{
    MRQ2_ASSERT(map_name != nullptr && map_name[0] != '\0');

    GameInterface::Printf("==== ModelStore::BeginRegistration '%s' ====", map_name);
    ++m_registration_num;

    LoadWorldModel(map_name);
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::EndRegistration()
{
    GameInterface::Printf("==== ModelStore::EndRegistration ====");

    int num_removed = 0;
    auto RemovePred = [this, &num_removed](ModelInstance * mdl)
    {
        if (mdl->reg_num != m_registration_num)
        {
            DestroyModel(mdl);
            ++num_removed;
            return true;
        }
        return false;
    };

    m_models_cache.erase_if(RemovePred);

    GameInterface::Printf("Freed %i unused models.", num_removed);
}

///////////////////////////////////////////////////////////////////////////////

const ModelInstance * ModelStore::Find(const char * const name, const ModelType mt)
{
    MRQ2_ASSERT(name != nullptr && name[0] != '\0');
    MRQ2_ASSERT(mt != ModelType::kCount);

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
    sprintf_s(fullname, "maps/%s.bsp", map_name);

    // Free the previous map if we are loading a new one:
    if ((m_world_model != nullptr) && (std::strcmp(m_world_model->name.CStr(), fullname) != 0))
    {
        if (kVerboseModelStore)
        {
            GameInterface::Printf("Unloading current map '%s'...", m_world_model->name.CStr());
        }

        auto erase_iter = std::find(m_models_cache.begin(), m_models_cache.end(), m_world_model);
        MRQ2_ASSERT(erase_iter != m_models_cache.end());
        m_models_cache.erase_swap(erase_iter);

        DestroyModel(m_world_model);
        m_world_model = nullptr;
    }

    // Load/reference the world map:
    m_world_model = const_cast<ModelInstance *>(FindOrLoad(fullname, ModelType::kBrush));
    if (m_world_model == nullptr)
    {
        GameInterface::Errorf("ModelStore: Unable to load level map '%s'!", fullname);
    }
}

///////////////////////////////////////////////////////////////////////////////

ModelInstance * ModelStore::FindInlineModel(const char * const name)
{
    const int idx = atoi(name + 1);
    if (idx < 1 || m_world_model == nullptr || idx >= m_world_model->data.num_submodels)
    {
       GameInterface::Errorf("Bad inline model number or null world model (%i)", idx);
    }
    return m_inline_models[idx];
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
    ModelInstance * new_model = CreateModel(name, mdl_type, m_registration_num, /* inline_mdl = */false);
    switch (id)
    {
    case IDBSPHEADER :
        LoadBrushModel(*m_tex_store, *new_model, file.data_ptr, file.length);
        break;
    case IDSPRITEHEADER :
        LoadSpriteModel(*m_tex_store, *new_model, file.data_ptr, file.length);
        break;
    case IDALIASHEADER :
        if (Config::r_hd_skins.IsSet())
        {
            // If we have higher definition overrides for MD2 model skins also check if
            // there is a replacement model for it in the equivalent mrq2/ directory.

            char hd_mdl_name[512];
            sprintf_s(hd_mdl_name, "mrq2/%s", name);

            GameInterface::FS::ScopedFile hd_mdl_file{ hd_mdl_name };
            if (hd_mdl_file.IsLoaded())
            {
                LoadAliasMD2Model(*m_tex_store, *new_model, hd_mdl_file.data_ptr, hd_mdl_file.length);
            }
            else
            {
                LoadAliasMD2Model(*m_tex_store, *new_model, file.data_ptr, file.length);
            }
        }
        else
        {
            LoadAliasMD2Model(*m_tex_store, *new_model, file.data_ptr, file.length);
        }
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
                    tex->m_reg_num = m_tex_store->RegistrationNum();
                }
            }
            break;
        }
    case ModelType::kSprite :
        {
            auto * p_sprite = mdl.hunk.ViewBaseAs<dsprite_t>();
            MRQ2_ASSERT(p_sprite != nullptr);
            for (int i = 0; i < p_sprite->numframes; ++i)
            {
                mdl.data.skins[i] = m_tex_store->FindOrLoad(p_sprite->frames[i].name, TextureType::kSprite);
            }
            break;
        }
    case ModelType::kAliasMD2 :
        {
            auto * p_md2 = mdl.hunk.ViewBaseAs<dmdl_t>();
            MRQ2_ASSERT(p_md2 != nullptr);
            for (int i = 0; i < p_md2->num_skins; ++i)
            {
                auto * skin_name = reinterpret_cast<const char *>(p_md2) + p_md2->ofs_skins + (i * MAX_SKINNAME);
                mdl.data.skins[i] = m_tex_store->FindOrLoad(skin_name, TextureType::kSkin);
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
