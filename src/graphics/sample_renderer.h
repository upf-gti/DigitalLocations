#pragma once

#include "includes.h"

#include "graphics/renderer.h"

enum {
    CAMERA_FLYOVER,
    CAMERA_ORBIT
};

class SampleRenderer : public Renderer {

public:

    SampleRenderer();

    int initialize(GLFWwindow* window, bool use_mirror_screen = false) override;
    void clean() override;

    void update(float delta_time) override;
    void render() override;

    void set_camera_type(int camera_type);
};
