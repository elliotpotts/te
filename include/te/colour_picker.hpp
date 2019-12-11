#ifndef TE_COLOUR_PICKER_HPP_INCLUDED
#define TE_COLOUR_PICKER_HPP_INCLUDED
#include <te/gl.hpp>
#include <te/window.hpp>
#include <te/camera.hpp>
#include <te/mesh.hpp>
#include <glm/glm.hpp>
namespace te {
    struct colour_picker {
        struct instance_attrs {
            glm::vec2 offset;
            glm::vec3 pick_colour;
        };
        struct instanced {
            te::primitive& primitive;
            gl::buffer<GL_ARRAY_BUFFER> instance_attribute_buffer;
            gl::vao vertex_array;
        };
        te::window& win;
        gl::program program;
        GLint model_uniform;
        GLint view_uniform;
        GLint proj_uniform;
        gl::framebuffer colour_fbuffer;
        gl::renderbuffer attachment;
        std::unordered_map<primitive*, instanced> instances;
    public:
        colour_picker(te::window& win);
        instanced& instance(te::primitive& primitive);
        void draw(instanced& instanced, const glm::mat4& model_mat, const te::camera& cam, int count);
    };
}
#endif
