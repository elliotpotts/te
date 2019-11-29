#include <te/terrain_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    te::gl::buffer<GL_ARRAY_BUFFER> fill_grid(te::gl::context& gl, std::mt19937& rengine, int width, int height) {
        static std::uniform_int_distribution tile_select {0, 1};
        struct vertex {
            glm::vec2 pos;
            glm::vec3 col;
            glm::vec2 uv;
        };
        static_assert(sizeof(vertex) == sizeof(GLfloat) * 7, "Platform doesn't support this directly.");
        std::vector<vertex> data;
        data.reserve(width * height * 6);
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
        return gl.make_buffer<GL_ARRAY_BUFFER>(data.begin(), data.end());
    }
}

te::terrain_renderer::terrain_renderer(gl::context& ogl, std::mt19937& rengine, int width, int height):
    gl(ogl),
    width(width),
    height(height),
    vbo(fill_grid(gl, rengine, width, height)),
    program(gl.link(gl.compile(te::file_contents("shaders/terrain_vertex.glsl"), GL_VERTEX_SHADER),
                    gl.compile(te::file_contents("shaders/terrain_fragment.glsl"), GL_FRAGMENT_SHADER)).hnd),
    model_uniform(program.uniform("model")),
    view_uniform(program.uniform("view")),
    proj_uniform(program.uniform("projection")),
    sampler(gl.make_sampler()),
    texture(gl.image_texture("grass_tiles.png"))
{
    glUseProgram(*program.hnd);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    GLint pos_attrib = program.find_attribute("position").value();
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 7*sizeof(float), reinterpret_cast<void*>(0));
    GLint col_attrib = program.find_attribute("colour").value();
    glEnableVertexAttribArray(col_attrib);
    glVertexAttribPointer(col_attrib, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), reinterpret_cast<void*>(2*sizeof(float)));
    GLint tex_attrib = program.find_attribute("texcoord").value();
    glEnableVertexAttribArray(tex_attrib);
    glVertexAttribPointer(tex_attrib, 2, GL_FLOAT, GL_FALSE, 7*sizeof(float), reinterpret_cast<void*>(5*sizeof(float)));
}

void te::terrain_renderer::render(const te::camera& cam) {
    glUseProgram(*program.hnd);
    glm::mat4 model { 1 };
    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    glBindVertexArray(vao);

    sampler.bind(0);
    texture.activate(0);
    
    glDrawArrays(GL_TRIANGLES, 0, width * height * 6);
}
