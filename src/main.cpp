#include "engine/sample_engine.h"
#include "graphics/sample_renderer.h"

#include "spdlog/spdlog.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/bind.h>

// Binding code
EMSCRIPTEN_BINDINGS(_Class_) {
    emscripten::class_<SampleEngine>("Engine")
        .constructor<>()
        .class_function("setEnvironment", &SampleEngine::set_skybox_texture)
        .class_function("loadGLB", &SampleEngine::load_glb)
        .class_function("setCameraType", &SampleEngine::set_camera_type)
        .class_function("getCameraNames", &SampleEngine::get_cameras_names)
        .class_function("setCameraLookAtIndex", &SampleEngine::set_camera_lookat_index)
        .class_function("setCameraSpeed", &SampleEngine::set_camera_speed)
        .class_function("resetCamera", &SampleEngine::reset_camera)
        .class_function("toggleSceneRotation", &SampleEngine::toggle_rotation)
        .class_function("get_instance", &SampleEngine::get_instance);

    emscripten::register_vector<std::string>("vector<string>");
}
#endif

int main()
{
    SampleEngine* engine = new SampleEngine();
    SampleRenderer* renderer = new SampleRenderer();

    // change if necessary
    WGPURequiredLimits required_limits = {};
    required_limits.limits.maxVertexAttributes = 4;
    required_limits.limits.maxVertexBuffers = 1;
    required_limits.limits.maxBindGroups = 2;
    required_limits.limits.maxUniformBuffersPerShaderStage = 1;
    required_limits.limits.maxUniformBufferBindingSize = 65536;
    required_limits.limits.minUniformBufferOffsetAlignment = 256;
    required_limits.limits.minStorageBufferOffsetAlignment = 256;
    required_limits.limits.maxComputeInvocationsPerWorkgroup = 256;
    required_limits.limits.maxSamplersPerShaderStage = 1;
    required_limits.limits.maxDynamicUniformBuffersPerPipelineLayout = 1;

    renderer->set_required_limits(required_limits);

    if (engine->initialize(renderer)) {
        return 1;
    }

    engine->start_loop();

    engine->clean();

    delete engine;
    delete renderer;

    return 0;
}
