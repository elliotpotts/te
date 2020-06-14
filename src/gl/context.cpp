#include <te/gl.hpp>
#include <te/mesh.hpp>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <fmt/format.h>
#include <cassert>
#include <spdlog/spdlog.h>
#include <array>
#include <te/image.hpp>

namespace {
    std::string debug_type_to_string(GLenum type) {
        switch(type) {
        case GL_DEBUG_TYPE_ERROR: return "error";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "deprecation warning";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "undefined behaviour";
        case GL_DEBUG_TYPE_PORTABILITY: return "implementation specific behaviour";
        case GL_DEBUG_TYPE_PERFORMANCE: return "performance issue";
        case GL_DEBUG_TYPE_MARKER: return "command stream annotation";
        case GL_DEBUG_TYPE_PUSH_GROUP: return "group pushing";
        case GL_DEBUG_TYPE_POP_GROUP: return "group popping";
        case GL_DEBUG_TYPE_OTHER: return "other issue";
        default: return "unknown issue";
        }
    }
    std::string debug_source_to_string(GLenum source) {
        switch (source) {
        case GL_DEBUG_SOURCE_API: return "GL";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "GLX";
        case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Compiler";
        case GL_DEBUG_SOURCE_THIRD_PARTY: return "3rd Party";
        case GL_DEBUG_SOURCE_APPLICATION: return "This";
        case GL_DEBUG_SOURCE_OTHER: return "Other";
            default: return "Unknown";
        }
    }
    std::string debug_severity_to_string(GLenum severity) {
        switch(severity) {
        case GL_DEBUG_SEVERITY_HIGH: return "high";
        case GL_DEBUG_SEVERITY_MEDIUM: return "medium";
        case GL_DEBUG_SEVERITY_LOW: return "low";
        case GL_DEBUG_SEVERITY_NOTIFICATION: return "nofication";
        default: return "Unknown";
        }
    }
}

void opengl_error_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam) {
    spdlog::debug (
        "{}: {} severity {}: {}",
        debug_source_to_string(source),
        debug_severity_to_string(type),
        debug_type_to_string(type),
        message
    );
}

te::gl::context::context() {
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        throw std::runtime_error("Could not load opengl extensions");
    }
    spdlog::info("Loaded opengl extensions");
    glDebugMessageCallback(opengl_error_callback, nullptr);
}

void te::gl::context::toggle_perf_warnings(bool enabled) {
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PERFORMANCE, GL_DONT_CARE, 0, nullptr, enabled);
}

void te::gl::shader_deleter::operator()(GLuint shader) const {
    glDeleteShader(shader);
}

te::gl::shader::shader(te::gl::shader_hnd shader): hnd(std::move(shader)) {
}

te::gl::shader te::gl::context::compile(std::string source, GLenum type) {
    shader_hnd shader {glCreateShader(type)};
    if (*shader == 0) {
        shader.release();
        throw std::runtime_error(fmt::format("glCreateShader failed. Reason: {}", glGetError()));
    }
    std::array<GLchar const* const, 1> const sources = { source.c_str() };
    std::array<GLint const, 1> const lengths = { static_cast<GLint>(source.size()) };
    glShaderSource(*shader, 1, sources.data(), lengths.data());
    glCompileShader(*shader);
    GLint status;
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE) {
        GLint log_length;
        glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &log_length);
	te::gl::string log(' ', log_length);
        glGetShaderInfoLog(*shader, log_length, nullptr, log.data());
        throw std::runtime_error(fmt::format("Shader compilation failed because: {}", log));
    } else {
        return te::gl::shader{std::move(shader)};
    }
}

void te::gl::program_deleter::operator()(GLuint program) const {
    if (!glIsProgram(program)) {
        throw std::runtime_error("This isn't a program!");
    }
    glDeleteProgram(program);
}

te::gl::program::program(program_hnd program): hnd(std::move(program)) {
}

GLint te::gl::program::uniform(const char* name) const {
    return glGetUniformLocation(*hnd, name);
}

std::optional<GLint> te::gl::program::find_attribute(const char* name) const {
    GLint location = glGetAttribLocation(*hnd, name);
    if (glGetError() == GL_INVALID_OPERATION) {
        throw std::runtime_error("The object represented by \"this\" is not a valid program");
    }
    if (location == -1) {
        return {};
    } else {
        return std::make_optional(location);
    }
}

void te::gl::buffer_deleter::operator()(GLuint buffer) const {
    glDeleteBuffers(1, &buffer);
}

te::gl::program te::gl::context::link(const te::gl::shader& vertex, const te::gl::shader& fragment, const std::vector<std::pair<std::string, GLuint>>& locations) {
    program_hnd program {glCreateProgram()};
    if (*program == 0) {
        program.release();
        throw std::runtime_error(fmt::format("glCreateProgram failed. Reason: {}", glGetError()));
    }
    glAttachShader(*program, *vertex.hnd);
    glAttachShader(*program, *fragment.hnd);
    //TODO: don't hardcode out_colour 
    glBindFragDataLocation(*program, 0, "out_colour");
    for (auto [attr_name, attr_location] : locations) {
        glBindAttribLocation(*program, attr_location, attr_name.c_str());
    }
    
    glLinkProgram(*program);
    GLint status;
    glGetProgramiv(*program, GL_LINK_STATUS, &status);
    if(status != GL_TRUE) {
        GLint log_length;
        glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &log_length);
	te::gl::string log(' ', log_length);
        glGetProgramInfoLog(*program, log_length, nullptr, log.data());
        throw std::runtime_error(fmt::format("Program linking failed because: {}", log));
    } else {
        return te::gl::program{std::move(program)};
    }
}

void te::gl::sampler_deleter::operator()(GLuint sampler) const {
    glDeleteSamplers(1, &sampler);
}

te::gl::sampler::sampler(sampler_hnd sampler) : hnd(std::move(sampler)) {
    glSamplerParameteri(*hnd, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(*hnd, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glSamplerParameteri(*hnd, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(*hnd, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void te::gl::sampler::bind(GLuint texture_unit) const {
    glBindSampler(texture_unit, *hnd);
}

void te::gl::sampler::set(GLenum param, GLenum value) {
    glSamplerParameteri(*hnd, param, value);
}

te::gl::sampler te::gl::context::make_sampler() {
    return te::gl::sampler { make_hnd<sampler_hnd>(glGenSamplers) };
}

void te::gl::texture_deleter::operator()(GLuint hnd) const {
    glDeleteTextures(1, &hnd);
}

//TODO: write  a texture function which can take raw bytes
te::gl::texture2d te::gl::context::make_texture(FT_GlyphSlotRec glyph) {
    assert(glyph.bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
    //copy to unpadded data
    // BGRA
    std::vector<std::array<unsigned char, 4>> pixels (glyph.bitmap.rows * glyph.bitmap.width);
    for (unsigned y = 0; y < glyph.bitmap.rows; y++) {
        for (unsigned x = 0; x < glyph.bitmap.width; x++) {
            double grey = glyph.bitmap.buffer[y * glyph.bitmap.pitch + x] / 255.0;
            pixels[y * glyph.bitmap.width + x] = std::array {
                static_cast<unsigned char>(grey * 255),// * 67),
                static_cast<unsigned char>(grey * 255),// * 194),
                static_cast<unsigned char>(grey * 255),// * 255),
                static_cast<unsigned char>(grey * 255)
            };
        }
    }

    te::gl::texture2d tex2d { make_hnd<te::gl::texture_hnd>(glGenTextures) };
    tex2d.bind();
    glTexImage2D (
        GL_TEXTURE_2D, 0, GL_RGBA,
        glyph.bitmap.width, glyph.bitmap.rows,
        0, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data()
    );
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex2d;
}

te::gl::texture<GL_TEXTURE_2D> te::gl::context::make_texture(unique_bitmap bmp) {
    auto width = FreeImage_GetWidth(bmp.get());
    auto height = FreeImage_GetHeight(bmp.get());
    auto pitch = FreeImage_GetPitch(bmp.get());
    auto rawbits = std::vector<unsigned char>(height * pitch);
    FreeImage_ConvertToRawBits(rawbits.data(), bmp.get(), pitch, 32, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, TRUE);
    te::gl::texture<GL_TEXTURE_2D> tex2d {make_hnd<te::gl::texture_hnd>(glGenTextures)};
    tex2d.bind();
    glTexImage2D (
        GL_TEXTURE_2D, 0, GL_RGBA,
        width, height,
        0, GL_BGRA, GL_UNSIGNED_BYTE, rawbits.data()
    );
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex2d;
}

te::gl::texture<GL_TEXTURE_2D> te::gl::context::make_texture(const unsigned char* begin, const unsigned char* end) {
    return make_texture(make_bitmap(begin, end));
}

te::gl::texture<GL_TEXTURE_2D> te::gl::context::make_texture(std::string filename) {
    return make_texture(make_bitmap(filename));
}

void te::gl::framebuffer_deleter::operator()(GLuint hnd) const {
    glDeleteFramebuffers(1, &hnd);
}

te::gl::framebuffer::framebuffer(framebuffer_hnd fbo) : hnd {std::move(fbo)} {
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        spdlog::error("fbo not complete");
    }
}

void te::gl::framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, *hnd);
}

te::gl::framebuffer te::gl::context::make_framebuffer() {
    te::gl::framebuffer_hnd fbo = make_hnd<framebuffer_hnd>(glGenFramebuffers);
    return te::gl::framebuffer{std::move(fbo)};
}

void te::gl::renderbuffer_deleter::operator()(GLuint hnd) const {
    glDeleteRenderbuffers(1, &hnd);
}

te::gl::renderbuffer::renderbuffer(renderbuffer_hnd old) : hnd {std::move(old)} {
}

void te::gl::renderbuffer::bind() const {
    glBindRenderbuffer(GL_RENDERBUFFER, *hnd);
}

te::gl::renderbuffer te::gl::context::make_renderbuffer(int w, int h) {
    renderbuffer buffer { make_hnd<renderbuffer_hnd>(glGenRenderbuffers) };
    buffer.bind();
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, w, h);
    return buffer;
}

void te::gl::context::attach(te::gl::renderbuffer& rbuf, te::gl::framebuffer& fbuf) {
    rbuf.bind();
    fbuf.bind();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, *rbuf.hnd);
}

void te::gl::vao_deleter::operator()(GLuint vao) const {
    glDeleteVertexArrays(1, &vao);
}
te::gl::vao::vao(vao_hnd hnd) : hnd(std::move(hnd)) {
}
void te::gl::vao::bind() const {
    glBindVertexArray(*hnd);
}
te::gl::vao te::gl::context::make_vertex_array(const te::input_description& inputs) {
    vao array { make_hnd<vao_hnd>(glGenVertexArrays) };
    array.bind();
    if (inputs.elements) inputs.elements->bind();
    for (auto& attribute_input : inputs.attributes) {
        attribute_input.source.bind();
        glEnableVertexAttribArray(attribute_input.location);
        glVertexAttribPointer (
            attribute_input.location,
            attribute_input.size,
            attribute_input.type,
            attribute_input.normalized,
            attribute_input.stride,
            attribute_input.offset
        );
    }
    return array;
}
