#include "sample_engine.h"

#include "framework/nodes/environment_3d.h"
#include "framework/nodes/camera.h"
#include "framework/parsers/parse_scene.h"
#include "framework/camera/camera.h"
#include "framework/camera/camera_3d.h"
#include "framework/input.h"

#include "graphics/sample_renderer.h"
#include "graphics/renderer_storage.h"

#include "glm/gtx/quaternion.hpp"

#include "engine/scene.h"

#include "shaders/mesh_grid.wgsl.gen.h"

int SampleEngine::initialize(Renderer* renderer, sEngineConfiguration configuration)
{
    int error = Engine::initialize(renderer, configuration);

    if (error) return error;

    main_scene = new Scene("main_scene");

	return error;
}

int SampleEngine::post_initialize()
{
    // Create skybox
    {
        skybox = new Environment3D();
        MeshInstance3D* skybox = new Environment3D();
        main_scene->add_node(skybox);
    }

    // Create grid
    //{
    //    MeshInstance3D* grid = new MeshInstance3D();
    //    grid->set_name("Grid");
    //    grid->add_surface(RendererStorage::get_surface("quad"));
    //    grid->set_position(glm::vec3(0.0f));
    //    grid->rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //    grid->scale(glm::vec3(10.f));
    //    grid->set_frustum_culling_enabled(false);

    //    // NOTE: first set the transparency and all types BEFORE loading the shader
    //    Material* grid_material = new Material();
    //    grid_material->set_transparency_type(ALPHA_BLEND);
    //    grid_material->set_cull_type(CULL_NONE);
    //    grid_material->set_type(MATERIAL_UNLIT);
    //    grid_material->set_shader(RendererStorage::get_shader_from_source(shaders::mesh_grid::source, shaders::mesh_grid::path, grid_material));
    //    grid->set_surface_material_override(grid->get_surface(0), grid_material);

    //    main_scene->add_node(grid);
    //}

    return 0;
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

    Engine::update(delta_time);

    main_scene->update(delta_time);
    skybox->update(delta_time);

    //if (Input::was_key_pressed(GLFW_KEY_O)) {
    //    set_camera_type(CAMERA_ORBIT);
    //}

    //if (Input::was_key_pressed(GLFW_KEY_F)) {
    //    set_camera_type(CAMERA_FLYOVER);
    //}

    // Interpolate current camera position
    if (target_camera_idx != -1)
    {
        // Lerp eye
        glm::vec3 new_eye = cameras[target_camera_idx]->get_translation();
        eye_lerp.value = smooth_damp(eye_lerp.value, new_eye, &eye_lerp.velocity, 0.50f, 20.0f, delta_time);

        Camera* camera = renderer->get_camera();
        camera->look_at(eye_lerp.value, camera->get_center(), camera->get_up());

        if (glm::distance(eye_lerp.value, new_eye) < 0.01f)
        {
            target_camera_idx = -1;
        }
    }

    if (lerp_center) {
        // Lerp center
        center_lerp.value = smooth_damp(center_lerp.value, target_center, &center_lerp.velocity, 0.25f, 20.0f, delta_time);

        Camera* camera = renderer->get_camera();
        camera->look_at(camera->get_eye(), center_lerp.value, camera->get_up());

        if (glm::distance(center_lerp.value, target_center) < 0.01f)
        {
            lerp_center = false;
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

std::vector<std::string> SampleEngine::load_glb(const std::string& filename)
{
    main_scene->delete_all();

    std::vector<Node*> entities;
    parse_scene(filename.c_str(), entities);

    main_scene->add_nodes(entities);

    cameras.clear();

    std::function<void(Node*)> recurse_tree = [&](Node* node) {
        EntityCamera* new_camera = dynamic_cast<EntityCamera*>(node);
        if (new_camera) {
            cameras.push_back(new_camera);
        }

        if (!node->get_children().empty()) {
            for (auto child : node->get_children()) {
                recurse_tree(child);
            }
        }
    };

    // Each time we load entities, get the cameras!
    for (auto node : main_scene->get_nodes()) {
        recurse_tree(node);
    }

    reset_camera();

    if (!cameras.empty()) {
        set_camera_lookat_index(0);
    }

    return get_cameras_names();
}

void SampleEngine::load_ply(const std::string& filename)
{
    main_scene->delete_all();

    std::vector<Node*> entities;
    parse_scene(filename.c_str(), entities);

    main_scene->add_nodes(entities);

    cameras.clear();
}

void SampleEngine::toggle_rotation()
{
    rotate_scene = !rotate_scene;
}

void SampleEngine::set_camera_type(int camera_type)
{
    SampleRenderer* renderer = static_cast<SampleRenderer*>(SampleRenderer::instance);

    if (camera_type == CAMERA_ORBIT) {
        //renderer->get_camera()->set_center(glm::vec3(target_center));
        center_lerp.value = renderer->get_camera()->get_center();
        lerp_center = true;
    }

    renderer->set_camera_type(static_cast<eCameraType>(camera_type));
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
            Node3D* mesh_node = dynamic_cast<Node3D*>(node);
            if (mesh_node)
            {
                camera->look_at_entity(mesh_node);
                target_center = mesh_node->get_aabb().center;
                lerp_center = true;
                break;
            }
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
    names.resize(cameras.size());

    for (int i = 0; i < cameras.size(); ++i)
    {
        names[i] = cameras[i]->get_name();
    }

    return names;
}
