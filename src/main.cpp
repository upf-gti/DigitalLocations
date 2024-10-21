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
        .function("setEnvironment", &SampleEngine::set_skybox_texture)
        .function("loadGLB", &SampleEngine::load_glb)
        .function("loadPly", &SampleEngine::load_ply)
        .function("setCameraType", &SampleEngine::set_camera_type)
        .class_function("getInstance", &SampleEngine::get_sample_instance, emscripten::return_value_policy::reference())
        .function("setCameraLookAtIndex", &SampleEngine::set_camera_lookat_index)
        .function("setCameraSpeed", &SampleEngine::set_camera_speed)
        .function("resetCamera", &SampleEngine::reset_camera)
        .function("toggleSceneRotation", &SampleEngine::toggle_rotation);

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

    sEngineConfiguration configuration;
    configuration.window_width = 1600;
    configuration.window_height = 900;
    configuration.camera_type = CAMERA_ORBIT;
    configuration.msaa_count = 4;
    configuration.window_title = "Digital Locations";

    if (engine->initialize(renderer, configuration)) {
        return 1;
    }

    engine->start_loop();

    engine->clean();

    delete engine;
    delete renderer;

    return 0;
}
