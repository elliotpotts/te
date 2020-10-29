project('te', 'cpp', 'c', default_options: ['cpp_std=c++2a'])

cpp = meson.get_compiler('cpp')

threads = dependency('threads')
glfw3 = dependency('glfw3', version: '>=3.3')
glad = declare_dependency(include_directories: 'deps/glad/include')
glad_src = ['deps/glad/src/glad.c']
freeimage = dependency('freeimage')
fmod = declare_dependency (
  include_directories: 'deps/fmodstudioapi20007linux/api/core/inc/',
  dependencies: cpp.find_library('fmod', dirs: '/home/elliot/projects/te/deps/fmodstudioapi20007linux/api/core/lib/x86_64')
)
boost = dependency('boost', version: '>=1.73') #signals need not be included as it's header-only.
fmt = dependency('fmt')
fxgltf = declare_dependency(include_directories: 'deps/fx-gltf/include')
entt = declare_dependency(include_directories: 'deps/entt/src')
networking = subproject('GameNetworkingSockets').get_variable('dep_GameNetworkingSockets_so')
networking_flags = ['-DSTEAMNETWORKINGSOCKETS_OPENSOURCE', '-DPOSIX', '-DLINUX']
freetype = dependency('freetype2')
harfbuzz = dependency('harfbuzz')
spdlog = dependency('spdlog')

imgui = declare_dependency(include_directories: 'deps/imgui-1.76')
imgui_src = ['deps/imgui-1.76/imgui.cpp', 'deps/imgui-1.76/imgui_demo.cpp', 'deps/imgui-1.76/imgui_draw.cpp', 'deps/imgui-1.76/imgui_widgets.cpp', 'deps/imgui-1.76/examples/imgui_impl_opengl3.cpp', 'deps/imgui-1.76/examples/imgui_impl_glfw.cpp']

backward = declare_dependency(include_directories: 'deps/backward-cpp')
backward_src = ['deps/backward-cpp/backward.cpp']

executable('main',
    ['src/fmod.cpp', 'src/main.cpp', 'src/terrain_renderer.cpp', 'src/camera.cpp', 'src/util.cpp', 'src/loader.cpp', 'src/window.cpp', 'src/gl/context.cpp', 'src/sim.cpp', 'src/app.cpp', 'src/mesh_renderer.cpp', 'src/network.cpp', 'src/client.cpp', 'src/server.cpp', 'src/ui_renderer.cpp', 'src/image.cpp', 'src/ft/ft.cpp', 'src/ft/face.cpp', 'src/hb/buffer.cpp', 'src/hb/font.cpp', glad_src, backward_src],
    dependencies: [glfw3, glad, freeimage, fmod, boost, threads, fmt, fxgltf, entt, networking, spdlog, freetype, harfbuzz, backward],
    include_directories: 'include',
    cpp_args: ['-fcoroutines', '-DGLFW_INCLUDE_NONE', '-DGLM_ENABLE_EXPERIMENTAL', '-DImTextureID=unsigned', networking_flags],
    link_args: ['-ldl']
)
