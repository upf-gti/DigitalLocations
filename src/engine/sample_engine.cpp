#include "sample_engine.h"

#include "framework/nodes/environment_3d.h"
#include "framework/nodes/camera.h"
#include "framework/nodes/mesh_instance_3d.h"
#include "framework/parsers/parse_scene.h"
#include "framework/camera/camera.h"
#include "framework/camera/camera_3d.h"
#include "framework/input.h"

#include "graphics/sample_renderer.h"
#include "graphics/renderer_storage.h"
#include "graphics/texture.h"

#include "glm/gtx/quaternion.hpp"

#include "engine/scene.h"

#include "spdlog/spdlog.h"

#include "zmq.h"

#include "shaders/mesh_grid.wgsl.gen.h"

int SampleEngine::initialize(Renderer* renderer, sEngineConfiguration configuration)
{
    int error = Engine::initialize(renderer, configuration);

    if (error) return error;

    main_scene = new Scene("main_scene");

	return error;
}

int SampleEngine::post_initialize()
{
    // Create skybox
    {
        skybox = new Environment3D();
        MeshInstance3D* skybox = new Environment3D();
        main_scene->add_node(skybox);
    }

    // Create grid
    {
        MeshInstance3D* grid = new MeshInstance3D();
        grid->set_name("Grid");
        grid->add_surface(RendererStorage::get_surface("quad"));
        grid->set_position(glm::vec3(0.0f));
        grid->rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        grid->scale(glm::vec3(10.f));
        grid->set_frustum_culling_enabled(false);

        // NOTE: first set the transparency and all types BEFORE loading the shader
        Material* grid_material = new Material();
        grid_material->set_transparency_type(ALPHA_BLEND);
        grid_material->set_cull_type(CULL_NONE);
        grid_material->set_type(MATERIAL_UNLIT);
        grid_material->set_shader(RendererStorage::get_shader_from_source(shaders::mesh_grid::source, shaders::mesh_grid::path, grid_material));
        grid->set_surface_material_override(grid->get_surface(0), grid_material);

        main_scene->add_node(grid);
    }

    //load_glb("data/ContainerCity.glb");

    // VPET connection
    {
        vpet.context = zmq_ctx_new();

        // Handles scene distribution
        vpet.distributor = zmq_socket(vpet.context, ZMQ_REP);
        int rc = zmq_bind(vpet.distributor, "tcp://127.0.0.1:5565");
        assert(rc == 0);

        // Handles scene updates
        vpet.subscriber = zmq_socket(vpet.context, ZMQ_SUB);
        rc = zmq_connect(vpet.subscriber, "tcp://127.0.0.1:5556");
        zmq_setsockopt(vpet.subscriber, ZMQ_SUBSCRIBE, "", 0);
        int opt_val = 1;
        zmq_setsockopt(vpet.subscriber, ZMQ_RCVTIMEO, &opt_val, sizeof(int));

        assert(rc == 0);

        // To avoid blocking waiting for messages
        vpet.poller = zmq_poller_new();
    }

    return 0;
}

void SampleEngine::clean()
{
    Engine::clean();

    zmq_close(vpet.distributor);
    zmq_close(vpet.subscriber);
    zmq_ctx_destroy(vpet.context);
}

void SampleEngine::process_vpet_msg()
{
    // Check if any message is received
    zmq_pollitem_t item;
    item.socket = vpet.distributor;
    item.fd = 0;
    item.events = ZMQ_POLLIN;

    int rc = zmq_poll(&item, 1, 0);

    if (rc > 0 && item.revents != 0) {
        // If so check message type and send data
        spdlog::info("Messsage received...");

        char buffer[64];
        std::string buffer_str;
        int msg_size = zmq_recv(vpet.distributor, buffer, 64, 0);
        buffer_str.reserve(msg_size);
        buffer_str.assign(buffer, msg_size);

        spdlog::info("Requested: {}", buffer_str);

        if (buffer_str == "header") {
            sVPETHeader header = {};
            zmq_send(vpet.distributor, &header, sizeof(sVPETHeader), 0);
        } else
        if (buffer_str == "materials") {
            zmq_send(vpet.distributor, nullptr, 0, 0);
        } else
        if (buffer_str == "textures") {

            for (const sVPETTexture& vpet_texture : vpet.textures) {


            }

            zmq_send(vpet.distributor, vpet.textures.data(), vpet.textures.size() * sizeof(sVPETTexture), 0);
        } else
        if (buffer_str == "objects") { // meshes
            zmq_send(vpet.distributor, nullptr, 0, 0);
        } else
        if (buffer_str == "nodes") {
            zmq_send(vpet.distributor, nullptr, 0, 0);
        } else
        if (buffer_str == "characters") {
            zmq_send(vpet.distributor, nullptr, 0, 0);
        }
    }

    {
        zmq_msg_t message;
        zmq_msg_init(&message);
        zmq_msg_recv(&message, vpet.subscriber, 0);

        uint32_t msg_size = zmq_msg_size(&message);
        uint8_t* buffer = reinterpret_cast<uint8_t*>(zmq_msg_data(&message));

        if (msg_size > 0) {

            uint32_t buffer_ptr = 0;

            uint8_t client_id = buffer[buffer_ptr];
            buffer_ptr += sizeof(uint8_t);

            uint8_t time = buffer[buffer_ptr];
            buffer_ptr += sizeof(uint8_t);

            eVPETMessageType message_type = static_cast<eVPETMessageType>(buffer[buffer_ptr]);
            buffer_ptr += sizeof(uint8_t);

            //switch (message_type) {
            //case eVPETMessageType::PARAMETER_UPDATE:
            //    spdlog::info("MSG: PARAM UPDATE");
            //    break;
            //case eVPETMessageType::SYNC:
            //    spdlog::info("MSG: SYNC");
            //    break;
            //default:
            //    spdlog::info("MSG: {}", static_cast<uint8_t>(message_type));
            //}

            while (buffer_ptr < msg_size) {

                uint8_t scene_id = buffer[buffer_ptr];
                buffer_ptr += sizeof(uint8_t);

                uint16_t scene_object_id;
                memcpy(&scene_object_id, &buffer[buffer_ptr], sizeof(uint16_t));
                buffer_ptr += sizeof(uint16_t);

                uint16_t parameter_id;
                memcpy(&parameter_id, &buffer[buffer_ptr], sizeof(uint16_t));
                buffer_ptr += sizeof(uint16_t);

                eVPETParameterType param_type = static_cast<eVPETParameterType>(buffer[buffer_ptr]);
                buffer_ptr += sizeof(uint8_t);

                uint32_t param_length = buffer[buffer_ptr];
                buffer_ptr += sizeof(uint32_t);

                switch (param_type)
                {
                case eVPETParameterType::VECTOR2: {
                    glm::vec2 vector2;
                    memcpy(&vector2[0], &buffer[buffer_ptr], sizeof(glm::vec2));
                    //spdlog::info("Vec2: {}, {}", vector2.x, vector2.y);
                    buffer_ptr += sizeof(glm::vec2);
                    break;
                }
                case eVPETParameterType::VECTOR3: {
                    glm::vec3 vector3;
                    memcpy(&vector3[0], &buffer[buffer_ptr], sizeof(glm::vec3));
                    //spdlog::info("Vec3: {}, {}, {}", vector3.x, vector3.y, vector3.z);
                    buffer_ptr += sizeof(glm::vec3);
                    break;
                }
                case eVPETParameterType::QUATERNION: {
                    glm::quat rotation;
                    memcpy(&rotation[0], &buffer[buffer_ptr], sizeof(glm::quat));
                    //spdlog::info("Rot: {}, {}, {}, {}", rotation.x, rotation.y, rotation.z, rotation.w);
                    buffer_ptr += sizeof(glm::quat);
                    break;
                }
                default:
                    break;
                }
            }
        }

        zmq_msg_close(&message);
    }
}

void SampleEngine::update(float delta_time)
{
    process_vpet_msg();

    std::vector<Node*>& scene_nodes = main_scene->get_nodes();

    if (rotate_scene) {
        for (auto node : main_scene->get_nodes()) {
            MeshInstance3D* mesh_node = dynamic_cast<MeshInstance3D*>(node);
            if (mesh_node) {
                mesh_node->rotate(delta_time, normals::pY);
            }
        }
    }

    Engine::update(delta_time);

    main_scene->update(delta_time);
    skybox->update(delta_time);

    //if (Input::was_key_pressed(GLFW_KEY_O)) {
    //    set_camera_type(CAMERA_ORBIT);
    //}

    //if (Input::was_key_pressed(GLFW_KEY_F)) {
    //    set_camera_type(CAMERA_FLYOVER);
    //}

    // Interpolate current camera position
    if (target_camera_idx != -1)
    {
        // Lerp eye
        glm::vec3 new_eye = cameras[target_camera_idx]->get_translation();
        eye_lerp.value = smooth_damp(eye_lerp.value, new_eye, &eye_lerp.velocity, 0.50f, 20.0f, delta_time);

        Camera* camera = renderer->get_camera();
        camera->look_at(eye_lerp.value, camera->get_center(), camera->get_up());

        if (glm::distance(eye_lerp.value, new_eye) < 0.01f)
        {
            target_camera_idx = -1;
        }
    }

    if (lerp_center) {
        // Lerp center
        center_lerp.value = smooth_damp(center_lerp.value, target_center, &center_lerp.velocity, 0.25f, 20.0f, delta_time);

        Camera* camera = renderer->get_camera();
        camera->look_at(camera->get_eye(), center_lerp.value, camera->get_up());

        if (glm::distance(center_lerp.value, target_center) < 0.01f)
        {
            lerp_center = false;
        }
    }
}

void SampleEngine::render()
{
#ifndef __EMSCRIPTEN__
    render_default_gui();
#endif

    skybox->render();
    main_scene->render();

	Engine::render();
}

void SampleEngine::set_skybox_texture(const std::string& filename)
{
    if (!skybox) {
        return;
    }

    Texture* new_skybox = RendererStorage::get_texture(filename);
    skybox->get_surface(0)->get_material()->set_diffuse_texture(new_skybox);
}

std::vector<std::string> SampleEngine::load_glb(const std::string& filename)
{
    main_scene->delete_all();

    std::vector<Node*> entities;
    parse_scene(filename.c_str(), entities, true);

    main_scene->add_nodes(entities);

    cameras.clear();

    std::function<void(Node*)> recurse_tree = [&](Node* node) {
        EntityCamera* new_camera = dynamic_cast<EntityCamera*>(node);
        if (new_camera) {
            cameras.push_back(new_camera);
        }

        MeshInstance3D* mesh_instance = dynamic_cast<MeshInstance3D*>(node);
        if (mesh_instance) {
            for (Surface* surface : mesh_instance->get_surfaces()) {
                Texture* diffuse_texture = surface->get_material()->get_diffuse_texture();

                if (diffuse_texture) {
                    //vpet.textures.push_back({
                    //    diffuse_texture->get_name(),
                    //    diffuse_texture->get_width() * diffuse_texture->get_height(),
                    //    diffuse_texture->get_texture_data()
                    //});
                }
            }
        }

        if (!node->get_children().empty()) {
            for (auto child : node->get_children()) {
                recurse_tree(child);
            }
        }
    };

    // Each time we load entities, get the cameras!
    for (auto node : main_scene->get_nodes()) {
        recurse_tree(node);
    }

    reset_camera();

    if (!cameras.empty()) {
        set_camera_lookat_index(0);
    }

    return get_cameras_names();
}

void SampleEngine::load_ply(const std::string& filename)
{
    main_scene->delete_all();

    std::vector<Node*> entities;
    parse_scene(filename.c_str(), entities);

    main_scene->add_nodes(entities);

    cameras.clear();
}

void SampleEngine::toggle_rotation()
{
    rotate_scene = !rotate_scene;
}

void SampleEngine::set_camera_type(int camera_type)
{
    SampleRenderer* renderer = static_cast<SampleRenderer*>(SampleRenderer::instance);

    if (camera_type == CAMERA_ORBIT) {
        //renderer->get_camera()->set_center(glm::vec3(target_center));
        center_lerp.value = renderer->get_camera()->get_center();
        lerp_center = true;
    }

    renderer->set_camera_type(static_cast<eCameraType>(camera_type));
}

void SampleEngine::reset_camera()
{
    SampleRenderer* renderer = static_cast<SampleRenderer*>(SampleRenderer::instance);
    Camera3D* camera = static_cast<Camera3D*>(renderer->get_camera());

    for (auto node : main_scene->get_nodes())
    {
        EntityCamera* is_camera = dynamic_cast<EntityCamera*>(node);
        // Get first non camera
        if (!is_camera) {
            Node3D* mesh_node = dynamic_cast<Node3D*>(node);
            if (mesh_node)
            {
                camera->look_at_entity(mesh_node);
                target_center = mesh_node->get_aabb().center;
                lerp_center = true;
                break;
            }
        }
    }
}

void SampleEngine::set_camera_speed(float value)
{
    Camera* camera = Renderer::instance->get_camera();
    camera->set_speed(value);
}

void SampleEngine::set_camera_lookat_index(int index)
{
    if (index < 0 || index >= cameras.size())
        return;

    target_camera_idx = index;
    last_camera_target_name = cameras[index]->get_name();

    SampleRenderer* renderer = static_cast<SampleRenderer*>(SampleRenderer::instance);
    Camera* camera = renderer->get_camera();

    eye_lerp.value = camera->get_eye();
    center_lerp.value = camera->get_center();
}

std::vector<std::string> SampleEngine::get_cameras_names()
{
    std::vector<std::string> names;
    names.resize(cameras.size());

    for (int i = 0; i < cameras.size(); ++i)
    {
        names[i] = cameras[i]->get_name();
    }

    return names;
}
