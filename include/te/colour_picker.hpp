#ifndef TE_COLOUR_PICKER_HPP_INCLUDED
#define TE_COLOUR_PICKER_HPP_INCLUDED
#include <te/gl.hpp>
#include <te/window.hpp>
#include <te/camera.hpp>
#include <glm/glm.hpp>
#include <te/mesh.hpp>
namespace te {
    struct colour_picker {
        te::window& win;
        gl::program program;
        GLint model_uniform;
        GLint view_uniform;
        GLint proj_uniform;
        GLint pick_colour_uniform;
        gl::framebuffer colour_fbuffer;
        gl::renderbuffer attachment;
        colour_picker(te::window& win);
        void draw(te::primitive& themesh, const glm::mat4& model, std::uint32_t id, const te::camera& cam);
    };
}
#endif
