#include "sample_renderer.h"

#include "graphics/shader.h"

#include "framework/camera/camera.h"
#include "framework/camera/flyover_camera.h"
#include "framework/camera/orbit_camera.h"

SampleRenderer::SampleRenderer() : Renderer()
{

}

int SampleRenderer::pre_initialize(GLFWwindow* window, bool use_mirror_screen)
{
    return Renderer::pre_initialize(window, use_mirror_screen);
}

int SampleRenderer::initialize()
{
    int err = Renderer::initialize();

    clear_color = glm::vec4(0.22f, 0.22f, 0.22f, 1.0);

    return err;
}

int SampleRenderer::post_initialize()
{
    return Renderer::post_initialize();
}

void SampleRenderer::clean()
{
    Renderer::clean();
}

void SampleRenderer::update(float delta_time)
{
    Renderer::update(delta_time);
}

void SampleRenderer::render()
{
    Renderer::render();
}
