#include "sample_engine.h"
#include "framework/nodes/mesh_instance_3d.h"
#include "framework/nodes/text.h"
#include "framework/nodes/camera.h"
#include "framework/nodes/environment_3d.h"
#include "framework/input.h"
#include "framework/scene/parse_scene.h"
#include "framework/scene/parse_gltf.h"
#include "graphics/sample_renderer.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"

#include "framework/utils/tinyfiledialogs.h"

#include "spdlog/spdlog.h"

Environment3D* SampleEngine::skybox = nullptr;

std::vector<Node3D*> SampleEngine::entities;
std::vector<EntityCamera*> SampleEngine::cameras;

bool SampleEngine::rotate_scene = false;
int SampleEngine::target_camera_idx = -1;
std::string SampleEngine::last_camera_target_name = "Current";

LerpedValue<glm::vec3> SampleEngine::eye_lerp;
LerpedValue<glm::vec3> SampleEngine::center_lerp;

int SampleEngine::initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen)
{
	int error = Engine::initialize(renderer, window, use_glfw, use_mirror_screen);

    // Create skybox

    skybox = new Environment3D();
    entities.push_back(skybox);

    //MeshInstance3D* cube = parse_mesh("data/meshes/cube/cube.obj");
    //cube->scale(glm::vec3(0.1f));
    //entities.push_back(cube);

    // load_glb("data/scenes/Cameras.glb");

	return error;
}

void SampleEngine::clean()
{
    Engine::clean();
}

void SampleEngine::update(float delta_time)
{
    if (rotate_scene)
        for (auto e : entities) e->rotate(delta_time, normals::pY);

    for (auto entity : entities) {
        entity->update(delta_time);
    }

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
        glm::vec3 front = normalize(camera->get_center() - camera->get_eye());
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
    render_gui();
#endif

	for (auto entity : entities) {
		entity->render();
	}

	Engine::render();
}

void SampleEngine::render_gui()
{
    if (SampleRenderer::instance->get_openxr_available()) {
        return;
    }
    bool active = true;

    ImGui::SetNextWindowSize({ 300, 400 });
    ImGui::Begin("Debug panel", &active, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open scene (.gltf, .glb, .obj)"))
            {
                std::vector<const char*> filter_patterns = { "*.gltf", "*.glb", "*.obj" };
                char const* open_file_name = tinyfd_openFileDialog(
                    "Scene loader",
                    "",
                    filter_patterns.size(),
                    filter_patterns.data(),
                    "Scene formats",
                    0
                );

                if (open_file_name) {
                    load_glb(open_file_name);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("TabBar", tab_bar_flags))
    {
        if (ImGui::BeginTabItem("Scene"))
        {
            if (ImGui::TreeNodeEx("Root", ImGuiTreeNodeFlags_DefaultOpen))
            {
                std::vector<Node3D*>::iterator it = entities.begin();
                while (it != entities.end())
                {
                    if (show_tree_recursive(*it)) {
                        it = entities.erase(it);
                    }
                    else {
                        it++;
                    }
                }

                ImGui::TreePop();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Camera"))
        {
            static int camera_type = 0;

            if (ImGui::Combo("Camera Type", &camera_type, "FLYOVER\0ORBIT"))
            {
                set_camera_type(camera_type);
            }

            if (ImGui::BeginCombo("Look at", last_camera_target_name.c_str()))
            {
                for (int i = 0; i < cameras.size(); ++i)
                {
                    static bool sel = true;
                    if(ImGui::Selectable(cameras[i]->get_name().c_str(), &sel)) {
                        set_camera_lookat_index(i);
                    }
                }
                ImGui::EndCombo();
            }

            Camera* camera = renderer->get_camera();
            static float camera_speed = camera->get_speed();

            if (ImGui::SliderFloat("Speed", &camera_speed, 0.0f, 10.0f))
            {
                set_camera_speed(camera_speed);
            }

            if (ImGui::Button("Reset Camera"))
            {
                reset_camera();
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();

    ImGui::End();
}

bool SampleEngine::show_tree_recursive(Node3D* entity)
{
    std::vector<Node*>& children = entity->get_children();

    MeshInstance3D* entity_mesh = dynamic_cast<MeshInstance3D*>(entity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;

    if (!entity_mesh && children.empty() || (entity_mesh && children.empty() && entity_mesh->get_surfaces().empty())) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (ImGui::TreeNodeEx(entity->get_name().c_str(), flags))
    {
        if (ImGui::BeginPopupContextItem()) // <-- use last item id as popup id
        {
            if (ImGui::Button("Delete")) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                ImGui::TreePop();
                return true;
            }
            ImGui::EndPopup();
        }

        if (entity_mesh) {

            const std::vector<Surface*>& surfaces = entity_mesh->get_surfaces();

            for (int i = 0; i < surfaces.size(); ++i) {

                ImGui::TreeNodeEx(("Surface " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf);
                ImGui::TreePop();
            }
        }

        std::vector<Node*>::iterator it = children.begin();

        while (it != children.end())
        {
            Node3D* node_3d = static_cast<Node3D*>(*it);
            if (show_tree_recursive(node_3d)) {
                it = children.erase(it);
            }
            else {
                it++;
            }
        }

        ImGui::TreePop();
    }

    return false;
}

void SampleEngine::set_skybox_texture(const std::string& filename)
{
    skybox->set_texture(filename);
}

void SampleEngine::load_glb(const std::string& filename)
{
    entities.clear();

    // Add skybox again..
    entities.push_back(skybox);

    parse_scene(filename.c_str(), entities);

    // Each time we load entities, get the cameras!
    cameras.clear();
    for (auto entity : entities)
    {
        EntityCamera* new_camera = dynamic_cast<EntityCamera*>(entity);
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

    for (auto entity : entities)
    {
        EntityCamera* is_camera = dynamic_cast<EntityCamera*>(entity);
        // Get first non camera
        if (!is_camera) {
            camera->look_at_entity(entity);
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
