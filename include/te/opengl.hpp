#ifndef TE_OPENGL_HPP_INCLUDED
#define TE_OPENGL_HPP_INCLUDED
#include <string>
#include <glad/glad.h>
#include <array>
#include <vector>

#include <optional>
namespace te {
    // Unique reference
    // T = identity value representing object to which we only want one "reference" to
    template<typename T, typename Deleter>
    struct unique {
        std::optional<T> storage;
        unique(unique&& other) : storage(std::move(other.storage)) {
            other.storage.reset();
        }
        explicit unique(T hnd) : storage(hnd) {
        }
        unique(const unique&) = delete;
        const T& operator*() const {
            return *storage;
        }
        ~unique() {
            if (storage) {
                Deleter {}(storage.value());
            }
        }
    };
}
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
    shader compile(std::string source, GLenum type);

    template<typename T>
    struct uniform {
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
    program link(const shader&, const shader&);

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

    template<typename T>
    GLuint make_buffer(T const * const data, std::size_t size, GLenum type) {
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(type, vbo);
        glBufferData(type, size * sizeof(T), data, GL_STATIC_DRAW);
        return vbo;
    }
  
    template<typename T, size_t N>
    GLuint make_buffer(const std::array<T, N> data, GLenum type) {
        return make_buffer(data.data(), data.size(), type);
    }

    template<typename T>
    GLuint make_buffer(const std::vector<T> data, GLenum type) {
        return make_buffer(data.data(), data.size(), type);
    }

    GLuint image_texture(std::string filename);
}

#endif /* TE_OPENGL_HPP_INCLUDED */
