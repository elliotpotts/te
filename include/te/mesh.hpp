#ifndef TE_MESH_HPP_INCLUDED
#define TE_MESH_HPP_INCLUDED
#include <te/gl.hpp>
#include <list>
#include <functional>
namespace te {
    struct attribute_source {
        gl::buffer<GL_ARRAY_BUFFER>& source;
        GLuint location;
        GLint size;
        GLenum type;
        GLboolean normalized;
        GLsizei stride;
        const void* offset;
    };
    struct input_description {
        std::list<attribute_source> attributes;
        gl::buffer<GL_ELEMENT_ARRAY_BUFFER>* elements;
    };
    struct texture_unit_binding {
        gl::texture2d* texture;
        gl::sampler* sampler;
    };
    struct primitive {
        te::input_description inputs;
        GLenum mode;
        GLenum element_type;
        unsigned element_count;
        unsigned element_offset;
        std::list<texture_unit_binding> texture_unit_bindings;
    };
    struct mesh {
        std::list<std::reference_wrapper<primitive>> primitives;
    };
    struct gltf {
        std::list<gl::buffer<GL_ARRAY_BUFFER>> attribute_buffers;
        std::list<gl::buffer<GL_ELEMENT_ARRAY_BUFFER>> element_buffers;
        std::list<gl::texture2d> textures;
        std::list<gl::sampler> samplers;
        std::list<primitive> primitives;
        std::list<mesh> meshes;
    };
}
#endif
