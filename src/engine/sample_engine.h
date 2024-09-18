#pragma once

#include "engine/engine.h"

#include "glm/glm.hpp"

#include "framework/math/math_utils.h"

#include <string>
#include <vector>

class EntityCamera;
class MeshInstance3D;

class SampleEngine : public Engine {

    static int target_camera_idx;
    static std::string last_camera_target_name;
    static LerpedValue<glm::vec3> eye_lerp;
    static LerpedValue<glm::vec3> center_lerp;

    static MeshInstance3D* skybox;
    static std::vector<EntityCamera*> cameras;
    static bool rotate_scene;

public:

    int initialize(Renderer* renderer, sEngineConfiguration configuration = {}) override;
    void clean() override;

    void update(float delta_time) override;
    void render() override;

    // Methods to use in web demonstrator
    void set_skybox_texture(const std::string& filename);
    void load_glb(const std::string& filename);
    void toggle_rotation();
    void set_camera_type(int camera_type);
    void set_camera_lookat_index(int index);
    void reset_camera();
    void set_camera_speed(float value);
    static std::vector<std::string> get_cameras_names();
};
