project('te', 'cpp', 'c', default_options: ['cpp_std=c++2a'])

cpp = meson.get_compiler('cpp')

threads = dependency('threads')
glfw3 = dependency('glfw3', version: '>=3.3')
glad = declare_dependency(include_directories: 'deps/glad/include')
glad_src = ['deps/glad/src/glad.c']
freeimage = dependency('freeimage')
fmod = declare_dependency (
  include_directories: 'deps/fmodstudioapi20007linux/api/core/inc/',
  dependencies: cpp.find_library('fmod', dirs: '/home/elliot/projects/te/deps/fmodstudioapi20107linux/api/core/lib/x86_64')
)
boost = dependency('boost', version: '>=1.73') #signals need not be specifically listed as it's header-only.
fmt = dependency('fmt')
fxgltf = declare_dependency(include_directories: 'deps/fx-gltf/include')
entt = declare_dependency(include_directories: 'deps/entt/src')
networking = declare_dependency (
  include_directories: 'deps/GameNetworkingSockets/include',
  dependencies: cpp.find_library('GameNetworkingSockets', dirs: '/home/elliot/projects/te/deps/GameNetworkingSockets/build/bin')
)
networking_flags = ['-DSTEAMNETWORKINGSOCKETS_OPENSOURCE', '-DPOSIX', '-DLINUX']
freetype = dependency('freetype2')
harfbuzz = dependency('harfbuzz')
ibus = dependency('ibus-1.0')
spdlog = dependency('spdlog')
guile = dependency('guile-3.0')

imgui = declare_dependency(include_directories: 'deps/imgui-1.76')
imgui_src = ['deps/imgui-1.76/imgui.cpp', 'deps/imgui-1.76/imgui_demo.cpp', 'deps/imgui-1.76/imgui_draw.cpp', 'deps/imgui-1.76/imgui_widgets.cpp', 'deps/imgui-1.76/examples/imgui_impl_opengl3.cpp', 'deps/imgui-1.76/examples/imgui_impl_glfw.cpp']

backward = declare_dependency(include_directories: 'deps/backward-cpp')
backward_src = ['deps/backward-cpp/backward.cpp']

executable('main',
    ['src/fmod.cpp', 'src/main.cpp', 'src/terrain_renderer.cpp', 'src/camera.cpp', 'src/util.cpp', 'src/loader.cpp', 'src/window.cpp', 'src/gl/context.cpp', 'src/sim.cpp', 'src/app.cpp', 'src/mesh_renderer.cpp', 'src/network.cpp', 'src/client.cpp', 'src/server.cpp', 'src/te/classic_ui.cpp', 'src/te/canvas_renderer.cpp', 'src/image.cpp', 'src/ft/ft.cpp', 'src/ft/face.cpp', 'src/hb/buffer.cpp', 'src/hb/font.cpp', 'src/ibus/bus.cpp', glad_src, backward_src],
    dependencies: [glfw3, glad, freeimage, fmod, boost, threads, fmt, fxgltf, entt, networking, spdlog, freetype, harfbuzz, backward, ibus, guile],
    include_directories: 'include',
    cpp_args: ['-fcoroutines', '-DGLFW_INCLUDE_NONE', '-DGLM_ENABLE_EXPERIMENTAL', '-DImTextureID=unsigned', networking_flags, '-DSCM_DEBUG_TYPING_STRICTNESS=2'],
    link_args: ['-ldl', '-static-libstdc++']
)
