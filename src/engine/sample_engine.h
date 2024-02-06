#pragma once

#include "engine.h"

#include "framework/utils/utils.h"

#include <string>
#include <vector>

class Entity;
class EntityMesh;
class EntityCamera;

class SampleEngine : public Engine {

    static int target_camera_idx;
    static std::string last_camera_target_name;
    LerpedValue<glm::vec3> eye_lerp;

    static std::vector<Entity*> entities;
    static std::vector<EntityCamera*> cameras;
    static EntityMesh* skybox;
    static EntityMesh* grid;
    static bool rotate_scene;

    void render_gui();
    bool show_tree_recursive(Entity* entity);

public:

	int initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen) override;
    void clean() override;

	void update(float delta_time) override;
	void render() override;

    // Methods to use in web demonstrator
    static void set_skybox_texture(const std::string& filename);
    static void load_glb(const std::string& filename);
    static void toggle_rotation();
    static void set_camera_type(int camera_type);
    static void set_camera_lookat_index(int index);
    static std::vector<std::string> get_cameras_names();
};
