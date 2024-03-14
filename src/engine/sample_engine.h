#pragma once

#include "engine.h"

#include "framework/utils/utils.h"

#include <string>
#include <vector>

class Node3D;
class MeshInstance3D;
class EntityCamera;
class Environment3D;

class SampleEngine : public Engine {

    static int target_camera_idx;
    static std::string last_camera_target_name;
    static LerpedValue<glm::vec3> eye_lerp;
    static LerpedValue<glm::vec3> center_lerp;

    static std::vector<Node3D*> entities;
    static std::vector<EntityCamera*> cameras;
    static Environment3D* skybox;
    static bool rotate_scene;

    void render_gui();
    bool show_tree_recursive(Node3D* entity);

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
    static void reset_camera();
    static std::vector<std::string> get_cameras_names();
};
