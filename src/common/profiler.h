//
// profiler.h
//  Optick profiler support for Quake2 (C wrapper).
//

#ifndef PROFILER_H
#define PROFILER_H

// Toggle profiler on/off
#define USE_OPTICK 1

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#if USE_OPTICK

void Optick_BeginFrame(void);
void Optick_EndFrame(void);
void Optick_PushEvent(const char* name);
void Optick_PopEvent(void);

#else // USE_OPTICK

static inline void Optick_BeginFrame(void) {}
static inline void Optick_EndFrame(void) {}
static inline void Optick_PushEvent(const char* name) { (void)name; }
static inline void Optick_PopEvent(void) {}

#endif // USE_OPTICK

#ifdef __cplusplus
} // extern C
#endif // __cplusplus

#endif // PROFILER_H
