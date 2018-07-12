project('te', 'cpp', 'c')

includes = include_directories('deps/glad/include')

threads = dependency('threads')
glfw3 = dependency('glfw3', fallback: ['glfw-3.2.1', 'dep'])
freeimage = dependency('freeimage')
boost = dependency('boost', modules: ['log'])

executable('main',
    ['main.cpp', 'deps/glad/src/glad.c'],
    dependencies: [glfw3, freeimage, boost, threads],
    include_directories: includes
)
