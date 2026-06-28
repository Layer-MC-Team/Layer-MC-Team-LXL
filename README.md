OpenLXL — 专为 Minecraft 优化的 OpenGL 3.3 翻译层

https://github.com/Layer-MC-Team/Layer-MC-Team-sign/raw/main/distribution.png

骁龙专精 | 稳定高效

🌐 语言
简体中文 | English

🎯 日后目标

 逐步升级至 OpenGL 4.6 伪装,解锁更多高级渲染特性
 持续优化骁龙 Adreno GPU 的渲染路径,冲击 150 FPS 性能天花板

✨ 项目简介

OpenLXL 是一个专为 Minecraft Java 版（安卓端）设计的 OpenGL 翻译层。底层基于 OpenGL ES 3.1+,上层伪装成 OpenGL 3.3,让游戏和模组以为你运行在标准的桌面 OpenGL 环境,从而启用最稳定的渲染路径。
核心定位：专攻骁龙（Adreno）GPU,在 Minecraft 场景下榨干每一帧性能,同时保持极高的兼容性和稳定性。

当前状态：稳定版已通过真机测试,画面正常,帧率可达 112~130 FPS（视机型而定）。

🚀 核心特性

特性 说明
OpenGL 3.3 伪装 通过 glGetString + 安全扩展注入,让游戏/模组以为是桌面 OpenGL 3.3
骁龙专属优化 针对 Adreno GPU 的指令批处理、显存预分配、GMEM 优化,实测高帧率
安全扩展注入 只注入 ES 3.1+ 原生支持的扩展（如 GL_ARB_buffer_storage）,不引入空函数风险
全架构支持 arm64-v8a、armeabi-v7a、x86_64、x86
模组兼容 Sodium、Iris、OptiFine、Create 等主流模组运行稳定
轻量级 源码仅 ~50MB,编译后 .so 约 2~3MB,适合嵌入各种启动器

🧩 技术原理

OpenLXL 的核心逻辑分为三层：

1. 版本伪装层（main.c）：
    glGetString(GL_VERSION) 返回 3.3 OpenLXL
    glGetIntegerv(GL_MAJOR_VERSION) 返回 3,MINOR_VERSION 返回 3
    让 Minecraft 和模组检测器认为你运行在标准的桌面 OpenGL 3.3 环境
2. 扩展注入层（egl.c）：
    在 glGetString(GL_EXTENSIONS) 返回的扩展列表中追加安全扩展
    只添加 ES 3.1+ 原生支持或可无损模拟的扩展（如 GL_ARB_buffer_storage、GL_ARB_timer_query）
3. 状态拦截层（proc.c + es3_overrides.h）：
    拦截关键函数（glBindBuffer、glUseProgram、glGetIntegerv 等）
    对高频调用做状态缓存,减少驱动开销

📦 支持平台与架构

架构 状态 推荐场景
arm64-v8a ✅ 稳定（主力） 所有现代安卓手机
armeabi-v7a ✅ 稳定 老旧 32 位设备
x86_64 ✅ 稳定 安卓模拟器
x86 ✅ 稳定 老旧模拟器

🛠️ 编译指南

环境要求

 Android NDK r29 或更高版本
 Make（Windows 可用 NDK 自带的 ndk-build）

步骤

1. 进入项目目录:
cd OpenLXL/lxl/src/main/lxl

2. 执行编译：

# 如果你配置了 NDK 环境变量
ndk-build

# 或直接指定 NDK 路径
/path/to/android-ndk-r29/ndk-build

3. 编译成功后,.so 文件位于 libs/<架构>/liblxl.so。

📱 使用方法

在启动器中加载

将 liblxl.so 放入启动器的渲染器目录,或在启动器设置中指定加载该 .so 文件。

推荐环境变量（PojavLauncher / FCL）：
LIBGL_ES=3:POJAV_RENDERER=opengles3_lxl:MESA_NO_ERROR=1:MESA_GLTHREAD=true:LIBGL_NOERROR=1:LXL_NEVER_FLUSH_BUFFERS=1:LXL_COHERENT_DYNAMIC_STORAGE=1:LXL_ENABLE_TIMER_QUERY=0:LXL_DEBUG=0:LXL_HIDE_BUFFER_STORAGE=0

验证

 启动 Minecraft,按 F3 查看右上角 OpenGL 版本,应为 3.3 OpenLXL
 如果开启光影,Iris / OptiFine 应正常识别并启用对应路径
📜 开源协议

本项目采用 LGPL-3.0 许可证。

 你可以自由使用、修改、分发本代码
 如果你修改了核心源码并分发,必须开源你的修改
 本协议允许闭源商业软件动态链接本库（即游戏/启动器可以正常使用）

完整协议文本请查看仓库根目录 LICENSE

🤝 贡献与致谢

欢迎提交 Issue 和 Pull Request！

代码风格与现有代码保持一致
新增功能需说明测试平台和结果
遵守 LGPL-3.0 许可证

交流 QQ 群：943941270
OpenLXL 用稳定的 3.3 伪装,为安卓 Minecraft 带来桌面级的渲染体验。未来,我们将向 4.6 迈进！ 🚀