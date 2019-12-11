project('te', 'cpp', 'c', default_options: ['cpp_std=c++2a'])

threads = dependency('threads')
glfw3 = dependency('glfw3', version: '>=3.3')
glad = declare_dependency(include_directories: 'glad/include')
freeimage = dependency('freeimage')
boost = dependency('boost', modules: ['log']) #signals need not be included as it's header-only.
fmt = dependency('fmt')
fxgltf = declare_dependency(include_directories: 'fx-gltf/include')
entt = declare_dependency(include_directories: 'entt/src')
spdlog = dependency('spdlog')

imgui = declare_dependency(include_directories: 'imgui-1.74')
imgui_src = ['imgui-1.74/imgui.cpp', 'imgui-1.74/imgui_demo.cpp', 'imgui-1.74/imgui_draw.cpp', 'imgui-1.74/imgui_widgets.cpp', 'imgui-1.74/examples/imgui_impl_opengl3.cpp', 'imgui-1.74/examples/imgui_impl_glfw.cpp']

executable('main',
    ['src/main.cpp', 'src/terrain_renderer.cpp', 'src/camera.cpp', 'src/util.cpp', 'glad/src/glad.c', 'src/loader.cpp', 'src/window.cpp', 'src/gl/context.cpp', 'src/sim.cpp', 'src/app.cpp', 'src/mesh_renderer.cpp', 'src/colour_picker.cpp', imgui_src],
    dependencies: [glfw3, glad, freeimage, boost, threads, fmt, fxgltf, entt, spdlog, imgui],
    include_directories: 'include',
    cpp_args: ['-DGLFW_INCLUDE_NONE', '-DGLM_ENABLE_EXPERIMENTAL', '-DImTextureID=unsigned'],
    link_args: ['-ldl']
)
