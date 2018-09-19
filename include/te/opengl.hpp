#ifndef TE_OPENGL_HPP_INCLUDED
#define TE_OPENGL_HPP_INCLUDED
#include <string>
#include <glad/glad.h>
#include <array>
#include <vector>
namespace te::gl {
  using string = std::basic_string<GLchar>;
  
  GLuint compile_shader(std::string source, GLenum type);
  GLuint link_program(GLuint vertex, GLuint fragment);
  GLuint image_texture(std::string filename);

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
}

#endif /* TE_OPENGL_HPP_INCLUDED */
