# ECSEngine
 Data Oriented Game Engine With A ECS Architecture.

# Summary
This is a C++ Game Engine designed to be among the fastest and the most responsive game engine to use. It is in active development, all parts being highly volatile. It uses C++ scripting based on dlls, with hot reloading for editor iterations. This provides access to the native engine functions without any wrappers or marshalling. The design is highly modular, the functionality of the engine (Graphics, Collision Detection, Physics, and all others) being developed as modules, not as hard coded utilities in the engine.

# Screenshots
![Prefab](https://github.com/TheRealANDREWQA/ECSEngine/assets/68424250/2b2edbb3-7165-44d4-bd12-97f943732f80)
<img width="1280" alt="Screenshot_276" src="https://github.com/TheRealANDREWQA/ECSEngine/assets/68424250/7418ad2a-7a2b-4c53-b0ad-3ceeee97f35b">

# Platforms
- OS: Windows
- Graphics API: DirectX 11
- Compiler: MSVC
Since this is a solo effort, having to support and test multiple platforms is very hard, so the scope was reduced to the mainstream gaming platform.

# Features (worthy of mention)
- Sandboxes. The editor for ECSEngine is implemented such that you can have many scenes in memory at the same time. Each sandbox is completely independent from the others, and they run inside the same process as the editor. This allows for asset resource sharing between them. If an asset is referenced in 2 or more sandboxes, the editor detects this and it will alias the resource, drastically reducing memory consumption (CPU and GPU alike). This can provide unique workflows compared to other game engines, where you can have 2 sandboxes with a small setting difference be run side by side and watch or analyze the impact of the feature. The input to sandboxes can be splatted (it's an option), such that you can interact with all/some of them at once
- Custom C++ Reflection, able to reflect most relevant structures. With this, automatic serialization and UI generation is implemented.
- Automatic module compilation. For graphics modules that are used by a sandbox, each time a change is detected, in scene or runtime mode, it will try to compile it automatically. For normal modules, only during runtime the automatic compilation will be triggered.
- Full debugging support for C++ modules. Every time you use a module, the engine will correctly handle the related .pdb file such that you get native debugging capabilities.
- Configuration enabled modules. Each module currently can have 3 configurations: Debug, Release (A better name for this is Optimized Debug) and Distribution. Each sandbox chooses a certain configuration to use. This allows for having well tested and known to be correct modules in the highest optimization possible, while modules in development can be relegated to Release (Optimized Debug) to allow for debuggability. Release is the configuration used by ECSEngine where small optimizations are enabled that provide a significant performance gain without any concernable debuggability loss.
- Multiple options per asset file. Each mesh, texture can have multiple different sets of options, allowing you to see the delta between using a set compared to another set.
- Editor responsiveness. The ECSEngine editor was implemented using as much multithreading as possible, providing you with an unparalleled level of responsiveness.

# Active Modules (In current development)
- Graphics
- Collision Detection
- Physics

# Planned features
- Graphics
   - An antialiasing solution (Second Depth Aliasing is the primary consideration)
   - GPU driven rendering
   - Global Illumination
- Collision Detection
   - Robust Narrowphase detection
