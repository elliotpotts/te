#ifndef TE_TERRAIN_RENDERER_HPP_INCLUDED
#define TE_TERRAIN_RENDERER_HPP_INCLUDED

#include <glad/glad.h>
#include <te/camera.hpp>
#include <te/gl.hpp>
#include <random>

namespace te {
    class terrain_renderer {
        gl::context& gl;
        int width;
        int height;
        gl::buffer<GL_ARRAY_BUFFER> vbo;
        gl::program program;
        GLint model_uniform;
        GLint view_uniform;
        GLint proj_uniform;
        gl::sampler sampler;
        gl::texture<GL_TEXTURE_2D> texture;
        GLuint vao;
    public:
        terrain_renderer(gl::context& gl, std::mt19937& rengine, int width, int height);
        void render(const te::camera& cam);
    };
}
#endif
