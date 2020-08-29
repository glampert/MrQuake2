//
// ModelStore.hpp
//  Generic 3D models/geometry loading and caching for all renderer back-ends.
//
#pragma once

#include "ModelStructs.hpp"
#include "Pool.hpp"
#include "Array.hpp"

namespace MrQ2
{

class TextureStore;

/*
===============================================================================

    ModelStore

===============================================================================
*/
class ModelStore final
{
public:

    static constexpr int kModelPoolSize = 512;

    void Init(TextureStore & tex_store);
    void Shutdown();

    // Registration sequence:
    void BeginRegistration(const char * map_name);
    void EndRegistration();

    std::uint32_t RegistrationNum() const { return m_registration_num; }
    ModelInstance * WorldModel() const { return m_world_model; }
	ModelInstance * InlineModelAt(std::uint32_t index) const { return m_inline_models[index]; }

    // Models cache:
    const ModelInstance * Find(const char * name, ModelType mt);       // Must be in cache, returns null otherwise
    const ModelInstance * FindOrLoad(const char * name, ModelType mt); // Load if necessary

    // Defined in ModelLoad.cpp
    void LoadBrushModel(TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const int mdl_data_len);
    static void LoadSpriteModel(TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const int mdl_data_len);
    static void LoadAliasMD2Model(TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const int mdl_data_len);

private:

    ModelInstance * CreateModel(const char * name, ModelType mt, std::uint32_t regn, bool inline_mdl);
    void DestroyModel(ModelInstance * mdl);
    void DestroyAllLoadedModels();

    void LoadWorldModel(const char * map_name);
    ModelInstance * FindInlineModel(const char * name);
    ModelInstance * LoadNewModel(const char * name);
    void ReferenceAllTextures(ModelInstance & mdl);

private:

    ModelInstance * m_world_model{ nullptr }; // Cached pointer to currently loaded map
    TextureStore *  m_tex_store{ nullptr };
    std::uint32_t   m_registration_num{ 0 };

    Pool<ModelInstance, kModelPoolSize> m_models_pool{ MemTag::kWorldModel };
    FixedSizeArray<ModelInstance *, kModelPoolSize> m_models_cache;
    FixedSizeArray<ModelInstance *, kModelPoolSize> m_inline_models;
};

} // MrQ2
