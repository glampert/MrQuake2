//
// ModelStore.hpp
//  Generic 3D models/geometry loading and caching for all renderer back-ends.
//
#pragma once

#include "ModelStructs.hpp"

#include <vector>
#include <memory>

/*
===============================================================================

    ModelStore

===============================================================================
*/
class ModelStore
{
public:

    static constexpr int kModelPoolSize = 512;

    ModelStore();
    virtual ~ModelStore();

    // Registration sequence:
    virtual void BeginRegistration();
    virtual void EndRegistration();
    std::uint32_t RegistrationNum() const { return m_registration_num; }

    // Models cache:
    const ModelInstance * Find(const char * name, ModelType mt);       // Must be in cache, null otherwise
    const ModelInstance * FindOrLoad(const char * name, ModelType mt); // Load if necessary

private:

    // Loaded models cache
    std::uint32_t m_registration_num = 0;
    std::vector<ModelInstance *> m_models_cache;
};

// ============================================================================
