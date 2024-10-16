#include "sample_engine.h"

#include "framework/nodes/environment_3d.h"
#include "framework/nodes/camera.h"
#include "framework/parsers/parse_scene.h"
#include "framework/camera/camera.h"
#include "framework/camera/camera_3d.h"

#include "graphics/sample_renderer.h"
#include "graphics/renderer_storage.h"

#include "glm/gtx/quaternion.hpp"

#include "engine/scene.h"

#include "shaders/mesh_grid.wgsl.gen.h"

MeshInstance3D* SampleEngine::skybox = nullptr;
std::vector<EntityCamera*> SampleEngine::cameras;

bool SampleEngine::rotate_scene = false;
int SampleEngine::target_camera_idx = -1;
std::string SampleEngine::last_camera_target_name = "Current";

LerpedValue<glm::vec3> SampleEngine::eye_lerp;
LerpedValue<glm::vec3> SampleEngine::center_lerp;

int SampleEngine::initialize(Renderer* renderer, sEngineConfiguration configuration)
{
    int error = Engine::initialize(renderer);

    if (error) return error;

    main_scene = new Scene("main_scene");

    // Create skybox
    {
        skybox = new Environment3D();
        MeshInstance3D* skybox = new Environment3D();
        main_scene->add_node(skybox);
    }

    // Create grid
    {
        MeshInstance3D* grid = new MeshInstance3D();
        grid->set_name("Grid");
        grid->add_surface(RendererStorage::get_surface("quad"));
        grid->set_position(glm::vec3(0.0f));
        grid->rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        grid->scale(glm::vec3(10.f));
        grid->set_frustum_culling_enabled(false);

        // NOTE: first set the transparency and all types BEFORE loading the shader
        Material* grid_material = new Material();
        grid_material->set_transparency_type(ALPHA_BLEND);
        grid_material->set_cull_type(CULL_NONE);
        grid_material->set_type(MATERIAL_UNLIT);
        grid_material->set_shader(RendererStorage::get_shader_from_source(shaders::mesh_grid::source, shaders::mesh_grid::path, grid_material));
        grid->set_surface_material_override(grid->get_surface(0), grid_material);

        main_scene->add_node(grid);
    }

	return error;
}

void SampleEngine::clean()
{
    Engine::clean();
}

void SampleEngine::update(float delta_time)
{
    std::vector<Node*>& scene_nodes = main_scene->get_nodes();

    if (rotate_scene) {
        for (auto node : main_scene->get_nodes()) {
            MeshInstance3D* mesh_node = dynamic_cast<MeshInstance3D*>(node);
            if (mesh_node) {
                mesh_node->rotate(delta_time, normals::pY);
            }
        }
    }

    main_scene->update(delta_time);
    skybox->update(delta_time);

    // Interpolate current camera position
    if (target_camera_idx == -1)
    {
        Engine::update(delta_time);
    }
    else
    {
        Camera* camera = renderer->get_camera();

        // Lerp eye
        glm::vec3 new_eye = cameras[target_camera_idx]->get_translation();
        eye_lerp.value = smooth_damp(eye_lerp.value, new_eye, &eye_lerp.velocity, 0.50f, 20.0f, delta_time);

        // Lerp center
        glm::vec3 front = glm::normalize(camera->get_center() - camera->get_eye());
        glm::quat rot = glm::rotation(front, get_front(cameras[target_camera_idx]->get_model()));
        front = glm::rotate(rot, front);
        glm::vec3 new_center = new_eye + front;
        center_lerp.value = smooth_damp(center_lerp.value, new_center, &center_lerp.velocity, 0.50f, 20.0f, delta_time);

        camera->look_at(eye_lerp.value, center_lerp.value, camera->get_up());

        if (glm::distance(eye_lerp.value, new_eye) < 0.01f && glm::distance(center_lerp.value, new_center) < 0.01f)
        {
            target_camera_idx = -1;
        }
    }
}

void SampleEngine::render()
{
#ifndef __EMSCRIPTEN__
    render_default_gui();
#endif

    skybox->render();
    main_scene->render();

	Engine::render();
}

void SampleEngine::set_skybox_texture(const std::string& filename)
{
    if (!skybox) {
        return;
    }

    Texture* new_skybox = RendererStorage::get_texture(filename);
    skybox->get_surface(0)->get_material()->set_diffuse_texture(new_skybox);
}

void SampleEngine::load_glb(const std::string& filename)
{
    main_scene->delete_all();

    std::vector<Node*> entities;
    parse_scene(filename.c_str(), entities);

    main_scene->add_nodes(entities);

    // Each time we load entities, get the cameras!
    cameras.clear();
    for (auto node : main_scene->get_nodes())
    {
        EntityCamera* new_camera = dynamic_cast<EntityCamera*>(node);
        if (new_camera) {
            cameras.push_back(new_camera);
        }
    }
}

void SampleEngine::toggle_rotation()
{
    rotate_scene = !rotate_scene;
}

void SampleEngine::set_camera_type(int camera_type)
{
    SampleRenderer* renderer = static_cast<SampleRenderer*>(SampleRenderer::instance);
    renderer->set_camera_type(camera_type);
}

void SampleEngine::reset_camera()
{
    SampleRenderer* renderer = static_cast<SampleRenderer*>(SampleRenderer::instance);
    Camera3D* camera = static_cast<Camera3D*>(renderer->get_camera());

    for (auto node : main_scene->get_nodes())
    {
        EntityCamera* is_camera = dynamic_cast<EntityCamera*>(node);
        // Get first non camera
        if (!is_camera) {
            MeshInstance3D* mesh_node = dynamic_cast<MeshInstance3D*>(node);
            camera->look_at_entity(mesh_node);
            break;
        }
    }
}

void SampleEngine::set_camera_speed(float value)
{
    Camera* camera = Renderer::instance->get_camera();
    camera->set_speed(value);
}

void SampleEngine::set_camera_lookat_index(int index)
{
    if (index < 0 || index >= cameras.size())
        return;

    target_camera_idx = index;
    last_camera_target_name = cameras[index]->get_name();

    SampleRenderer* renderer = static_cast<SampleRenderer*>(SampleRenderer::instance);
    Camera* camera = renderer->get_camera();

    eye_lerp.value = camera->get_eye();
    center_lerp.value = camera->get_center();
}

std::vector<std::string> SampleEngine::get_cameras_names()
{
    std::vector<std::string> names;

    for (auto camera : cameras)
    {
        names.push_back(camera->get_name());
    }

    return names;
}
