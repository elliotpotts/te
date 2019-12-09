#ifndef TE_INSTANCE_RENDERER_HPP_INCLUDED
#define TE_INSTANCE_RENDERER_HPP_INCLUDED
#include <te/gl.hpp>
#include <te/camera.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace te {
    struct instanced_primitive {
        std::vector<gl::buffer<GL_ARRAY_BUFFER>> attribute_buffers;
        gl::buffer<GL_ELEMENT_ARRAY_BUFFER> element_buffer;
        gl::buffer<GL_ARRAY_BUFFER> instance_attribute_buffer;
        GLuint vao;
        GLenum element_mode;
        GLenum element_type;
        unsigned element_count;
        unsigned element_offset;
        gl::texture2d texture;
        gl::sampler sampler;
    };

    class instance_renderer {
        static const std::vector<std::pair<gl::string, GLuint>> attribute_locations;
        gl::context& gl;
        gl::program program;
        GLint view;
        GLint proj;
        GLint model;
        GLint sampler;
    public:
        instance_renderer(gl::context&);
        instanced_primitive load_from_file(std::string filename, int howmany);
        void draw(instanced_primitive& prim, const glm::mat4& model, const te::camera& cam);
    };
}
#endif
