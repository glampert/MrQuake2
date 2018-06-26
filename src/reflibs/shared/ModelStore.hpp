//
// ModelStore.hpp
//  Generic 3D models/geometry loading and caching for all renderer back-ends.
//
#pragma once

#include "ModelStructs.hpp"

#include <vector>
#include <memory>

namespace MrQ2
{

/*
===============================================================================

    ModelStore

===============================================================================
*/
class ModelStore
{
public:

    static constexpr int kModelPoolSize = 512;

    explicit ModelStore(TextureStore & tex_store);
    virtual ~ModelStore();

    // Registration sequence:
    virtual void BeginRegistration(const char * map_name);
    virtual void EndRegistration();

    std::uint32_t RegistrationNum() const { return m_registration_num; }
    ModelInstance * WorldModel() const { return m_world_model; }

    // Models cache:
    const ModelInstance * Find(const char * name, ModelType mt);       // Must be in cache, null otherwise
    const ModelInstance * FindOrLoad(const char * name, ModelType mt); // Load if necessary
    virtual ModelInstance * GetInlineModel(int model_index) = 0;

protected:

    // Back-end hooks:
    virtual ModelInstance * CreateModel(const char * name, ModelType mt, std::uint32_t regn) = 0;
    virtual void DestroyModel(ModelInstance * mdl) = 0;

    // So the back end can cleanup on exit.
    void DestroyAllLoadedModels();

private:

    void LoadWorldModel(const char * map_name);
    ModelInstance * FindInlineModel(const char * name);
    ModelInstance * LoadNewModel(const char * name);
    void ReferenceAllTextures(ModelInstance & mdl);

private:

    TextureStore & m_tex_store;

    // Loaded models cache
    std::uint32_t m_registration_num = 0;
    std::vector<ModelInstance *> m_models_cache;

    // Cached pointer to currently loaded map
    ModelInstance * m_world_model = nullptr;
};

// ============================================================================

// Defined in ModelLoad.cpp
void LoadBrushModel(ModelStore & mdl_store, TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const int mdl_data_len);
void LoadSpriteModel(TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const int mdl_data_len);
void LoadAliasMD2Model(TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const int mdl_data_len);

// ============================================================================

} // MrQ2
