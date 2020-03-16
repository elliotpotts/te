#include <te/cache.hpp>
#include <te/mesh.hpp>
#include <te/gl.hpp>
#include <spdlog/spdlog.h>
#include <fx/gltf.h>
#include <fmod_errors.h>

te::gl::texture2d te::asset_loader::operator()(type_tag<te::gl::texture2d>, const std::string& filename) {
    return gl.make_texture(filename);
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
    
    struct gltf_loader {
        te::gl::context& gl;
        const fx::gltf::Document& in;
        te::gltf& out;
        
        std::unordered_map<int, te::gl::buffer<GL_ARRAY_BUFFER>*> attribute_buffers;
        te::gl::buffer<GL_ARRAY_BUFFER>& load_array_buffer(int buffer_view_ix) {
            if (auto it = attribute_buffers.find(buffer_view_ix); it != attribute_buffers.end()) {
                return *it->second;
            }
            spdlog::info("        Loading attribute array {}/{}", buffer_view_ix + 1, in.bufferViews.size());
            const fx::gltf::BufferView& doc_buffer_view = in.bufferViews[buffer_view_ix];
            const fx::gltf::Buffer& doc_buffer = in.buffers[doc_buffer_view.buffer];
            const unsigned char* buffer_begin = doc_buffer.data.data() + doc_buffer_view.byteOffset;
            auto& gl_buffer = out.attribute_buffers.emplace_back(gl.make_buffer<GL_ARRAY_BUFFER>(buffer_begin, buffer_begin + doc_buffer_view.byteLength));
            attribute_buffers.emplace(buffer_view_ix, &gl_buffer);
            return gl_buffer;
        }
        
        std::unordered_map<int, te::gl::buffer<GL_ELEMENT_ARRAY_BUFFER>*> element_buffers;
        te::gl::buffer<GL_ELEMENT_ARRAY_BUFFER>& load_element_buffer(int buffer_view_ix) {
            if (auto it = element_buffers.find(buffer_view_ix); it != element_buffers.end()) {
                return *it->second;
            }
            spdlog::info("    Loading element array {}/{}", buffer_view_ix + 1, in.bufferViews.size());
            const fx::gltf::BufferView& doc_buffer_view = in.bufferViews[buffer_view_ix];
            const fx::gltf::Buffer& doc_buffer = in.buffers[doc_buffer_view.buffer];
            const unsigned char* buffer_begin = doc_buffer.data.data() + doc_buffer_view.byteOffset;
            auto& gl_buffer = out.element_buffers.emplace_back(gl.make_buffer<GL_ELEMENT_ARRAY_BUFFER>(buffer_begin, buffer_begin + doc_buffer_view.byteLength));
            element_buffers.emplace(buffer_view_ix, &gl_buffer);
            return gl_buffer;
        }
        
        std::unordered_map<int, te::gl::texture2d*> image_textures;
        te::gl::texture2d& load_image_texture(int image_ix) {
            spdlog::info("      Loading image {}/{}", image_ix + 1, in.images.size());
            const fx::gltf::Image& doc_image = in.images[image_ix];
            const fx::gltf::BufferView& doc_buffer_view = in.bufferViews[doc_image.bufferView];
            const fx::gltf::Buffer& doc_buffer = in.buffers[doc_buffer_view.buffer];
            const unsigned char* image_begin = doc_buffer.data.data() + doc_buffer_view.byteOffset;
            auto& gl_tex = out.textures.emplace_back(gl.make_texture(image_begin, image_begin + doc_buffer_view.byteLength));
            image_textures.emplace(image_ix, &gl_tex);
            return gl_tex;
        }
        
        std::unordered_map<int, te::gl::sampler*> samplers;
        te::gl::sampler& load_sampler(int sampler_ix) {
            if (auto it = samplers.find(sampler_ix); it != samplers.end()) {
                return *it->second;
            }
            spdlog::info("      Loading sampler {}/{}", sampler_ix + 1, in.samplers.size());
            const fx::gltf::Sampler& doc_sampler = in.samplers[sampler_ix];
            auto& gl_sampler = out.samplers.emplace_back(gl.make_sampler());
            gl_sampler.set(GL_TEXTURE_WRAP_S, static_cast<GLenum>(doc_sampler.wrapS));
            gl_sampler.set(GL_TEXTURE_WRAP_T, static_cast<GLenum>(doc_sampler.wrapT));
            gl_sampler.set(GL_TEXTURE_MIN_FILTER, static_cast<GLenum>(doc_sampler.minFilter));
            gl_sampler.set(GL_TEXTURE_MAG_FILTER, static_cast<GLenum>(doc_sampler.magFilter));
            samplers.emplace(sampler_ix, &gl_sampler);
            return gl_sampler;
        }

        struct foo_hash {
            std::size_t operator()(const std::tuple<int, int>& x) const {
                std::size_t h1 = std::hash<int>{}(std::get<0>(x));
                std::size_t h2 = std::hash<int>{}(std::get<1>(x));
                return h1 ^ (h2 << h1);
            }
        };
        
        std::unordered_map<std::tuple<int, int>, te::primitive*, foo_hash> primitives;
        te::primitive& load_primitive(int mesh_ix, int primitive_ix) {
            if (auto it = primitives.find(std::make_tuple(mesh_ix, primitive_ix)); it != primitives.end()) {
                return *it->second;
            }
            const fx::gltf::Mesh& doc_mesh = in.meshes[mesh_ix];
            spdlog::info("    Loading primitive {}/{}", primitive_ix + 1, doc_mesh.primitives.size());
            const fx::gltf::Primitive& doc_primitive = doc_mesh.primitives[primitive_ix];
            std::list<te::attribute_source> attributes;
            for (auto [attrib_name, accessor_ix] : doc_primitive.attributes) {
                auto attrib_loc_it = std::find_if (
                    te::gl::common_attribute_names.begin(),
                    te::gl::common_attribute_names.end(),
                    [&] (const auto& pair) {
                        return pair.first == attrib_name;
                    }
                );
                if (attrib_loc_it == te::gl::common_attribute_names.end()) {
                    spdlog::info("      Unused attribute {}", attrib_name);
                    continue;
                } else {
                    spdlog::info("      Mapping attribute {} to location {}", attrib_name, attrib_loc_it->second);
                }
                const fx::gltf::Accessor& accessor = in.accessors[accessor_ix];
                const fx::gltf::BufferView& view = in.bufferViews[accessor.bufferView];
                attributes.emplace_back ( te::attribute_source {
                    load_array_buffer(accessor.bufferView),
                    attrib_loc_it->second,
                    component_count(accessor.type),
                    static_cast<GLenum>(accessor.componentType),
                    GL_FALSE, // don't normalise
                    static_cast<GLsizei>(view.byteStride),
                    reinterpret_cast<void*>(accessor.byteOffset)
                });
            }

            const fx::gltf::Accessor& elements_accessor = in.accessors[doc_primitive.indices];
            //out_primitive.inputs.elements = load_element_buffer(elements_accessor.bufferView);
            //out_primitive.mode = static_cast<GLenum>(doc_primitive.mode);
            //out_primitive.element_type = static_cast<GLenum>(elements_accessor.componentType);
            //out_primitive.element_count = elements_accessor.count;
            //out_primitive.element_offset = 0; //elements_accessor.byteOffset;

            const fx::gltf::Material::PBRMetallicRoughness& doc_material = in.materials[doc_primitive.material].pbrMetallicRoughness;
            const fx::gltf::Texture doc_texture_info = in.textures[doc_material.baseColorTexture.index];
            std::list<te::texture_unit_binding> texture_unit_bindings;
            if (doc_texture_info.sampler >= 0) {
                texture_unit_bindings.push_back (
                    te::texture_unit_binding {
                        &load_image_texture(doc_texture_info.source),
                        &load_sampler(doc_texture_info.sampler)
                    }
                );
            } else {
                texture_unit_bindings.push_back (
                    te::texture_unit_binding {
                        &load_image_texture(doc_texture_info.source),
                        nullptr
                    }
                );
            }

            auto& out_primitive = out.primitives.emplace_back (
                te::primitive {
                    te::input_description {
                        attributes,
                        load_element_buffer(elements_accessor.bufferView)
                    },
                    static_cast<GLenum>(doc_primitive.mode),
                    static_cast<GLenum>(elements_accessor.componentType),
                    elements_accessor.count,
                    elements_accessor.byteOffset,
                    texture_unit_bindings
                 }
            );
            primitives.emplace (
                std::make_tuple(mesh_ix, primitive_ix),
                &out_primitive
            );
            return out_primitive;
        }

        std::unordered_map<int, te::mesh*> meshes;
        te::mesh& load_mesh(int mesh_ix) {
            if (auto it = meshes.find(mesh_ix); it != meshes.end()) {
                return *it->second;
            }
            spdlog::info("  Loading mesh {}/{}", mesh_ix + 1, in.meshes.size());
            const fx::gltf::Mesh& doc_mesh = in.meshes[mesh_ix];
            te::mesh& out_mesh = out.meshes.emplace_back();
            for (std::size_t primitive_ix = 0; primitive_ix < doc_mesh.primitives.size(); primitive_ix++) {
                out_mesh.primitives.push_back(load_primitive(mesh_ix, primitive_ix));
            }
            meshes.emplace(mesh_ix, &out_mesh);
            return out_mesh;
        }

        
        gltf_loader(te::gl::context& gl, const fx::gltf::Document& in, te::gltf& out) : gl{gl}, in{in}, out{out} {
        }
    };
}

te::gltf te::asset_loader::operator()(type_tag<te::gltf>, const std::string& filename) {
    const fx::gltf::Document in = fx::gltf::LoadFromBinary(filename);
    te::gltf out;
    gltf_loader loader {gl, in, out};
    for (std::size_t i = 0; i < in.meshes.size(); i++) {
        loader.load_mesh(i);
    }
    return out;
}

te::fmod_sound_hnd te::asset_loader::operator()(type_tag<te::fmod_sound_hnd>, const std::string& filename) {
    FMOD::Sound* sound;
    if (FMOD_RESULT result = fmod.createSound(filename.c_str(), FMOD_3D, nullptr, &sound); result != FMOD_OK) {
        throw std::runtime_error(fmt::format("Couldn't create sound due to error {}: {}", result, FMOD_ErrorString(result)));
    } else {
        return te::fmod_sound_hnd{sound};
    }
}
