#include <te/instance_renderer.hpp>
#include <te/util.hpp>
#include <sstream>
#include <fx/gltf.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

const std::vector<std::pair<te::gl::string, GLuint>> te::instance_renderer::attribute_locations = {
    {"POSITION", 0},
    {"INSTANCE_OFFSET", 4},
    {"NORMAL", 1},
    {"TANGENT", 2},
    {"TEXCOORD_0", 3},
};

te::instance_renderer::instance_renderer(gl::context& ogl):
    gl(ogl),
    program(gl.link(gl.compile(te::file_contents("shaders/instance_vertex.glsl"), GL_VERTEX_SHADER),
                    gl.compile(te::file_contents("shaders/instance_fragment.glsl"), GL_FRAGMENT_SHADER),
                    attribute_locations)),
    view(program.uniform("view")),
    proj(program.uniform("projection")),
    model(program.uniform("model")),
    sampler(program.uniform("tex"))
{
}

namespace {
    std::string stringise_extents(const std::vector<float>& extents) {
        if (extents.size() == 0) { return ""; }
        if (extents.size() == 0) { return std::to_string(extents[0]); }
        std::ostringstream sstr;
        std::copy(extents.begin(), std::prev(extents.end()), std::ostream_iterator<float>(sstr, ", "));
        sstr << extents.back();
        return sstr.str();
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

    template<GLenum target>
    using buffer_cache = std::unordered_map<const fx::gltf::BufferView*, te::gl::buffer<target>*>;
    
    template<GLenum target>
    te::gl::buffer<target>& get_gl_buffer(te::gl::context& gl,
                                          std::vector<te::gl::buffer<target>>& store,
                                          buffer_cache<target>& loaded,
                                          const fx::gltf::BufferView& view,
                                          const std::vector<fx::gltf::Buffer>& doc_buffers) {
        auto buffer_it = std::find_if (
            loaded.begin(), loaded.end(),
            [&view](auto pair) { return pair.first == &view; }
        );
        if (buffer_it == loaded.end()) {
            const unsigned char* begin = doc_buffers[view.buffer].data.data() + view.byteOffset;
            te::gl::buffer<target>& gl_buffer = store.emplace_back(gl.make_buffer<target>(begin, begin + view.byteLength));
            loaded.emplace(&view, &gl_buffer);
            return gl_buffer;
        } else {
            return *buffer_it->second;
        }
    }
}

te::instanced_primitive te::instance_renderer::load_from_file(std::string filename, int howmany) {
    spdlog::info("Loading primitive from {}", filename);
    const fx::gltf::Document doc = fx::gltf::LoadFromBinary(filename);
    spdlog::info("Loading primitive 1/{} from mesh 1/{}", doc.meshes[0].primitives.size(), doc.meshes.size());
    const fx::gltf::Primitive& docprim = doc.meshes[0].primitives[0];
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    std::vector<gl::buffer<GL_ARRAY_BUFFER>> gl_attribute_buffers;
    for (auto [attr_name, accessor_ix] : docprim.attributes) {
        auto attrib_loc_it = std::find_if(attribute_locations.begin(), attribute_locations.end(),
                                          [&] (const auto& pair) { return pair.first == attr_name; });
        if (attrib_loc_it == attribute_locations.end()) {
            spdlog::info("  Unused attribute {}", attr_name);
            continue;
        } else {
            spdlog::info("  Mapping attribute {} to location {}", attr_name, attrib_loc_it->second);
        }
        const fx::gltf::Accessor& accessor = doc.accessors[accessor_ix];
        if (accessor.min.size() > 0 && accessor.max.size() > 0) {
            spdlog::info("    Min/max extents are ({}), ({})", stringise_extents(accessor.min), stringise_extents(accessor.max));
        }
        const fx::gltf::BufferView& view = doc.bufferViews[accessor.bufferView];
        gl::buffer<GL_ARRAY_BUFFER>& gl_attribute_buffer = gl_attribute_buffers.emplace_back (
            gl.make_buffer<GL_ARRAY_BUFFER> (
                doc.buffers[view.buffer].data.data() + view.byteOffset,
                doc.buffers[view.buffer].data.data() + view.byteOffset + view.byteLength
            )
        );
        gl_attribute_buffer.bind();
        glEnableVertexAttribArray(attrib_loc_it->second);
        glVertexAttribPointer (
            attrib_loc_it->second,
            component_count(accessor.type),
            static_cast<GLenum>(accessor.componentType),
            GL_FALSE, // normalise
            view.byteStride,
            reinterpret_cast<void*>(accessor.byteOffset)
        );
    }
    const fx::gltf::Material::PBRMetallicRoughness& doc_material = doc.materials[docprim.material].pbrMetallicRoughness;
    const fx::gltf::Texture& doc_texture = doc.textures[doc_material.baseColorTexture.index];
    const fx::gltf::Image& doc_image = doc.images[doc_texture.source];
    const fx::gltf::BufferView& doc_image_view = doc.bufferViews[doc_image.bufferView];
    const fx::gltf::Buffer& doc_image_buffer = doc.buffers[doc_image_view.buffer];
    te::gl::texture<GL_TEXTURE_2D> gl_texture { gl.make_texture(doc_image_buffer.data.data() + doc_image_view.byteOffset,
                                                                doc_image_buffer.data.data() + doc_image_view.byteOffset + doc_image_view.byteLength) };
    
    te::gl::sampler gl_sampler = gl.make_sampler();
    gl_sampler.set(GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl_sampler.set(GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl_sampler.set(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_sampler.set(GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    te::gl::buffer<GL_ARRAY_BUFFER> gl_instance_attribute_buffer { gl.make_hnd<te::gl::buffer_hnd>(glGenBuffers) };
    gl_instance_attribute_buffer.bind();
    glEnableVertexAttribArray(4);
    glVertexAttribPointer (
        4, // attribute location. TODO: un-hardcode this location
        3, // component count. TODO: un-hardcode this component count
        GL_FLOAT, // component type.
        GL_FALSE, // normalise
        0, // stride
        reinterpret_cast<void*>(0) // offset
    );
    glVertexAttribDivisor(4, 1);

    const fx::gltf::Accessor& doc_elements_accessor = doc.accessors[docprim.indices];
    const fx::gltf::BufferView& elements_view = doc.bufferViews[doc_elements_accessor.bufferView];
    auto gl_element_buffer = gl.make_buffer<GL_ELEMENT_ARRAY_BUFFER>(doc.buffers[elements_view.buffer].data.data() + elements_view.byteOffset,
                                                                     doc.buffers[elements_view.buffer].data.data() + elements_view.byteOffset + elements_view.byteLength);
    glBindVertexArray(0);
    return te::instanced_primitive {
        std::move(gl_attribute_buffers),
        std::move(gl_element_buffer),
        std::move(gl_instance_attribute_buffer),
        vao,
        static_cast<GLenum>(docprim.mode),
        static_cast<GLenum>(doc_elements_accessor.componentType),
        doc_elements_accessor.count,
        doc_elements_accessor.byteOffset,
        std::move(gl_texture),
        std::move(gl_sampler)
    };
};
void te::instance_renderer::draw(te::instanced_primitive& prim, const glm::mat4& model_mat, const te::camera& cam) {
    glUseProgram(*program.hnd);
    glUniformMatrix4fv(view, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(model_mat));
    //TODO: do we need a default sampler?
    prim.sampler.bind(0);
    prim.texture.activate(0);
    glBindVertexArray(prim.vao);
    glDrawElementsInstanced(prim.element_mode, prim.element_count, prim.element_type, reinterpret_cast<void*>(prim.element_offset), 1);
}
