project('te', 'cpp', 'c', default_options: ['cpp_std=c++17'])

includes = include_directories('glad/include')

threads = dependency('threads')
glfw3 = dependency('glfw3')
freeimage = dependency('freeimage')
boost = dependency('boost', modules: ['log'])

executable('main',
    ['main.cpp', 'glad/src/glad.c'],
    dependencies: [glfw3, freeimage, boost, threads],
    include_directories: includes
)
