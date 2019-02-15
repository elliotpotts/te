project('te', 'cpp', 'c', default_options: ['cpp_std=c++17'])

includes = include_directories('glad/include', 'include')

threads = dependency('threads')
glfw3 = dependency('glfw3', version: '>=3.3')
freeimage = dependency('freeimage')
boost = dependency('boost', modules: ['log'])
fmt = dependency('fmt')

executable('main',
    ['src/main.cpp', 'src/opengl.cpp', 'src/terrain_renderer.cpp', 'src/camera.cpp', 'src/util.cpp', 'glad/src/glad.c'],
    dependencies: [glfw3, freeimage, boost, threads, fmt],
    include_directories: includes,
    cpp_args: '-DGLFW_INCLUDE_NONE',
    link_args: '-ldl'
)
