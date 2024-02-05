#pragma once

#include "engine.h"
#include <string>
#include <vector>

class Entity;
class EntityMesh;

class SampleEngine : public Engine {

    static std::vector<Entity*> entities;
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

    static void set_skybox_texture(const std::string& filename);
    static void load_glb(const std::string& filename);
    static void toggle_rotation();
};
