# MrQuake2

**M**ultiple **R**enderers Quake 2, or **MrQuake2**, is a Quake 2 custom renderers playground.

Currently the following back ends are implemented:

- [D3D11 - Windows](https://github.com/glampert/MrQuake2/tree/master/src/renderers/d3d11)
- [D3D12 - Windows](https://github.com/glampert/MrQuake2/tree/master/src/renderers/d3d12)
- [Vulkan - Windows](https://github.com/glampert/MrQuake2/tree/master/src/renderers/vulkan)

The aim is to implement each renderer with the same visuals as the original Quake 2 but some modernizations are also implemented and can be toggled by CVars.
We also support loading higher quality textures such as the HD texture pack from [Yamagi Quake 2](https://www.yamagi.org/quake2/). There's also support for [RenderDoc](https://github.com/baldurk/renderdoc) debugging and profiling with [Optick](https://github.com/bombomby/optick).

This project is based on the [original Quake 2 source release](https://github.com/id-Software/Quake-2) from id Software.

See [`src/renderers`](https://github.com/glampert/MrQuake2/tree/master/src/renderers) for the back end implementations (C++) which compile into individual DLLs and the shaders used.
