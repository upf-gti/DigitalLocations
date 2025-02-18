#pragma once

#include "engine/engine.h"

#include "glm/glm.hpp"

#include "framework/math/math_utils.h"

#include <string>
#include <vector>

class EntityCamera;
class MeshInstance3D;
class Node3D;

class SampleEngine : public Engine {

    int target_camera_idx = -1;
    std::string last_camera_target_name = "Current";
    LerpedValue<glm::vec3> eye_lerp;
    LerpedValue<glm::vec3> center_lerp;

    glm::vec3 target_center = {};
    bool lerp_center = false;

    MeshInstance3D* skybox = nullptr;
    std::vector<EntityCamera*> cameras;
    bool rotate_scene = false;

    Node3D* scene_root = nullptr;

    float camera_interp_speed = 1.0f;

    // Vpet connection
    void* context;
    void* distributor; // to send scene
    void* subscriber; // to sync scene
    //void* publisher;
    void* poller; // to avoid blocking checking for messages

public:

    int initialize(Renderer* renderer, sEngineConfiguration configuration = {}) override;
    int post_initialize() override;

#ifndef __EMSCRIPTEN__
    void process_vpet_msg();
#endif

    void clean() override;

    static SampleEngine* get_sample_instance() { return static_cast<SampleEngine*>(instance); }

    void update(float delta_time) override;
    void render() override;

    void update_scene_parameter(uint32_t scene_object_id, uint16_t parameter_id, float vx, float vy, float vz, float vw);
    void load_tracer_scene();

    // Methods to use in web demonstrator
    void set_skybox_texture(const std::string& filename);
    std::vector<std::string> load_glb(const std::string& filename);
    void load_ply(const std::string& filename);
    void toggle_rotation();
    void set_camera_type(int camera_type);
    void set_camera_lookat_index(int index);
    void reset_camera();
    void set_camera_speed(float value);
    std::vector<std::string> get_cameras_names();

    // Methods to use in UHasselts gltf streaming demo
    void append_glb(const std::string& filename);
    void append_glb_data(int8_t* byte_array, uint32_t array_size);
    Camera* get_current_camera();
    void set_light_color(const std::string& light_name, float r, float g, float b);
    void set_light_intensity(const std::string& light_name, float intensity);
};
