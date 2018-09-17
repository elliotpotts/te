#include <te/opengl.hpp>
#include <array>
#include <FreeImage.h>
#include <sstream>

GLuint te::gl::compile_shader(std::string source, GLenum type) {
    GLuint const shader = glCreateShader(type);
    std::array<GLchar const* const, 1> const sources = { source.c_str() };
    std::array<GLint const, 1> const lengths = { static_cast<GLint>(source.size()) };
    glShaderSource(shader, 1, sources.data(), lengths.data());
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	te::gl::string log(' ', log_length);
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());
        std::ostringstream sstr;
        switch(type) {
            case GL_VERTEX_SHADER: sstr << "Vertex "; break;
            case GL_FRAGMENT_SHADER: sstr << "Fragment "; break;
            default: sstr << "Unkown " ;
        }
        sstr << "shader compilation failed because: " << log;
        throw std::runtime_error(sstr.str());
    } else {
        return shader;
    }
}

GLuint te::gl::link_program(GLuint vertex, GLuint fragment) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindFragDataLocation(program, 0, "out_colour");
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(status != GL_TRUE) {
        GLint log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
	te::gl::string log(' ', log_length);
        glGetProgramInfoLog(program, log_length, nullptr, log.data());
        std::ostringstream sstr;
        sstr << "Program linking failed because: " << log;
        throw std::runtime_error(sstr.str());
    } else {
        return program;
    }
}

GLuint te::gl::image_texture(std::string filename) {
    FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(filename.c_str());
    if(fmt == FIF_UNKNOWN) {
        std::ostringstream sstr;
        sstr << "The FREE_IMAGE_FORMAT of " << filename << " could not be ascertained so it cannot be loaded!";
        throw std::runtime_error(sstr.str());
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
