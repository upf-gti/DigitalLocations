#include "engine/sample_engine.h"
#include "graphics/sample_renderer.h"

#include "spdlog/spdlog.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/bind.h>

#include "vpet/scene_distribution.h"

// Binding code
EMSCRIPTEN_BINDINGS(_Class_) {

    emscripten::class_<sVPETContext>("sVPETContext");

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
        .function("toggleSceneRotation", &SampleEngine::toggle_rotation)
        .function("getVPETContext", &SampleEngine::get_vpet_context);

    emscripten::register_vector<std::string>("vector<string>");
}

EMSCRIPTEN_BINDINGS(exported_funcs) {
    emscripten::function("get_scene_request_buffer", &get_scene_request_buffer, emscripten::allow_raw_pointers());
}

#endif

int main()
{
    SampleEngine* engine = new SampleEngine();
    SampleRenderer* renderer = new SampleRenderer();

    sEngineConfiguration configuration = {
        .window_width = 1600,
        .window_height = 900,
        .window_title = "Digital Locations",
        .camera_type = CAMERA_ORBIT,
        .msaa_count = 4
    };

    if (engine->initialize(renderer, configuration)) {
        return 1;
    }

    engine->start_loop();

    engine->clean();

    delete engine;

    delete renderer;

    return 0;
}
