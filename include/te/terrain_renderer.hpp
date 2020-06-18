#ifndef TE_TERRAIN_RENDERER_HPP_INCLUDED
#define TE_TERRAIN_RENDERER_HPP_INCLUDED

#include <glad/glad.h>
#include <te/camera.hpp>
#include <te/gl.hpp>
#include <random>
#include <glm/glm.hpp>

namespace te {
    class terrain_renderer {
        gl::context& gl;
        gl::buffer<GL_ARRAY_BUFFER> vbo;
        gl::program program;
        GLint model_uniform;
        GLint view_uniform;
        GLint proj_uniform;
        gl::sampler sampler;
        gl::texture2d texture;
        GLuint vao;
    public:
        const int width;
        const int height;
        const glm::vec3 grid_topleft;
        terrain_renderer(gl::context& gl, std::default_random_engine& rengine, int width, int height);
        void render(const te::camera& cam);
    };
}
#endif
