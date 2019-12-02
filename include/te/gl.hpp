#ifndef TE_GL_HPP_INCLUDED
#define TE_GL_HPP_INCLUDED
#include <glad/glad.h>
#include <te/unique.hpp>
#include <type_traits>
#include <iterator>
#include <memory>
#include <vector>

struct FIBITMAP;

namespace te::gl {
    using string = std::basic_string<GLchar>;
    
    struct shader_deleter {
        void operator()(GLuint) const;
    };
    using shader_hnd = unique<GLuint, shader_deleter>;
    struct shader {
        shader_hnd hnd;
        explicit shader(shader_hnd shader);
    };

    struct program_deleter {
        void operator()(GLuint) const;
    };
    using program_hnd = unique<GLuint, program_deleter>;
    struct program {
        program_hnd hnd;
        explicit program(program_hnd program);
        GLint uniform(const char* name) const;
        std::optional<GLint> find_attribute(const char* name) const;
    };

    struct buffer_deleter {
        void operator()(GLuint) const;
    };
    using buffer_hnd = unique<GLuint, buffer_deleter>;
    template<GLenum target>
    struct buffer {
        buffer_hnd hnd;
        explicit buffer(buffer_hnd buffer) : hnd(std::move(buffer)) {
        }
        void bind() const {
            glBindBuffer(target, *hnd);
            if (glGetError() == GL_INVALID_OPERATION) {
                throw std::runtime_error("Invalid bind!\n");
            }
        }
    };

    struct sampler_deleter {
        void operator()(GLuint) const;
    };
    using sampler_hnd = unique<GLuint, sampler_deleter>;
    struct sampler {
        sampler_hnd hnd;
        explicit sampler(sampler_hnd sampler);
        void bind(GLuint texture_unit) const;
        void set(GLenum param, GLenum value);
    };

    struct texture_deleter {
        void operator()(GLuint) const;
    };
    using texture_hnd = unique<GLuint, texture_deleter>;
    template<GLenum target>
    struct texture {
        texture_hnd hnd;
        explicit texture(texture_hnd texture) : hnd(std::move(texture)) {
        }
        void bind() const {
            glBindTexture(target, *hnd);
        }
        void activate(GLuint texture_unit) const {
            glActiveTexture(GL_TEXTURE0 + texture_unit);
            bind();
        }
    };
    using texture2d = texture<GL_TEXTURE_2D>;

    struct framebuffer_deleter {
        void operator()(GLuint) const;
    };
    using framebuffer_hnd = unique<GLuint, framebuffer_deleter>;
    struct framebuffer {
        framebuffer_hnd hnd;
        explicit framebuffer(framebuffer_hnd fbo);
        void bind() const;
    };

    struct renderbuffer_deleter {
        void operator()(GLuint) const;
    };
    using renderbuffer_hnd = unique<GLuint, framebuffer_deleter>;
    struct renderbuffer {
        renderbuffer_hnd hnd;
        explicit renderbuffer(renderbuffer_hnd hnd);
        void bind() const;
    };

    using attribute_locations = std::vector<std::pair<string, GLuint>>;
    const attribute_locations common_attribute_locations = {
        {"POSITION", 0},
        {"NORMAL", 1},
        {"TANGENT", 2},
        {"TEXCOORD_0", 3}
    };
    
    struct context {
        shader compile(std::string source, GLenum type);
        program link(const shader&, const shader&, const std::vector<std::pair<string, GLuint>>& locations = {});
        texture<GL_TEXTURE_2D> make_texture(FIBITMAP* bitmap);
        texture<GL_TEXTURE_2D> make_texture(std::string filename);
        texture<GL_TEXTURE_2D> make_texture(const unsigned char* begin, const unsigned char* end);
        sampler make_sampler();
        framebuffer make_framebuffer();
        renderbuffer make_renderbuffer(int w, int h);
        void attach(renderbuffer& rbuf, framebuffer& fbuf);
        template<typename T, typename F>
        T make_hnd(F&& generate) {
            GLuint hnd;
            generate(1, &hnd);
            //TODO: typecheck 
            return T{hnd};
        }
        template<GLenum target, typename It>
        buffer<target> make_buffer(It begin, It end, GLenum usage_hint = GL_STATIC_DRAW) {
            GLuint vbo;
            glGenBuffers(1, &vbo);
            buffer<target> buffer {buffer_hnd{vbo}};
            buffer.bind();
            glBufferData(target, reinterpret_cast<const char*>(std::to_address(end)) - reinterpret_cast<const char*>(std::to_address(begin)), std::to_address(begin), usage_hint);
            return buffer;
        }
        explicit context();
        void toggle_perf_warnings(bool enabled);
    };
}

#endif
