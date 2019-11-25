#ifndef TE_MESH_RENDERER_HPP_INCLUDED
#define TE_MESH_RENDERER_HPP_INCLUDED
#include <string>
#include <te/opengl.hpp>
#include <te/camera.hpp>
#include <glad/glad.h>
namespace te {
    struct primitive {
        GLuint vao;
        GLenum mode;
        GLenum type;
        unsigned count;
        unsigned offset;
    };
    struct mesh {
        std::vector<gl::buffer<GL_ARRAY_BUFFER>> attribute_buffers;
        std::vector<gl::buffer<GL_ELEMENT_ARRAY_BUFFER>> element_buffers;
        std::vector<GLuint> textures;
        std::vector<primitive> primitives;
    };
    mesh load_mesh(std::string filename, gl::program& prog);
    struct mesh_renderer {
        gl::program program;
        GLint model_uniform;
        GLint view_uniform;
        GLint proj_uniform;
        GLuint texture;
        mesh the_mesh;
        
        mesh_renderer(std::string filename);
        void draw(const te::camera& cam);
    };
}
#endif
