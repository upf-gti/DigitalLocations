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

    // UHasselt gltf streaming demo
    emscripten::class_<glm::vec3>("Vector3")
        .constructor<>()
        .constructor<float, float, float>()
        .property("x", &glm::vec3::x)
        .property("y", &glm::vec3::y)
        .property("z", &glm::vec3::z);

    emscripten::class_<Camera>("Camera")
        .constructor<>()
        .function("setSpeed", &Camera::set_speed)
        .function("setPosition", &Camera::set_eye)
        .function("setTarget", &Camera::set_center)
        .function("getPosition", &Camera::get_eye)
        .function("getTarget", &Camera::get_center)
        .function("setPerspective", &Camera::set_perspective)
        .function("setOrthographic", &Camera::set_orthographic)
        .function("lookAt", &Camera::look_at);
    // --------------------------------

    emscripten::class_<SampleEngine>("Engine")
        .constructor<>()
        .class_function("getInstance", &SampleEngine::get_sample_instance, emscripten::return_value_policy::reference())
        .function("setEnvironment", &SampleEngine::set_skybox_texture)
        .function("loadGLB", &SampleEngine::load_glb)
        .function("loadPly", &SampleEngine::load_ply)
        .function("setCameraType", &SampleEngine::set_camera_type)
        .function("setCameraLookAtIndex", &SampleEngine::set_camera_lookat_index)
        .function("setCameraSpeed", &SampleEngine::set_camera_speed)
        .function("resetCamera", &SampleEngine::reset_camera)
        .function("toggleSceneRotation", &SampleEngine::toggle_rotation)
      /*  .function("setSceneMeshes", &SampleEngine::set_scene_meshes, emscripten::allow_raw_pointers())
        .function("setSceneTextures", &SampleEngine::set_scene_textures, emscripten::allow_raw_pointers())
        .function("setSceneMaterials", &SampleEngine::set_scene_materials, emscripten::allow_raw_pointers())
        .function("setSceneNodes", &SampleEngine::set_scene_nodes, emscripten::allow_raw_pointers())*/
        //.function("getVPETContext", &SampleEngine::get_vpet_context);
        .function("loadTracerScene", &SampleEngine::load_tracer_scene)
        .function("updateSceneParameter", &SampleEngine::update_scene_parameter)
        // UHasselt gltf streaming demo
        .function("appendGLB", &SampleEngine::append_glb)
        .function("getCamera", &SampleEngine::get_current_camera, emscripten::return_value_policy::reference())
        .function("setLightColor", &SampleEngine::set_light_color)
        .function("setLightIntensity", &SampleEngine::set_light_intensity);

    emscripten::register_vector<float>("vector<float>");
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
