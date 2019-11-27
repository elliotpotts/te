#ifndef TE_MESH_RENDERER_HPP_INCLUDED
#define TE_MESH_RENDERER_HPP_INCLUDED
#include <te/gl.hpp>
#include <te/camera.hpp>
#include <string>
#include <vector>
#include <glad/glad.h>
namespace te {
    struct primitive {
        GLuint vao;
        GLenum mode;
        GLenum type;
        unsigned count;
        unsigned offset;
        gl::texture<GL_TEXTURE_2D>* texture;
        gl::sampler* sampler;
    };
    struct mesh {
        std::vector<gl::buffer<GL_ARRAY_BUFFER>> attribute_buffers;
        std::vector<gl::buffer<GL_ELEMENT_ARRAY_BUFFER>> element_buffers;
        std::vector<gl::texture<GL_TEXTURE_2D>> textures;
        std::vector<gl::sampler> samplers;
        std::vector<primitive> primitives;
    };
    mesh load_mesh(gl::context& gl, std::string filename, gl::program& prog);
    struct mesh_renderer {
        gl::context& gl;
        gl::program program;
        GLint model_uniform;
        GLint view_uniform;
        GLint proj_uniform;
        GLint sampler_uniform;
        mesh the_mesh;
        
        mesh_renderer(gl::context&, std::string filename);
        void draw(const te::camera& cam);
    };
}
#endif
