#ifndef TE_INSTANCE_RENDERER_HPP_INCLUDED
#define TE_INSTANCE_RENDERER_HPP_INCLUDED
#include <te/gl.hpp>
#include <te/camera.hpp>
#include <te/mesh.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace te {
    struct instanced_primitive {
        primitive& instancee;
        gl::buffer<GL_ARRAY_BUFFER> instance_attribute_buffer;
        gl::vao vertex_array;
    };

    class instance_renderer {
        gl::context& gl;
        gl::program program;
        GLint view;
        GLint proj;
        GLint model;
        GLint sampler;
        std::unordered_map<primitive*, instanced_primitive> instances;
    public:
        instance_renderer(gl::context&);
        instanced_primitive& instance(primitive& instancee);
        void draw(instanced_primitive& prim, const glm::mat4& model, const te::camera& cam, int count);
    };
}
#endif
