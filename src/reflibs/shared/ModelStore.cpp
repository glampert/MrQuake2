//
// ModelStore.cpp
//  Generic 3D models/geometry loading and caching for all renderer back-ends.
//

#include "ModelStore.hpp"

///////////////////////////////////////////////////////////////////////////////

ModelStore::ModelStore()
{
    m_models_cache.reserve(kModelPoolSize);
    MemTagsTrackAlloc(m_models_cache.capacity() * sizeof(ModelInstance *), MemTag::kWorldModel);

    GameInterface::Printf("ModelStore instance created.");
}

///////////////////////////////////////////////////////////////////////////////

ModelStore::~ModelStore()
{
    // Anchor the vtable to this file.
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::BeginRegistration()
{
    //TODO
}

///////////////////////////////////////////////////////////////////////////////

void ModelStore::EndRegistration()
{
    //TODO
}

///////////////////////////////////////////////////////////////////////////////

const ModelInstance * ModelStore::Find(const char * const name, const ModelType mt)
{
    //TODO
    (void)name; (void)mt;
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const ModelInstance * ModelStore::FindOrLoad(const char * const name, const ModelType mt)
{
    //TODO
    (void)name; (void)mt;
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
