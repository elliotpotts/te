#ifndef TE_GL_HPP_INCLUDED
#define TE_GL_HPP_INCLUDED
#include <glad/glad.h>
#include <te/unique.hpp>
#include <type_traits>
#include <iterator>
#include <memory>
namespace te::gl {
    using string = std::basic_string<GLchar>;
    
    struct shader_deleter {
        void operator()(GLuint);
    };
    using shader_hnd = unique<GLuint, shader_deleter>;
    struct shader {
        shader_hnd hnd;
        shader(shader_hnd shader);
    };

    struct program_deleter {
        void operator()(GLuint);
    };
    using program_hnd = unique<GLuint, program_deleter>;
    struct program {
        program_hnd hnd;
        program(program_hnd program);
        GLint uniform(const char* name) const;
        GLint attribute(const char* name) const;
    };
    
    template<GLenum type>
    struct buffer {
        GLuint hnd;
        buffer() {
            glGenBuffers(1, &hnd);
        }
        explicit buffer(GLuint buffer) : hnd(buffer) {
        }
        void bind() const {
            glBindBuffer(type, hnd);
            if (glGetError() == GL_INVALID_OPERATION) {
                throw std::runtime_error("Invalid bind!\n");
            }
        }
    };
    

    template<typename It, GLenum type>
    void copy(It begin, It end, buffer<type>& to, GLenum hint) {
        glBufferData(type, std::distance(begin, end) * sizeof(decltype(*begin)), begin, hint);
    }

    template<typename It>
    GLuint make_buffer(It begin, It end, GLenum target, GLenum usage_hint = GL_STATIC_DRAW) {
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(target, vbo);
        glBufferData(target, reinterpret_cast<char*>(std::to_address(end)) - reinterpret_cast<char*>(std::to_address(begin)), std::to_address(begin), usage_hint);
        return vbo;
    }

    GLuint image_texture(std::string filename);
    
    struct context {
        shader compile(std::string source, GLenum type);
        program link(const shader&, const shader&);
        context();
    };
}

#endif
