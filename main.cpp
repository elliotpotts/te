#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <array>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <vector>
#include <FreeImage.h>
#include <tuple>
#include <random>
#define  BOOST_LOG_DYN_LINK
#include <boost/log/trivial.hpp>

int const win_w = 1024;
int const win_h = 768;

using gl_string = std::basic_string<GLchar>;

GLuint compile_shader(std::string source, GLenum type) {
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
        gl_string log(' ', log_length);
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

GLuint link_program(GLuint vertex, GLuint fragment) {
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
        gl_string log(' ', log_length);
        glGetProgramInfoLog(program, log_length, nullptr, log.data());
        std::ostringstream sstr;
        sstr << "Program linking failed because: " << log;
        throw std::runtime_error(sstr.str());
    } else {
        return program;
    }
}

template<typename T, size_t N>
GLuint make_buffer(const std::array<T, N> data, GLenum type) {
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(type, vbo);
    glBufferData(type, data.size() * sizeof(T), data.data(), GL_STATIC_DRAW);
    return vbo;
}

template<typename T>
GLuint make_buffer(const std::vector<T> data, GLenum type) {
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(type, vbo);
    glBufferData(type, data.size() * sizeof(T), data.data(), GL_STATIC_DRAW);
    return vbo;
}

std::random_device seed_device;
std::mt19937 rengine;
std::uniform_int_distribution tile_select {0, 1};

GLuint grid(int width, int height) {
    struct vertex {
        glm::vec2 pos;
        glm::vec3 col;
        glm::vec2 uv;
    };
    static_assert(sizeof(vertex) == sizeof(GLfloat) * 7, "Platform doesn't support this directly.");
    std::vector<vertex> data;
    glm::vec2 grid_tl {-width/2.0f, -height/2.0f};
    glm::vec3 white {1.0f, 1.0f, 1.0f};
    for (int xi = 0; xi < width; xi++) {
        for (int yi = 0; yi < height; yi++) {
            glm::vec2 uv_tl {tile_select(rengine) * 0.5f, tile_select(rengine) * 0.5f};
            auto cell_tl_pos = grid_tl + static_cast<float>(xi) * glm::vec2{1.0f, 0.0f}
                                       + static_cast<float>(yi) * glm::vec2{0.0f, 1.0f};
            vertex cell_tl {cell_tl_pos + glm::vec2{0.0f, 0.0f}, white, uv_tl + glm::vec2(0.0f, 0.0f)};
            vertex cell_tr {cell_tl_pos + glm::vec2{1.0f, 0.0f}, white, uv_tl + glm::vec2(0.5f, 0.0f)};
            vertex cell_br {cell_tl_pos + glm::vec2{1.0f, 1.0f}, white, uv_tl + glm::vec2(0.5f, 0.5f)};
            vertex cell_bl {cell_tl_pos + glm::vec2{0.0f, 1.0f}, white, uv_tl + glm::vec2(0.0f, 0.5f)};
            data.push_back(cell_tl);
            data.push_back(cell_tr);
            data.push_back(cell_br);
            data.push_back(cell_br);
            data.push_back(cell_bl);
            data.push_back(cell_tl);
        }
    }
    return make_buffer(data, GL_ARRAY_BUFFER);
}

GLuint image_texture(std::string filename) {
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
        0, GL_RGB, GL_UNSIGNED_BYTE, FreeImage_GetBits(bitmap));
    FreeImage_Unload(bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

auto start_time = std::chrono::high_resolution_clock::now();

std::string const vshader_src = R"EOF(
#version 150 core
in vec2 position;
in vec3 colour;
in vec2 texcoord;
out vec3 v_colour;
out vec2 v_texcoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    v_colour = colour;
    v_texcoord = texcoord;
    gl_Position = projection * view * model * vec4(position, 0.0, 1.0);
} )EOF";

std::string const fshader_src = R"EOF(
#version 150 core
in vec3 v_colour;
in vec2 v_texcoord;
out vec4 out_colour;
uniform sampler2D tex;
void main() {
    out_colour = texture(tex, v_texcoord) * vec4(v_colour, 1.0f);
} )EOF";

class game {
    GLFWwindow* w;
    GLuint vao;
    GLuint vbo;
    GLuint program;
    GLint model_uniform;
    GLint view_uniform;
    glm::vec3 cam_focus;
    glm::vec3 cam_offset = {-48.0f, -48.0f, 48.0f};
    GLint proj_uniform;

public:
    game(GLFWwindow* w) : w{w} {
        program = link_program(
            compile_shader(vshader_src, GL_VERTEX_SHADER),
            compile_shader(fshader_src, GL_FRAGMENT_SHADER)
        );
        glUseProgram(program);
        
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        vbo = grid(5,5);

        GLint pos_attrib = glGetAttribLocation(program, "position");
        glEnableVertexAttribArray(pos_attrib);
        glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 7*sizeof(float), reinterpret_cast<void*>(0));

        GLint col_attrib = glGetAttribLocation(program, "colour");
        glEnableVertexAttribArray(col_attrib);
        glVertexAttribPointer(col_attrib, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), reinterpret_cast<void*>(2*sizeof(float)));

        GLint tex_attrib = glGetAttribLocation(program, "texcoord");
        glEnableVertexAttribArray(tex_attrib);
        glVertexAttribPointer(tex_attrib, 2, GL_FLOAT, GL_FALSE, 7*sizeof(float), reinterpret_cast<void*>(5*sizeof(float)));

        image_texture("media/grass_tiles.png");

        model_uniform = glGetUniformLocation(program, "model");
        view_uniform = glGetUniformLocation(program, "view");
        proj_uniform = glGetUniformLocation(program, "projection");
    }

    void handle_key(int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam_offset = glm::rotate(cam_offset, glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
        if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam_offset = glm::rotate(cam_offset, -glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
    }

    void operator()() {
        if(glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) { glfwSetWindowShouldClose(w, true); };
        glClear(GL_COLOR_BUFFER_BIT);

        std::chrono::duration<float> secs = start_time - std::chrono::high_resolution_clock::now();

        glm::mat4 model { 1 };
        glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));

        glm::vec3 forward = -cam_offset;
        forward.z = 0.0f;
        forward = glm::normalize(forward);
        glm::vec3 right = glm::rotate(forward, glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
        if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) cam_focus += 0.002f * forward;
        if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) cam_focus -= 0.002f * right;
        if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) cam_focus -= 0.002f * forward;
        if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) cam_focus += 0.002f * right;
        glm::mat4 view = glm::lookAt(
            cam_focus - cam_offset,
            cam_focus,
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        glUniformMatrix4fv(view_uniform, 1, GL_FALSE, glm::value_ptr(view));

        glm::mat4 proj = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.00001f, 500.0f);
        glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(proj));

        glDrawArrays(GL_TRIANGLES, 0, 5*5*6);
    }
};

void glfw_error_callback(int error, const char* description) {
    BOOST_LOG_TRIVIAL(error) << "glfw3 Error[" << error << "]: " << description;
    std::abort();
}

void opengl_error_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam) {
    BOOST_LOG_TRIVIAL(error) << "opengl error: " << message;
}

void glfw_dispatch_key(GLFWwindow* w, int key, int scancode, int action, int mods) {
    reinterpret_cast<game*>(glfwGetWindowUserPointer(w))
    ->handle_key(key, scancode, action, mods);
}

int main(void) {
    if (!glfwInit()) {
        BOOST_LOG_TRIVIAL(fatal) << "Could not initialise glfw";
        return -1;
    } else {
        BOOST_LOG_TRIVIAL(info) << "Initialized glfw " << glfwGetVersionString();
    }

    glfwSetErrorCallback(glfw_error_callback);
    glfwWindowHint(GLFW_RESIZABLE , false);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    GLFWwindow* window = glfwCreateWindow(win_w, win_h, "Hello World", NULL, NULL);
    if (!window) {
        BOOST_LOG_TRIVIAL(fatal) << "Could not create opengl window";
        glfwTerminate();
        return -1;
    } else {
        BOOST_LOG_TRIVIAL(info) << "Created window with following attributes: ";
        switch(glfwGetWindowAttrib(window, GLFW_CLIENT_API)) {
            case GLFW_OPENGL_API:
                BOOST_LOG_TRIVIAL(info) << " |             GLFW_CLIENT_API: GLFW_OPENGL_API";
                break;
            case GLFW_OPENGL_ES_API:
                BOOST_LOG_TRIVIAL(info) << " |             GLFW_CLIENT_API: GLFW_OPENGL_ES_API";
                break;
            case GLFW_NO_API:
                BOOST_LOG_TRIVIAL(info) << " |             GLFW_CLIENT_API: GLFW_NO_API";
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_CONTEXT_CREATION_API)) {
            case GLFW_NATIVE_CONTEXT_API:
                BOOST_LOG_TRIVIAL(info) << " |   GLFW_CONTEXT_CREATION_API: GLFW_NATIVE_CONTEXT_API";
                break;
            case GLFW_EGL_CONTEXT_API:
                BOOST_LOG_TRIVIAL(info) << " |   GLFW_CONTEXT_CREATION_API: GLFW_EGL_CONTEXT_API";
                break;
        }
        BOOST_LOG_TRIVIAL(info) << " |  GLFW_CONTEXT_VERSION_MAJOR: " << glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
        BOOST_LOG_TRIVIAL(info) << " |  GLFW_CONTEXT_VERSION_MINOR: " << glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
        BOOST_LOG_TRIVIAL(info) << " |       GLFW_CONTEXT_REVISION: " << glfwGetWindowAttrib(window, GLFW_CONTEXT_REVISION);
        switch(glfwGetWindowAttrib(window, GLFW_OPENGL_FORWARD_COMPAT)) {
            case GLFW_TRUE:
                BOOST_LOG_TRIVIAL(info) << " |  GLFW_OPENGL_FORWARD_COMPAT: GLFW_TRUE";
                break;
            case GLFW_FALSE:
                BOOST_LOG_TRIVIAL(info) << " |  GLFW_OPENGL_FORWARD_COMPAT: GLFW_FALSE";
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_OPENGL_DEBUG_CONTEXT)) {
            case GLFW_TRUE:
                BOOST_LOG_TRIVIAL(info) << " |   GLFW_OPENGL_DEBUG_CONTEXT: GLFW_TRUE";
                break;
            case GLFW_FALSE:
                BOOST_LOG_TRIVIAL(info) << " |   GLFW_OPENGL_DEBUG_CONTEXT: GLFW_FALSE";
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_OPENGL_PROFILE)) {
            case GLFW_OPENGL_CORE_PROFILE:
                BOOST_LOG_TRIVIAL(info) << " |         GLFW_OPENGL_PROFILE: GLFW_OPENGL_CORE_PROFILE";
                break;
            case GLFW_OPENGL_COMPAT_PROFILE:
                BOOST_LOG_TRIVIAL(info) << " |         GLFW_OPENGL_PROFILE: GLFW_OPENGL_COMPAT_PROFILE";
                break;
            case GLFW_OPENGL_ANY_PROFILE:
                BOOST_LOG_TRIVIAL(info) << " |         GLFW_OPENGL_PROFILE: GLFW_OPENGL_ANY_PROFILE";
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_CONTEXT_ROBUSTNESS)) {
            case GLFW_LOSE_CONTEXT_ON_RESET:
                BOOST_LOG_TRIVIAL(info) << " |     GLFW_CONTEXT_ROBUSTNESS: GLFW_LOSE_CONTEXT_ON_RESET";
                break;
            case GLFW_NO_RESET_NOTIFICATION:
                BOOST_LOG_TRIVIAL(info) << " |     GLFW_CONTEXT_ROBUSTNESS: GLFW_NO_RESET_NOTIFICATION";
                break;
            case GLFW_NO_ROBUSTNESS:
                BOOST_LOG_TRIVIAL(info) << " |     GLFW_CONTEXT_ROBUSTNESS: GLFW_NO_ROBUSTNESS";
                break;
        }
    }

    glfwMakeContextCurrent(window);
    BOOST_LOG_TRIVIAL(info) << "Context is now current.";
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        BOOST_LOG_TRIVIAL(fatal) << "Could not load opengl extensions";
        return -1;
    } else {
        BOOST_LOG_TRIVIAL(info) << "Loaded opengl extensions";
    }
    glDebugMessageCallback(opengl_error_callback, nullptr);
    game g(window);
    glfwSetWindowUserPointer(window, &g);
    glfwSetKeyCallback(window, glfw_dispatch_key);
    while (!glfwWindowShouldClose(window)) {
        g();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}