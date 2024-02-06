#include "sample_engine.h"
#include "framework/entities/entity_mesh.h"
#include "framework/entities/entity_text.h"
#include "framework/entities/entity_camera.h"
#include "framework/input.h"
#include "framework/scene/parse_scene.h"
#include "framework/scene/parse_gltf.h"
#include "graphics/sample_renderer.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"

#include "framework/utils/tinyfiledialogs.h"

#include "spdlog/spdlog.h"

EntityMesh* SampleEngine::skybox = nullptr;
EntityMesh* SampleEngine::grid = nullptr;
std::vector<Entity*> SampleEngine::entities;
std::vector<EntityCamera*> SampleEngine::cameras;
bool SampleEngine::rotate_scene = false;
int SampleEngine::target_camera_idx = -1;
std::string SampleEngine::last_camera_target_name = "Current";

int SampleEngine::initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen)
{
	int error = Engine::initialize(renderer, window, use_glfw, use_mirror_screen);

    std::string environment = "data/textures/environments/sky.hdre";

    // Create skybox

    {
        skybox = parse_mesh("data/meshes/cube.obj");
        skybox->set_surface_material_shader(0, RendererStorage::get_shader("data/shaders/mesh_texture_cube.wgsl"));
        skybox->set_surface_material_diffuse(0, Renderer::instance->get_irradiance_texture());
        skybox->scale(glm::vec3(100.f));
        skybox->set_surface_material_priority(0, 2);
    }

    // Create grid
    {
        grid = new EntityMesh();
        grid->add_surface(RendererStorage::get_surface("quad"));
        grid->set_translation(glm::vec3(0.0f));
        grid->rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        grid->scale(glm::vec3(3.f));

        Material grid_material;
        grid_material.shader = RendererStorage::get_shader("data/shaders/mesh_grid.wgsl");
        grid_material.flags |= MATERIAL_TRANSPARENT;

        grid->set_surface_material_override(grid->get_surface(0), grid_material);
    }

    EntityMesh* cube = parse_mesh("data/meshes/cube/cube.obj");
    cube->scale(glm::vec3(0.1f));
    entities.push_back(cube);

    load_glb("data/scenes/Cameras.gltf");

	return error;
}

void SampleEngine::clean()
{
    Engine::clean();

    if (grid) delete grid;
}

void SampleEngine::update(float delta_time)
{
    Engine::update(delta_time);

    if (rotate_scene)
        for (auto e : entities) e->rotate(delta_time, normals::pY);

    SampleRenderer* renderer = static_cast<SampleRenderer*>(SampleRenderer::instance);
    skybox->set_translation(renderer->get_camera_eye());

    // Interpolate current camera position
    if (target_camera_idx == -1)
        return;

    Camera* camera = renderer->get_camera();

    glm::vec3 new_eye = cameras[target_camera_idx]->get_translation();
    eye_lerp.value = smooth_damp(eye_lerp.value, new_eye, &eye_lerp.velocity, 0.05f, 10.0f, delta_time);

    camera->look_at(eye_lerp.value, camera->get_center(), camera->get_up());

    if (glm::distance(eye_lerp.value, new_eye) < FLT_EPSILON)
    {
        target_camera_idx = -1;
    }
}

void SampleEngine::render()
{
#ifndef __EMSCRIPTEN__
    render_gui();
#endif

    skybox->render();

	for (auto entity : entities) {
		entity->render();
	}

    grid->render();

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
                    parse_scene(open_file_name, entities);
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
                std::vector<Entity*>::iterator it = entities.begin();
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
                        target_camera_idx = i;
                        last_camera_target_name = cameras[i]->get_name();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();

    ImGui::End();
}

bool SampleEngine::show_tree_recursive(Entity* entity)
{
    std::vector<Entity*>& children = entity->get_children();

    EntityMesh* entity_mesh = dynamic_cast<EntityMesh*>(entity);

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

        std::vector<Entity*>::iterator it = children.begin();

        while (it != children.end())
        {
            if (show_tree_recursive(*it)) {
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
    Texture* tex = RendererStorage::get_texture(filename);
    skybox->set_surface_material_diffuse(0, tex);
}

void SampleEngine::load_glb(const std::string& filename)
{
    // TODO: We should destroy entities...
    entities.clear();
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

void SampleEngine::set_camera_lookat_index(int index)
{
    if (index < 0 || index >= cameras.size())
        return;

    target_camera_idx = index;
    last_camera_target_name = cameras[index]->get_name();
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
