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
#include <tuple>
#include <random>
#define  BOOST_LOG_DYN_LINK
#include <boost/log/trivial.hpp>
#include <te/util.hpp>
#include <te/opengl.hpp>
#include <te/camera.hpp>
#include <te/terrain_renderer.hpp>
#include <fmt/core.h>
#include <fx/gltf.h>
#include <entt/entity/registry.hpp>

std::random_device seed_device;
std::mt19937 rengine;

namespace te {
    struct primitive {
        gl::buffer<GL_ELEMENT_ARRAY_BUFFER>& elements;
        GLuint vao;
        GLenum mode;
        GLenum type;
        unsigned count;
        unsigned offset;
    };
    struct mesh {
        std::vector<gl::buffer<GL_ARRAY_BUFFER>> attribute_buffers;
        std::vector<gl::buffer<GL_ELEMENT_ARRAY_BUFFER>> element_buffers;
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
GLint component_count(fx::gltf::Accessor::Type type) {
    switch (type) {
    case fx::gltf::Accessor::Type::None: return 0;
    case fx::gltf::Accessor::Type::Scalar: return 1;
    case fx::gltf::Accessor::Type::Vec2: return 2;
    case fx::gltf::Accessor::Type::Vec3: return 3;
    case fx::gltf::Accessor::Type::Vec4: return 4;
    case fx::gltf::Accessor::Type::Mat2: return 4;
    case fx::gltf::Accessor::Type::Mat3: return 9;
    case fx::gltf::Accessor::Type::Mat4: return 16;
    default:
        throw std::runtime_error("Unrecognised type");
    }
}
#include <list>
te::mesh te::load_mesh(std::string filename, gl::program& program) {
    te::mesh out;
    const fx::gltf::Document doc = fx::gltf::LoadFromBinary(filename);
    // we're only going to load the first mesh.
    const fx::gltf::Mesh& docmesh = doc.meshes[0];
    for (const fx::gltf::Primitive& primitive : docmesh.primitives) {
        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        // Keep track of which buffers are already loaded
        std::list<std::pair<int, te::gl::buffer<GL_ARRAY_BUFFER>*>> attribute_buffers;
        for (auto [attribute_name, accessor_ix] : primitive.attributes) {
            const fx::gltf::Accessor& accessor = doc.accessors[accessor_ix];
            const fx::gltf::BufferView& view = doc.bufferViews[accessor.bufferView];
            // If this bufferView hasn't been uploaded to a gl buffer, do that now.
            auto gl_buffer_it = std::find_if (
                attribute_buffers.begin(), attribute_buffers.end(),
                [&](auto pair) { return pair.first == accessor.bufferView; }
            );
            if (gl_buffer_it == attribute_buffers.end()) {
                const fx::gltf::Buffer& buffer = doc.buffers[view.buffer];
                auto& gl_buffer = out.attribute_buffers.emplace_back();
                // For now just load the whole buffer
                gl_buffer.bind();
                glBufferData (
                    GL_ARRAY_BUFFER,
                    view.byteLength,
                    buffer.data.data() + view.byteOffset,
                    GL_STATIC_DRAW
                );
                attribute_buffers.emplace_front(view.buffer, &gl_buffer);
                gl_buffer_it = attribute_buffers.begin();
            }
            te::gl::buffer<GL_ARRAY_BUFFER>& gl_buffer = *gl_buffer_it->second;
            // Bind the buffer and associate the attribute with our vao
            gl_buffer.bind();
            auto attrib = program.attribute(attribute_name.c_str());
            glEnableVertexAttribArray(attrib);
            glVertexAttribPointer (
                attrib,// attribute in program
                component_count(accessor.type),// # of components
                static_cast<GLenum>(accessor.componentType),
                GL_FALSE,
                view.byteStride,// stride
                reinterpret_cast<void*>(accessor.byteOffset)
            );
        }
        const fx::gltf::Accessor& elements_accessor = doc.accessors[primitive.indices];
        const fx::gltf::BufferView& elements_view = doc.bufferViews[elements_accessor.bufferView];
        const fx::gltf::Buffer& elements_buffer = doc.buffers[elements_view.buffer];
        // Assume the element bufferView hasn't been loaded yet
        auto& gl_element_buffer = out.element_buffers.emplace_back();
        gl_element_buffer.bind();
        glBufferData (
            GL_ELEMENT_ARRAY_BUFFER,
            elements_view.byteLength,
            elements_buffer.data.data() + elements_view.byteOffset,
            GL_STATIC_DRAW
        );
        out.primitives.push_back ({
            gl_element_buffer,
            vao,
            static_cast<GLenum>(primitive.mode),
            static_cast<GLenum>(elements_accessor.componentType),
            elements_accessor.count,
            0 //we've already applied the offset during upload
            });
        glBindVertexArray(0);
    }
    return out;
}
te::mesh_renderer::mesh_renderer(std::string filename):
    program(te::gl::link(te::gl::compile(te::file_contents("shaders/basic_vertex.glsl"), GL_VERTEX_SHADER),
                         te::gl::compile(te::file_contents("shaders/basic_fragment.glsl"), GL_FRAGMENT_SHADER))),
    model_uniform(program.uniform("model")),
    view_uniform(program.uniform("view")),
    proj_uniform(program.uniform("projection")),
    texture(te::gl::image_texture("grass_tiles.png")),
    the_mesh(load_mesh(filename, program))
{
}
void te::mesh_renderer::draw(const te::camera& cam) {
    glUseProgram(*program.hnd);
    glm::mat4 model { 1 };
    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    for (auto& prim : the_mesh.primitives) {
        glBindVertexArray(prim.vao);
        glDrawElements(prim.mode, prim.count, prim.type, reinterpret_cast<void*>(prim.offset));
    }
}

class client {
    GLFWwindow* w;
    te::camera cam;
    te::terrain_renderer terrain;
    te::mesh_renderer meshr;
public:
    client(GLFWwindow* w):
        w {w},
        cam {
            {0.0f, 0.0f, 0.0f},
            {-8.0f, -8.0f, 8.0f}
        },
        terrain(rengine, 80, 80),
        meshr("BoxTextured.glb") {
    }

    void handle_key(int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, -glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
        if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
    }

    void handle_cursor_pos(double xpos, double ypos) {
    }

    void operator()() {
        if(glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) { glfwSetWindowShouldClose(w, true); };
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 forward = -cam.offset;
        forward.z = 0.0f;
        forward = glm::normalize(forward);
        glm::vec3 left = glm::rotate(forward, glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
        if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) cam.focus += 0.1f * forward;
        if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) cam.focus += 0.1f * left;
        if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) cam.focus -= 0.1f * forward;
        if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) cam.focus -= 0.1f * left;
	if (glfwGetKey(w, GLFW_KEY_H) == GLFW_PRESS) cam.zoom += 0.05f;
	if (glfwGetKey(w, GLFW_KEY_J) == GLFW_PRESS) cam.zoom -= 0.05f;

        terrain.render(cam);
        meshr.draw(cam);
    }
};

void glfw_error_callback(int error, const char* description) {
    BOOST_LOG_TRIVIAL(error) << "glfw3 error[" << error << "]: " << description;
    std::abort();
}

void opengl_error_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam) {
    BOOST_LOG_TRIVIAL(error) << "opengl error: " << message;
}

void glfw_dispatch_key(GLFWwindow* w, int key, int scancode, int action, int mods) {
    reinterpret_cast<client*>(glfwGetWindowUserPointer(w))
        ->handle_key(key, scancode, action, mods);
}

void glfw_dispatch_cursor_pos(GLFWwindow* w, double xpos, double ypos) {
    reinterpret_cast<client*>(glfwGetWindowUserPointer(w))
        ->handle_cursor_pos(xpos, ypos);
}
struct glfw_library {
    explicit glfw_library();
    ~glfw_library();
};
glfw_library::glfw_library() {
    if (!glfwInit()) {
        throw std::runtime_error("Could not initialise glfw");
    } else {
        BOOST_LOG_TRIVIAL(info) << "Initialized glfw " << glfwGetVersionString();
    }
}
glfw_library::~glfw_library() {
    glfwTerminate();
}
#include <cstddef>
int const win_w = 1024;
int const win_h = 768;
int main(void) {
    glfw_library glfw;
    glfwSetErrorCallback(glfw_error_callback);
    glfwWindowHint(GLFW_RESIZABLE , false);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    GLFWwindow* window = glfwCreateWindow(win_w, win_h, "Hello World", nullptr, nullptr);
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

    client g(window);
    glfwSetWindowUserPointer(window, &g);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, 1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetKeyCallback(window, glfw_dispatch_key);
    glfwSetCursorPosCallback(window, glfw_dispatch_cursor_pos);

    auto then = std::chrono::high_resolution_clock::now();
    int frames = 0;
    
    while (!glfwWindowShouldClose(window)) {
        g();
        glfwSwapBuffers(window);
        glfwPollEvents();
        frames++;
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> secs = now - then;
        if (secs.count() > 1.0) {
            fmt::print("fps: {}\n", frames / secs.count());
            frames = 0;
            then = std::chrono::high_resolution_clock::now();
        }
    }
    return 0;
}
