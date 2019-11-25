project('te', 'cpp', 'c', default_options: ['cpp_std=c++17'])

threads = dependency('threads')
glfw3 = dependency('glfw3', version: '>=3.3')
glad = declare_dependency(include_directories: 'glad/include')
freeimage = dependency('freeimage')
boost = dependency('boost', modules: ['log'])
fmt = dependency('fmt')
fxgltf = declare_dependency(include_directories: 'fx-gltf/include')
entt = declare_dependency(include_directories: 'entt/src')
spdlog = dependency('spdlog')

executable('main',
    ['src/main.cpp', 'src/opengl.cpp', 'src/terrain_renderer.cpp', 'src/camera.cpp', 'src/util.cpp', 'glad/src/glad.c'],
    dependencies: [glfw3, glad, freeimage, boost, threads, fmt, fxgltf, entt, spdlog],
    include_directories: 'include',
    cpp_args: ['-DGLFW_INCLUDE_NONE'],
    link_args: ['-ldl']
)
