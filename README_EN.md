OpenLXL — A Specialized OpenGL 3.3 Translation Layer for Minecraft
![picture](https://github.com/Layer-MC-Team/Layer-MC-Team-sign/raw/main/distribution.png)
Snapdragon‑focused | Stable & Efficient

🌐 Languages
简体中文 | English

---

🎯 Future Goals

· Gradually upgrade to OpenGL 4.6 emulation to unlock more advanced rendering features
· Continue optimizing the rendering path for Snapdragon Adreno GPUs, targeting 150 FPS performance

---

✨ Project Introduction

OpenLXL is an OpenGL translation layer designed specifically for Minecraft: Java Edition on Android. It is built on top of OpenGL ES 3.1+ and emulates OpenGL 3.3 at the upper layer, making games and mods believe they are running in a standard desktop OpenGL environment, thus enabling the most stable rendering path.

Core positioning: Focus on Snapdragon (Adreno) GPUs to extract every bit of performance in Minecraft scenarios while maintaining high compatibility and stability.

Current status: The stable version has passed real‑device testing – visuals are correct, and frame rates can reach 112–130 FPS (depending on the device).

---

🚀 Key Features

Feature Description
OpenGL 3.3 Emulation Uses glGetString + safe extension injection to make games/mods think they are on desktop OpenGL 3.3
Snapdragon‑specific Optimizations Instruction batching, pre‑allocated video memory, and GMEM optimisations for Adreno GPUs, delivering high frame rates
Safe Extension Injection Only injects extensions natively supported by ES 3.1+ (e.g., GL_ARB_buffer_storage), avoiding empty‑function risks
Full Architecture Support arm64‑v8a, armeabi‑v7a, x86_64, x86
Mod Compatibility Works stably with popular mods like Sodium, Iris, OptiFine, and Create
Lightweight Source code ~50 MB, compiled .so ~2‑3 MB – easy to embed in various launchers

---

🧩 Technical Principles

OpenLXL’s core logic is divided into three layers:

1. Version Emulation Layer (main.c):
   · glGetString(GL_VERSION) returns 3.3 OpenLXL
   · glGetIntegerv(GL_MAJOR_VERSION) returns 3, MINOR_VERSION returns 3
   · This makes Minecraft and mod detectors believe they are on a standard desktop OpenGL 3.3 environment
2. Extension Injection Layer (egl.c):
   · Appends safe extensions to the extension list returned by glGetString(GL_EXTENSIONS)
   · Only adds extensions that are natively supported by ES 3.1+ or can be losslessly emulated (e.g., GL_ARB_buffer_storage, GL_ARB_timer_query)
3. State Interception Layer (proc.c + es3_overrides.h):
   · Intercepts critical functions (glBindBuffer, glUseProgram, glGetIntegerv, etc.)
   · Caches frequently used calls to reduce driver overhead

---

📦 Supported Platforms & Architectures

Architecture Status Recommended Use
arm64‑v8a ✅ Stable (primary) All modern Android phones
armeabi‑v7a ✅ Stable Older 32‑bit devices
x86_64 ✅ Stable Android emulators
x86 ✅ Stable Older emulators

---

🛠️ Build Guide

Requirements

· Android NDK r29 or later
· Make (Windows can use ndk‑build bundled with the NDK)

Steps

1. Enter the project directory:

```bash
cd OpenLXL/lxl/src/main/lxl
```

2. Run the build:

```bash
# If you have NDK in your PATH
ndk-build

# Or specify the NDK path directly
/path/to/android-ndk-r29/ndk-build
```

3. After a successful build, the .so file is located in libs/<architecture>/liblxl.so.

---

📱 Usage

Loading in a Launcher

Place liblxl.so in the launcher’s renderer directory, or specify it in the launcher settings to load this .so file.

Recommended environment variables (for PojavLauncher / FCL):

```bash
LIBGL_ES=3:POJAV_RENDERER=opengles3_lxl:MESA_NO_ERROR=1:MESA_GLTHREAD=true:LIBGL_NOERROR=1:LXL_NEVER_FLUSH_BUFFERS=1:LXL_COHERENT_DYNAMIC_STORAGE=1:LXL_ENABLE_TIMER_QUERY=0:LXL_DEBUG=0:LXL_HIDE_BUFFER_STORAGE=0
```

Verification

· Launch Minecraft and press F3 to check the OpenGL version in the top‑right corner – it should show 3.3 OpenLXL
· If shaders are enabled, Iris / OptiFine should recognise and use the corresponding rendering path

---

📜 License

This project is licensed under LGPL‑3.0.

· You are free to use, modify, and distribute this code
· If you modify the core source and distribute it, you must open‑source your modifications
· This license allows closed‑source commercial software to dynamically link this library (i.e., games/launchers can use it normally)

The full license text is available in the LICENSE file in the repository root.

---

🤝 Contributions & Acknowledgements

We welcome Issues and Pull Requests!

· Code style should be consistent with existing code
· New features should include test platform and results
· Please adhere to the LGPL‑3.0 license

QQ Group: 943941270

---

OpenLXL uses stable 3.3 emulation to bring a desktop‑grade rendering experience to Android Minecraft. In the future, we will move towards 4.6! 🚀
