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
        get_gl_buffer(out.element_buffers, loaded_element_buffers, elements_view, doc.buffers).bind();
        out.primitives.push_back ({
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
