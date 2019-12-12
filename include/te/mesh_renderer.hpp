#ifndef TE_MESH_RENDERER_HPP_INCLUDED
#define TE_MESH_RENDERER_HPP_INCLUDED
#include <te/gl.hpp>
#include <te/camera.hpp>
#include <te/mesh.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace te {
    class mesh_renderer {
        struct instanced {
            te::primitive& primitive;
            gl::buffer<GL_ARRAY_BUFFER> instance_attribute_buffer;
            gl::vao vertex_array;
        };
        gl::context& gl;
        gl::program program;
        GLint view;
        GLint proj;
        GLint model;
        GLint sampler;
        std::unordered_map<primitive*, instanced> instances;
    public:
        struct instance_attributes {
            glm::vec2 offset;
            glm::vec3 tint;
        };
        mesh_renderer(gl::context&);
        instanced& instance(te::primitive& primitive);
        void draw(instanced& prim, const glm::mat4& model, const te::camera& cam, int count);
    };
}
#endif
