//
// profiler.cpp
//  Optick profiler support for Quake2.
//
//  Link against src/external/optick/lib/x64/[debug|release]/OptickCore.lib
//  and copy OptickCore.dll to the same folder where the executable is.
//

#include "profiler.h"

#if USE_OPTICK

#include "external/optick/include/optick.h"

extern "C" {

void Optick_BeginFrame(void)
{
    static Optick::ThreadScope mainThreadScope{ "MainThread" };
    OPTICK_UNUSED(mainThreadScope);
    Optick::EndFrame();
    Optick::Update();
    const uint32_t frameNumber = Optick::BeginFrame();
    OPTICK_PUSH_DYNAMIC(*Optick::GetFrameDescription());
    OPTICK_TAG("Frame", frameNumber);
}

void Optick_EndFrame(void)
{
    OPTICK_POP();
}

void Optick_PushEvent(const char * name)
{
    OPTICK_PUSH_DYNAMIC(name);
}

void Optick_PopEvent(void)
{
    OPTICK_POP();
}

} // extern "C"

#endif // USE_OPTICK
