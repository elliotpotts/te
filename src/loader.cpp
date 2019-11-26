#include <te/loader.hpp>
#include <glad/glad.h>
#include <te/mesh_renderer.hpp>
#include <fx/gltf.h>
#include <unordered_map>
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
    te::gl::buffer<target>& get_gl_buffer(std::vector<te::gl::buffer<target>>& store,
                                          buffer_cache<target>& loaded,
                                          const fx::gltf::BufferView& view,
                                          const std::vector<fx::gltf::Buffer>& doc_buffers) {
        auto buffer_it = std::find_if (
            loaded.begin(), loaded.end(),
            [&view](auto pair) { return pair.first == &view; }
        );
        if (buffer_it == loaded.end()) {
            te::gl::buffer<target>& gl_buffer = store.emplace_back();
            gl_buffer.bind();
            glBufferData (
                target,
                view.byteLength,
                doc_buffers[view.buffer].data.data() + view.byteOffset,
                GL_STATIC_DRAW
            );
            loaded.emplace(&view, &gl_buffer);
            return gl_buffer;
        } else {
            return *buffer_it->second;
        }
    }
}
#include <FreeImage.h>
namespace {
    GLuint memory_texture(const fx::gltf::Document& doc, const fx::gltf::Texture& doc_texture) {
        const fx::gltf::Image& image = doc.images[doc_texture.source];
        const fx::gltf::BufferView& view = doc.bufferViews[image.bufferView];
        const fx::gltf::Buffer& buffer = doc.buffers[view.buffer];
        FIMEMORY* memory_img = FreeImage_OpenMemory(const_cast<unsigned char*>(buffer.data.data()) + view.byteOffset, view.byteLength);
        FREE_IMAGE_FORMAT fmt = FreeImage_GetFileTypeFromMemory(memory_img);
        if(fmt == FIF_UNKNOWN) {
            throw std::runtime_error("Couldn't determine file format of memory image!");
        }
        FIBITMAP* bitmap = FreeImage_LoadFromMemory(fmt, memory_img, 0);
        if(!bitmap) {
            throw std::runtime_error("Couldn't load memory image.");
        }
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D (
            GL_TEXTURE_2D, 0, GL_RGB,
            FreeImage_GetWidth(bitmap), FreeImage_GetHeight(bitmap),
            0, GL_BGRA, GL_UNSIGNED_BYTE, FreeImage_GetBits(bitmap)
        );
        FreeImage_Unload(bitmap);
        // TODO: do samplers separately from textures
        const fx::gltf::Sampler& sampler = doc.samplers[doc_texture.sampler];
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLenum>(sampler.wrapS));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLenum>(sampler.wrapT));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLenum>(sampler.minFilter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLenum>(sampler.magFilter));
        glGenerateMipmap(GL_TEXTURE_2D);
        return tex;
    }
}

te::mesh te::load_mesh(std::string filename, gl::program& program) {
    te::mesh out;
    const fx::gltf::Document doc = fx::gltf::LoadFromBinary(filename);
    buffer_cache<GL_ARRAY_BUFFER> loaded_attribute_buffers;
    buffer_cache<GL_ELEMENT_ARRAY_BUFFER> loaded_element_buffers;
    // we're only going to load the first mesh.
    const fx::gltf::Mesh& docmesh = doc.meshes[0];
    for (const fx::gltf::Primitive& primitive : docmesh.primitives) {
        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        for (auto [attribute_name, accessor_ix] : primitive.attributes) {
            const fx::gltf::Accessor& accessor = doc.accessors[accessor_ix];
            const fx::gltf::BufferView& view = doc.bufferViews[accessor.bufferView];
            te::gl::buffer<GL_ARRAY_BUFFER>& gl_buffer = get_gl_buffer(out.attribute_buffers, loaded_attribute_buffers, view, doc.buffers);
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
        auto& gl_element_buffer = get_gl_buffer(out.element_buffers, loaded_element_buffers, elements_view, doc.buffers);
        gl_element_buffer.bind();
        // load one texture
        const fx::gltf::Material::PBRMetallicRoughness& material = doc.materials[primitive.material].pbrMetallicRoughness;
        const fx::gltf::Texture& base_colour_texture = doc.textures[material.baseColorTexture.index];
        
        out.primitives.push_back ({
                vao,
                static_cast<GLenum>(primitive.mode),
                static_cast<GLenum>(elements_accessor.componentType),
                elements_accessor.count,
                0, //we've already applied the offset during upload
                memory_texture(doc, base_colour_texture)
            });
        glBindVertexArray(0);
    }
    return out;
}
