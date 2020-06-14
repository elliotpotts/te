#ifndef TE_GL_HPP_INCLUDED
#define TE_GL_HPP_INCLUDED
#include <glad/glad.h>
#include <te/unique.hpp>
#include <type_traits>
#include <iterator>
#include <memory>
#include <vector>
#include <te/image.hpp>
#include <ft/face.hpp>

namespace te {
    struct input_description;
}

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

    enum common_attribute_locations : GLuint{
        POSITION,
        NORMAL,
        TANGENT,
        TEXCOORD_0,
        INSTANCE_OFFSET,
        INSTANCE_COLOUR,
        COLOUR
    };
    inline const std::vector<std::pair<te::gl::string, GLuint>> common_attribute_names = {
        {"POSITION", POSITION},
        {"NORMAL", NORMAL},
        {"TANGENT", TANGENT},
        {"TEXCOORD_0", TEXCOORD_0},
        {"INSTANCE_OFFSET", INSTANCE_OFFSET},
        {"INSTANCE_COLOUR", INSTANCE_COLOUR},
        {"COLOUR", COLOUR}
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
        template <typename It>
        void upload(const It begin, const It end, GLenum hint = GL_STATIC_READ) {
            glBufferData (
                GL_ARRAY_BUFFER,
                std::distance(begin, end) * sizeof(typename std::iterator_traits<It>::value_type),
                std::to_address(begin),
                GL_STATIC_READ
            );
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

    struct vao_deleter {
        void operator()(GLuint) const;
    };
    using vao_hnd = unique<GLuint, vao_deleter>;
    struct vao {
        vao_hnd hnd;
        explicit vao(vao_hnd hnd);
        void bind() const;
    };
    
    struct context {
        shader compile(std::string source, GLenum type);
        program link(const shader&, const shader&, const std::vector<std::pair<string, GLuint>>& locations = {});
        texture2d make_texture(unique_bitmap bitmap);
        texture2d make_texture(std::string filename);
        texture2d make_texture(const unsigned char* begin, const unsigned char* end);
        texture2d make_texture(FT_GlyphSlotRec glyph);
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
        template<GLenum target>
        buffer<target> make_buffer() {
            buffer<target> buffer { make_hnd<buffer_hnd>(glGenBuffers) };
            buffer.bind();
            return buffer;
        }
        template<GLenum target, typename It>
        buffer<target> make_buffer(It begin, It end, GLenum usage_hint = GL_STATIC_DRAW) {
            auto buffer = make_buffer<target>();
            glBufferData(target, reinterpret_cast<const char*>(std::to_address(end)) - reinterpret_cast<const char*>(std::to_address(begin)), std::to_address(begin), usage_hint);
            return buffer;
        }
        vao make_vertex_array(const te::input_description& inputs);
        explicit context();
        void toggle_perf_warnings(bool enabled);
    };
}

#endif
