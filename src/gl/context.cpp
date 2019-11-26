#include <te/gl.hpp>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <array>
#include <FreeImage.h>

void opengl_error_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam) {
    spdlog::error("opengl error: {}", message);
}

te::gl::context::context() {
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        throw std::runtime_error("Could not load opengl extensions");
    }
    spdlog::info("Loaded opengl extensions");
    glDebugMessageCallback(opengl_error_callback, nullptr);
}

void te::gl::shader_deleter::operator()(GLuint shader) {
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

void te::gl::program_deleter::operator()(GLuint program) {
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

GLint te::gl::program::attribute(const char* name) const {
    glGetError();
    GLint location = glGetAttribLocation(*hnd, name);
    GLenum error = glGetError();
    if (error == GL_INVALID_OPERATION) {
        throw std::runtime_error("This program is not a program! (this shouldn't happen)");
    } else if (location == -1) {
        throw std::runtime_error(fmt::format("The attribute \"{}\" is not an active attribute or is a reserved name", name));
    } else {
        return location;
    }
}

te::gl::program te::gl::context::link(const te::gl::shader& vertex, const te::gl::shader& fragment) {
    program_hnd program {glCreateProgram()};
    if (*program == 0) {
        program.release();
        throw std::runtime_error(fmt::format("glCreateProgram failed. Reason: {}", glGetError()));
    }
    //TODO: check that glCreateProgram was successful
    glAttachShader(*program, *vertex.hnd);
    glAttachShader(*program, *fragment.hnd);
    glBindFragDataLocation(*program, 0, "out_colour");
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

GLuint te::gl::image_texture(std::string filename) {
    FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(filename.c_str());
    if(fmt == FIF_UNKNOWN) {
        throw std::runtime_error(fmt::format("Couldn't determine image file format from filename: {}", filename));
    }
    FIBITMAP* bitmap = FreeImage_Load(fmt, filename.c_str());
    if(!bitmap) {
        throw std::runtime_error("Couldn't load image.");
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB,
        FreeImage_GetWidth(bitmap), FreeImage_GetHeight(bitmap),
        0, GL_BGR, GL_UNSIGNED_BYTE, FreeImage_GetBits(bitmap));
    FreeImage_Unload(bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

