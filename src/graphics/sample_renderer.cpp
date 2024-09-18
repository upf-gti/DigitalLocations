#include "sample_renderer.h"

#include "graphics/shader.h"

#include "framework/camera/camera.h"
#include "framework/camera/flyover_camera.h"
#include "framework/camera/orbit_camera.h"

SampleRenderer::SampleRenderer() : Renderer()
{

}

int SampleRenderer::initialize(GLFWwindow* window, bool use_mirror_screen)
{
    Renderer::initialize(window, use_mirror_screen);

    clear_color = glm::vec4(0.22f, 0.22f, 0.22f, 1.0);

    return 0;
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

void SampleRenderer::set_camera_type(int camera_type)
{
    Camera* old_camera = camera;
    if (camera_type == CAMERA_FLYOVER) {
        camera = new FlyoverCamera();
        camera->look_at(old_camera->get_eye(), old_camera->get_center(), old_camera->get_up());
    }
    else if (camera_type == CAMERA_ORBIT) {
        camera = new OrbitCamera();
        camera->look_at(old_camera->get_eye(), old_camera->get_center(), old_camera->get_up());
    }

    delete old_camera;
}
