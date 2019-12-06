#ifndef TE_MESH_RENDERER_HPP_INCLUDED
#define TE_MESH_RENDERER_HPP_INCLUDED
#include <te/gl.hpp>
#include <te/camera.hpp>
#include <string>
#include <vector>
#include <glad/glad.h>
#include <unordered_map>

namespace te {
    struct primitive {
        GLuint vao;
        GLenum mode;
        GLenum type;
        unsigned count;
        unsigned offset;
        gl::texture2d* texture;
        gl::sampler* sampler;
    };
    
    struct mesh {
        std::vector<gl::buffer<GL_ARRAY_BUFFER>> attribute_buffers;
        std::vector<gl::buffer<GL_ELEMENT_ARRAY_BUFFER>> element_buffers;
        std::vector<gl::texture2d> textures;
        std::vector<gl::sampler> samplers;
        std::vector<primitive> primitives;
    };

    class mesh_renderer {
        gl::context& gl;
        gl::program program;
        GLint model_uniform;
        GLint view_uniform;
        GLint proj_uniform;
        GLint tint_uniform;
        GLint sampler_uniform;
    public:
        static const std::vector<std::pair<gl::string, GLuint>> attribute_locations;
        mesh_renderer(gl::context&);
        mesh load_from_file(std::string filename);
        void draw(mesh& themesh, const glm::mat4& model, const te::camera& cam, glm::vec4 tint_colour = glm::vec4 { 0.0f });
    };
}
#endif
