#include <te/loader.hpp>
#include <glad/glad.h>
#include <te/mesh_renderer.hpp>
#include <fx/gltf.h>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <iterator>
#include <sstream>
namespace {
    std::string stringise_extents(const std::vector<float>& extents) {
        if (extents.size() == 0) { return ""; }
        if (extents.size() == 0) { return std::to_string(extents[0]); }
        std::ostringstream sstr;
        std::copy(extents.begin(), std::prev(extents.end()), std::ostream_iterator<float>(sstr, ", "));
        sstr << extents.back();
        return sstr.str();
    }
}

namespace {
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

te::mesh te::load_mesh(te::gl::context& gl, std::string filename, const gl::attribute_locations& attribute_locations) {
    spdlog::info("Loading mesh from {}", filename);
    te::mesh out;
    const fx::gltf::Document doc = fx::gltf::LoadFromBinary(filename);
    buffer_cache<GL_ARRAY_BUFFER> loaded_attribute_buffers;
    buffer_cache<GL_ELEMENT_ARRAY_BUFFER> loaded_element_buffers;
    spdlog::info("  Loading mesh 1/{}", doc.meshes.size());
    // we're only going to load the first mesh for now.
    const fx::gltf::Mesh& docmesh = doc.meshes[0];
    for (std::size_t primitive_ix = 0; primitive_ix < docmesh.primitives.size(); primitive_ix++) {
        spdlog::info("    Loading primitive {}/{}", primitive_ix + 1, docmesh.primitives.size());
        const fx::gltf::Primitive& primitive = docmesh.primitives[primitive_ix];
        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        for (auto [attribute_name, accessor_ix] : primitive.attributes) {
            auto attrib_it = std::find_if(attribute_locations.begin(), attribute_locations.end(),
                                          [&] (const auto& pair) { return pair.first == attribute_name; });
            if (attrib_it == attribute_locations.end()) {
                spdlog::info("      Unused attribute {}", attribute_name);
                continue;
            } else {
                spdlog::info("      Mapping attribute {} to location {}", attribute_name, attrib_it->second);
            }
            const fx::gltf::Accessor& accessor = doc.accessors[accessor_ix];
            if (accessor.min.size() > 0 && accessor.max.size() > 0) {
                spdlog::info("        Min/max extents are ({}), ({})", stringise_extents(accessor.min), stringise_extents(accessor.max));
            }
            const fx::gltf::BufferView& view = doc.bufferViews[accessor.bufferView];
            te::gl::buffer<GL_ARRAY_BUFFER>& gl_buffer = get_gl_buffer(gl, out.attribute_buffers, loaded_attribute_buffers, view, doc.buffers);
            // Bind the buffer and associate the attribute with our vao
            gl_buffer.bind();
            glEnableVertexAttribArray(attrib_it->second);
            glVertexAttribPointer (
                attrib_it->second,// attribute location
                component_count(accessor.type),
                static_cast<GLenum>(accessor.componentType),
                GL_FALSE, // normalise
                view.byteStride,
                reinterpret_cast<void*>(accessor.byteOffset)
            );
        }
        const fx::gltf::Accessor& elements_accessor = doc.accessors[primitive.indices];
        const fx::gltf::BufferView& elements_view = doc.bufferViews[elements_accessor.bufferView];
        auto& gl_element_buffer = get_gl_buffer(gl, out.element_buffers, loaded_element_buffers, elements_view, doc.buffers);
        gl_element_buffer.bind();

        const fx::gltf::Material::PBRMetallicRoughness& material = doc.materials[primitive.material].pbrMetallicRoughness;
        te::gl::texture<GL_TEXTURE_2D>* gl_texture = nullptr;
        te::gl::sampler* gl_sampler = nullptr;
        if (material.baseColorTexture.index >= 0) {
            const fx::gltf::Texture& texture = doc.textures[material.baseColorTexture.index];
            const fx::gltf::Image& image = doc.images[texture.source];
            const fx::gltf::BufferView& view = doc.bufferViews[image.bufferView];
            const fx::gltf::Buffer& buffer = doc.buffers[view.buffer];
            gl_texture = &out.textures.emplace_back(gl.make_texture(buffer.data.data() + view.byteOffset,
                                                                    buffer.data.data() + view.byteOffset + view.byteLength));

            if (texture.sampler >= 0) {
                const fx::gltf::Sampler& sampler = doc.samplers[texture.sampler];
                gl_sampler = &out.samplers.emplace_back(gl.make_sampler());
                gl_sampler->set(GL_TEXTURE_WRAP_S, static_cast<GLenum>(sampler.wrapS));
                gl_sampler->set(GL_TEXTURE_WRAP_T, static_cast<GLenum>(sampler.wrapT));
                gl_sampler->set(GL_TEXTURE_MIN_FILTER, static_cast<GLenum>(sampler.minFilter));
                gl_sampler->set(GL_TEXTURE_MAG_FILTER, static_cast<GLenum>(sampler.magFilter));
            }
        } else {
            spdlog::warn("Mesh material {} does not specify a base colour texture", primitive.material);
        }
        
        out.primitives.push_back (te::primitive {
                vao,
                static_cast<GLenum>(primitive.mode),
                static_cast<GLenum>(elements_accessor.componentType),
                elements_accessor.count,
                0, //we've already applied the offset during upload
                gl_texture,
                gl_sampler
            });
        glBindVertexArray(0);
    }
    return out;
}
