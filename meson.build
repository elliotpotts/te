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

executable('main',
    ['src/main.cpp', 'src/terrain_renderer.cpp', 'src/camera.cpp', 'src/util.cpp', 'glad/src/glad.c', 'src/mesh_renderer.cpp', 'src/loader.cpp', 'src/window.cpp', 'src/gl/context.cpp', 'src/colour_picker.cpp'],
    dependencies: [glfw3, glad, freeimage, boost, threads, fmt, fxgltf, entt, spdlog],
    include_directories: 'include',
    cpp_args: ['-DGLFW_INCLUDE_NONE', '-DGLM_ENABLE_EXPERIMENTAL'],
    link_args: ['-ldl']
)
