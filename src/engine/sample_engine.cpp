#include "sample_engine.h"

#include "framework/nodes/environment_3d.h"
#include "framework/nodes/camera.h"
#include "framework/nodes/mesh_instance_3d.h"
#include "framework/nodes/light_3d.h"
#include "framework/nodes/spot_light_3d.h"
#include "framework/nodes/omni_light_3d.h"
#include "framework/nodes/directional_light_3d.h"
#include "framework/parsers/parse_scene.h"
#include "framework/camera/camera.h"
#include "framework/camera/camera_3d.h"
#include "framework/input.h"

#include "graphics/sample_renderer.h"
#include "graphics/renderer_storage.h"
#include "graphics/texture.h"

#include "glm/gtx/quaternion.hpp"

#include "engine/scene.h"
#include "vpet/scene_distribution.h"

#include "spdlog/spdlog.h"

#ifndef __EMSCRIPTEN__
#include "zmq.h"
#endif

#include "vpet/structs.h"

#include "shaders/mesh_grid.wgsl.gen.h"
#include "shaders/mesh_forward.wgsl.gen.h"

sVPETContext vpet;

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
    //{
    //    MeshInstance3D* grid = new MeshInstance3D();
    //    grid->set_name("Grid");
    //    grid->add_surface(RendererStorage::get_surface("quad"));
    //    grid->set_position(glm::vec3(0.0f));
    //    grid->rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //    grid->scale(glm::vec3(10.f));
    //    grid->set_frustum_culling_enabled(false);

    //    // NOTE: first set the transparency and all types BEFORE loading the shader
    //    Material* grid_material = new Material();
    //    grid_material->set_transparency_type(ALPHA_BLEND);
    //    grid_material->set_cull_type(CULL_NONE);
    //    grid_material->set_type(MATERIAL_UNLIT);
    //    grid_material->set_shader(RendererStorage::get_shader_from_source(shaders::mesh_grid::source, shaders::mesh_grid::path, grid_material));
    //    grid->set_surface_material_override(grid->get_surface(0), grid_material);

    //    main_scene->add_node(grid);
    //}

    //load_glb("data/ContainerCity.glb");

#ifndef __EMSCRIPTEN__
    // VPET connection
    {
        context = zmq_ctx_new();

        {
            // Handles scene distribution
            distributor = zmq_socket(context, ZMQ_REP);
            int rc = zmq_bind(distributor, "tcp://127.0.0.1:5555");
            assert(rc == 0);

            // Handles scene updates
            subscriber = zmq_socket(context, ZMQ_SUB);
            rc = zmq_connect(subscriber, "tcp://127.0.0.1:5556");
            zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
            int opt_val = 1;
            zmq_setsockopt(subscriber, ZMQ_RCVTIMEO, &opt_val, sizeof(int));
        }

        // (Using WebSockets)
        //{
        //    // Vpet asking requesting scene
        //    distributor = zmq_socket(context, ZMQ_REQ);
        //    zmq_bind(distributor, "ws://127.0.0.1:5501");

        //    zmq_msg_t message;
        //    std::string data = "Requesting things!";
        //    zmq_send(distributor, data.data(), data.size(), 0);

        //    // DataHub sending updates
        //    publisher = zmq_socket(context, ZMQ_PUB);
        //    zmq_bind(publisher, "ws://127.0.0.1:5502");
        //}

        // To avoid blocking waiting for messages
        poller = zmq_poller_new();
    }
#endif

    return 0;
}

void SampleEngine::clean()
{
    Engine::clean();

#ifndef __EMSCRIPTEN__
    zmq_close(distributor);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
#endif

}

#ifndef __EMSCRIPTEN__


void SampleEngine::process_vpet_msg()
{
    // Check if any message is received
    zmq_pollitem_t item;
    item.socket = distributor;
    item.fd = 0;
    item.events = ZMQ_POLLIN;

    int rc = zmq_poll(&item, 1, 0);

    if (rc > 0 && item.revents != 0) {
        // If so check message type and send data
        spdlog::info("Messsage received...");

        char buffer[64];
        std::string buffer_str;
        int msg_size = zmq_recv(distributor, buffer, 64, 0);
        buffer_str.reserve(msg_size);
        buffer_str.assign(buffer, msg_size);

        uint8_t* byte_array = nullptr;

        spdlog::info("Requested: {}", buffer_str);

        uint32_t byte_array_size = get_scene_request_buffer(distributor, buffer_str, vpet, &byte_array);

        zmq_send(distributor, byte_array, byte_array_size, 0);

        if (byte_array) {
            delete byte_array;
        }
    }

    {
        zmq_msg_t message;
        zmq_msg_init(&message);
        zmq_msg_recv(&message, subscriber, 0);

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

            if (message_type == eVPETMessageType::PARAMETER_UPDATE) {
                while (buffer_ptr < msg_size) {

                    uint8_t scene_id = buffer[buffer_ptr];
                    buffer_ptr += sizeof(uint8_t);

                    uint16_t scene_object_id;
                    memcpy(&scene_object_id, &buffer[buffer_ptr], sizeof(uint16_t));
                    buffer_ptr += sizeof(uint16_t);

                    scene_object_id--;

                    uint16_t parameter_id;
                    memcpy(&parameter_id, &buffer[buffer_ptr], sizeof(uint16_t));
                    buffer_ptr += sizeof(uint16_t);

                    eVPETParameterType param_type = static_cast<eVPETParameterType>(buffer[buffer_ptr]);
                    buffer_ptr += sizeof(uint8_t);

                    uint32_t param_length = buffer[buffer_ptr];
                    buffer_ptr += sizeof(uint32_t);

                    assert(scene_object_id < vpet.node_list.size());

                    sVPETNode* vpet_node = vpet.editables_node_list[scene_object_id];
                    Node3D* node_ref = vpet_node->node_ref;

                    if (!node_ref) {
                        return;
                        zmq_msg_close(&message);
                    }

                    float f32;
                    glm::vec2 vector2;
                    glm::vec3 vector3;
                    glm::vec4 vector4;
                    glm::quat rotation;

                    switch (param_type)
                    {
                    case eVPETParameterType::FLOAT: {
                        memcpy(&f32, &buffer[buffer_ptr], sizeof(float));
                        buffer_ptr += sizeof(float);
                        break;
                    }
                    case eVPETParameterType::VECTOR2: {
                        memcpy(&vector2[0], &buffer[buffer_ptr], sizeof(glm::vec2));
                        //spdlog::info("Vec2: {}, {}", vector2.x, vector2.y);
                        buffer_ptr += sizeof(glm::vec2);
                        break;
                    }
                    case eVPETParameterType::VECTOR3: {
                        memcpy(&vector3[0], &buffer[buffer_ptr], sizeof(glm::vec3));
                        //spdlog::info("Vec3: {}, {}, {}", vector3.x, vector3.y, vector3.z);
                        buffer_ptr += sizeof(glm::vec3);
                        break;
                    }
                    case eVPETParameterType::COLOR: {
                        memcpy(&vector4[0], &buffer[buffer_ptr], sizeof(glm::vec4));
                        //spdlog::info("Color: {}, {}, {}, {}", vector4.x, vector4.y, vector4.z, vector4.w);
                        buffer_ptr += sizeof(glm::vec4);
                        break;
                    }
                    case eVPETParameterType::QUATERNION: {
                        memcpy(&rotation[0], &buffer[buffer_ptr], sizeof(glm::quat));
                        //spdlog::info("Rot: {}, {}, {}, {}", rotation.x, rotation.y, rotation.z, rotation.w);
                        buffer_ptr += sizeof(glm::quat);
                        break;
                    }
                    default:
                        assert(0);
                        break;
                    }

                    switch (parameter_id) {
                    case 0:
                        vector3.z = -vector3.z;
                        node_ref->set_position(vector3);
                        break;
                    case 1:
                        rotation.x = -rotation.x;
                        rotation.y = -rotation.y;
                        node_ref->set_rotation(rotation);
                        break;
                    case 2:
                        node_ref->set_scale(vector3);
                        break;
                    case 3:
                        if (vpet_node->node_type == eVPETNodeType::LIGHT) {
                            Light3D* light_ref = static_cast<Light3D*>(node_ref);
                            light_ref->set_color(vector4);
                        }
                        break;
                    case 4:
                        if (vpet_node->node_type == eVPETNodeType::LIGHT) {
                            Light3D* light_ref = static_cast<Light3D*>(node_ref);
                            light_ref->set_intensity(f32);
                        }
                        break;
                    case 5:
                        if (vpet_node->node_type == eVPETNodeType::LIGHT) {
                            Light3D* light_ref = static_cast<Light3D*>(node_ref);
                            light_ref->set_range(f32 * 2.0f);
                        }
                        break;
                    default:
                        assert(0);
                    }
                }
            }
        }

        zmq_msg_close(&message);
    }
}

#endif

void SampleEngine::update(float delta_time)
{
#ifndef __EMSCRIPTEN__
    process_vpet_msg();

    //// RECEIVE SCENE REQ DATA
    //{
    //    zmq_msg_t message;
    //    zmq_msg_init(&message);
    //    zmq_msg_recv(&message, distributor, 0);
    //    uint32_t msg_size = zmq_msg_size(&message);
    //    if (msg_size) {
    //        char* buffer = reinterpret_cast<char*>(zmq_msg_data(&message));
    //        std::string str(buffer, msg_size);
    //        spdlog::info(str);
    //    }
    //}

    //// SEND UPDATES
    //{
    //    std::string data = "Scene update!";
    //    zmq_send(publisher, data.data(), data.size(), 0);
    //}

#endif

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

#ifdef __EMSCRIPTEN__
extern "C" {
#endif
    void set_scene_objects(int8_t* byte_array, uint32_t array_size)
    {
        uint32_t buffer_ptr = 0u;

        while (buffer_ptr < array_size) {

            sVPETMesh* mesh = new sVPETMesh();

            uint32_t vertices_size = 0u;
            memcpy(&vertices_size, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            mesh->vertex_array.resize(vertices_size);
            memcpy(mesh->vertex_array.data(), &byte_array[buffer_ptr], vertices_size * sizeof(glm::vec3));
            buffer_ptr += vertices_size * sizeof(glm::vec3);

            uint32_t indices_size = 0u;
            memcpy(&indices_size, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            mesh->index_array.resize(indices_size);
            memcpy(mesh->index_array.data(), &byte_array[buffer_ptr], indices_size * sizeof(uint32_t));
            buffer_ptr += indices_size * sizeof(uint32_t);

            uint32_t normals_size = 0u;
            memcpy(&normals_size, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            mesh->normal_array.resize(normals_size);
            memcpy(mesh->normal_array.data(), &byte_array[buffer_ptr], normals_size * sizeof(glm::vec3));
            buffer_ptr += normals_size * sizeof(glm::vec3);

            uint32_t uvs_size = 0u;
            memcpy(&uvs_size, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            mesh->uv_array.resize(uvs_size);
            memcpy(mesh->uv_array.data(), &byte_array[buffer_ptr], uvs_size * sizeof(glm::vec2));
            buffer_ptr += uvs_size * sizeof(glm::vec2);

            uint32_t bone_weights_size = 0u;
            memcpy(&bone_weights_size, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            vpet.geo_list.push_back(mesh);
        }

        spdlog::info("Meshes: buffer {} array_size {}", buffer_ptr, array_size);

        assert(buffer_ptr == array_size);
    }

    void set_scene_textures(int8_t* byte_array, uint32_t array_size)
    {
        uint32_t buffer_ptr = 0;

        while (buffer_ptr < array_size) {

            sVPETTexture* texture = new sVPETTexture();

            memcpy(&texture->width, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&texture->height, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&texture->format, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            uint32_t texture_size = 0u;
            memcpy(&texture_size, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(texture->texture_data.data(), &byte_array[buffer_ptr], texture_size);
            buffer_ptr += texture_size;

            vpet.texture_list.push_back(texture);
        }

        spdlog::info("Textures: buffer {} array_size {}", buffer_ptr, array_size);


        assert(buffer_ptr == array_size);
    }

    void set_scene_materials(int8_t* byte_array, uint32_t array_size)
    {
        uint32_t buffer_ptr = 0;

        while (buffer_ptr < array_size) {

            sVPETMaterial* material = new sVPETMaterial();

            spdlog::info("NEW MATERIAL!");

            memcpy(&material->type, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            spdlog::info("B_PTR{} type {}", buffer_ptr, material->type);

            memcpy(&material->name_size, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            spdlog::info("B_PTR{} name_size {}", buffer_ptr, material->name_size);

            memcpy(&material->name, &byte_array[buffer_ptr], material->name_size);
            buffer_ptr += material->name_size;
            spdlog::info("B_PTR{} name {}", buffer_ptr, material->name);

            memcpy(&material->src_size, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            spdlog::info("B_PTR{} src_size {}", buffer_ptr, material->src_size);

            memcpy(&material->src, &byte_array[buffer_ptr], material->src_size);
            buffer_ptr += material->src_size;
            spdlog::info("B_PTR{} src {}", buffer_ptr, material->src);

            material->material_id = vpet.material_list.size();// *reinterpret_cast<int32_t*>(buffer_ptr);
            // memcpy(&material->material_id, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            spdlog::info("B_PTR{} material_id {}", buffer_ptr, material->material_id);

            memcpy(&material->texture_ids_size, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            spdlog::info("B_PTR{} texture_ids_size {}", buffer_ptr, material->texture_ids_size);

            material->texture_id.resize(material->texture_ids_size);
            material->texture_offset.resize(material->texture_ids_size);
            material->texture_scale.resize(material->texture_ids_size);

            for (uint32_t i = 0; i < material->texture_ids_size; ++i) {
                memcpy(&material->texture_id[i], &byte_array[buffer_ptr], sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);
                spdlog::info("B_PTR{} texture_id {}", buffer_ptr, material->texture_id[i]);

                memcpy(&material->texture_offset[i], &byte_array[buffer_ptr], sizeof(glm::vec2));
                buffer_ptr += sizeof(glm::vec2);
                spdlog::info("B_PTR{} texture_offset {} {}", buffer_ptr, material->texture_offset[i].x, material->texture_offset[i].y);

                memcpy(&material->texture_scale[i], &byte_array[buffer_ptr], sizeof(glm::vec2));
                buffer_ptr += sizeof(glm::vec2);
                spdlog::info("B_PTR{} texture_scale {} {}", buffer_ptr, material->texture_scale[i].x, material->texture_scale[i].y);
            }

           /* spdlog::info("{} {} {}", material->name, material->material_id, material->texture_id);
            spdlog::info("buffer {} array_size {}", buffer_ptr, array_size);*/

            vpet.material_list.push_back(material);

            return;
        }

        if (buffer_ptr != array_size) {
            spdlog::info("DIFFERENT!! buffer {} array_size {}", buffer_ptr, array_size);
        }


        assert(buffer_ptr == array_size);
    }

    void set_scene_nodes(int8_t* byte_array, uint32_t array_size)
    {
        uint32_t buffer_ptr = 0;

        while (buffer_ptr < array_size) {

            sVPETNode* node = new sVPETNode();

            memcpy(&node->node_type, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&node->editable, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&node->child_count, &byte_array[buffer_ptr], sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            glm::vec3 transformed_pos;
            memcpy(&transformed_pos, &byte_array[buffer_ptr], sizeof(glm::vec3));
            // Transform to unity coordinate system
            transformed_pos.z = -transformed_pos.z;
            node->position = transformed_pos;
            buffer_ptr += sizeof(glm::vec3);

            memcpy(&node->scale, &byte_array[buffer_ptr], sizeof(glm::vec3));
            buffer_ptr += sizeof(glm::vec3);

            glm::quat transformed_rot;
            memcpy(&transformed_rot, &byte_array[buffer_ptr], sizeof(glm::quat));
            // Transform to unity coordinate system
            transformed_rot.x = -transformed_rot.x;
            transformed_rot.y = -transformed_rot.y;
            node->rotation = transformed_pos;
            buffer_ptr += sizeof(glm::quat);

            memcpy(&node->name, &byte_array[buffer_ptr], 64);
            buffer_ptr += 64;

            switch (node->node_type)
            {
            case eVPETNodeType::GEO: {

                sVPETGeoNode* geo_node = static_cast<sVPETGeoNode*>(node);

                memcpy(&geo_node->geo_id, &byte_array[buffer_ptr], sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&geo_node->material_id, &byte_array[buffer_ptr], sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&geo_node->color, &byte_array[buffer_ptr], sizeof(glm::vec4));
                buffer_ptr += sizeof(glm::vec4);

                break;
            }
            case eVPETNodeType::LIGHT: {

                sVPETLightNode* light_node = static_cast<sVPETLightNode*>(node);

                memcpy(&light_node->light_type, &byte_array[buffer_ptr], sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&light_node->intensity, &byte_array[buffer_ptr], sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&light_node->angle, &byte_array[buffer_ptr], sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&light_node->range, &byte_array[buffer_ptr], sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&light_node->color, &byte_array[buffer_ptr], sizeof(glm::vec3));
                buffer_ptr += sizeof(glm::vec3);

                break;
            }
            case eVPETNodeType::CAMERA: {

                sVPETCamNode* camera_node = static_cast<sVPETCamNode*>(node);

                memcpy(&camera_node->fov, &byte_array[buffer_ptr], sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&camera_node->aspect, &byte_array[buffer_ptr], sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&camera_node->near, &byte_array[buffer_ptr], sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&camera_node->far, &byte_array[buffer_ptr], sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&camera_node->focal_dist, &byte_array[buffer_ptr], sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&camera_node->aperture, &byte_array[buffer_ptr], sizeof(float));
                buffer_ptr += sizeof(float);

                break;
            }
            default:
                if (node->node_type != eVPETNodeType::GROUP) {
                    assert(0);
                }
                break;
            }

            vpet.node_list.push_back(node);

            if (node->editable) {
                vpet.editables_node_list.push_back(node);
            }
        }

        spdlog::info("Nodes: buffer {} array_size {}", buffer_ptr, array_size);

        assert(buffer_ptr == array_size);
    }

#ifdef __EMSCRIPTEN__
}
#endif

void SampleEngine::load_tracer_scene()
{
    struct sParentStack {
        Node3D* parent_node = nullptr;
        uint32_t children_counter = 0;
        uint32_t children_size = 0;
    };

    std::vector<sParentStack> parent_stack;

    spdlog::info("VPET NODES: {}", vpet.node_list.size());

    for (sVPETNode* vpet_node : vpet.node_list) {

        spdlog::info("Node {} of type {}:", vpet_node->name, static_cast<uint32_t>(vpet_node->node_type));

        Node3D* engine_node = nullptr;

        switch (vpet_node->node_type) {
        case eVPETNodeType::GROUP: {
            engine_node = new Node3D();
            break;
        }
        case eVPETNodeType::GEO: {
            sVPETGeoNode* vpet_geo = static_cast<sVPETGeoNode*>(vpet_node);
            sVPETMesh* vpet_mesh = vpet.geo_list[vpet_geo->geo_id];

            MeshInstance3D* mesh_instance = new MeshInstance3D();
            Surface* surface = new Surface();

            std::vector<sInterleavedData> interleaved;
            interleaved.resize(vpet_mesh->vertex_array.size());

            for (uint32_t i = 0u; i < interleaved.size(); ++i) {
                interleaved[i].position = vpet_mesh->vertex_array[i];
                interleaved[i].position.z = -interleaved[i].position.z;
                interleaved[i].normal = vpet_mesh->normal_array[i];
                interleaved[i].normal.z = -interleaved[i].normal.z;
                interleaved[i].uv = vpet_mesh->uv_array[i];
            }

            surface->create_vertex_buffer(interleaved);

            std::vector<uint32_t> indices;
            indices.resize(vpet_mesh->index_array.size());
            uint32_t add_idx = 0;
            for (uint32_t idx = vpet_mesh->index_array.size(); idx > 0; --idx) {
                indices[add_idx] = vpet_mesh->index_array[idx - 1];
                add_idx++;
            }

            surface->create_index_buffer(indices);
            mesh_instance->add_surface(surface);

            // Material
            {
                sVPETMaterial* vpet_material = vpet.material_list[vpet_geo->material_id];

                Material* geo_material = new Material();
                geo_material->set_name(vpet_material->name);

                // Texture
                {
                    /*sVPETTexture* vpet_texture = vpet.texture_list[vpet_material->texture_id];
                    if (vpet_texture) {
                        Texture* texture = new Texture();
                        texture->set_name(vpet_texture->name);
                        texture->load_from_data(vpet_texture->name, WGPUTextureDimension_2D, vpet_texture->width, vpet_texture->height, 1u, &vpet_texture->texture_data);
                        geo_material->set_diffuse_texture(texture);
                    }*/
                }

                geo_material->set_shader(RendererStorage::get_shader_from_source(shaders::mesh_forward::source, shaders::mesh_forward::path, geo_material));
                mesh_instance->set_surface_material_override(surface, geo_material);
            }

            engine_node = mesh_instance;
            break;
        }
        case eVPETNodeType::CAMERA: {
            EntityCamera* engine_camera = new EntityCamera();
            sVPETCamNode* vpet_camera = static_cast<sVPETCamNode*>(vpet_node);
            engine_camera->set_perspective(vpet_camera->fov, vpet_camera->aspect, vpet_camera->near, vpet_camera->far);
            engine_node = engine_camera;
            break;
        }
        case eVPETNodeType::LIGHT: {

            sVPETLightNode* vpet_light = static_cast<sVPETLightNode*>(vpet_node);

            if (vpet_light->light_type == eVPETLightType::SPOT) {
                SpotLight3D* spot_entity = new SpotLight3D();
                spot_entity->set_inner_cone_angle(vpet_light->angle);
                spot_entity->set_outer_cone_angle(vpet_light->angle);
                engine_node = spot_entity;
            } else
            if (vpet_light->light_type == eVPETLightType::DIRECTIONAL) {
                DirectionalLight3D* directional_entity = new DirectionalLight3D();
                engine_node = directional_entity;
            } else
            if (vpet_light->light_type == eVPETLightType::POINT) {
                OmniLight3D* point_entity = new OmniLight3D();
                engine_node = point_entity;
            }
            else {
                spdlog::error("unsupported tracer light type");
                assert(0);
            }

            Light3D* light_node = static_cast<Light3D*>(engine_node);
            light_node->set_intensity(vpet_light->intensity);
            light_node->set_color(vpet_light->color);
            light_node->set_range(vpet_light->range * 2.0f);

            break;
        }
        default: {
            spdlog::error("unsupported tracer node type");
            assert(0);
            continue;
        }
        }

        engine_node->set_position(vpet_node->position);
        engine_node->set_rotation(vpet_node->rotation);
        engine_node->set_scale(vpet_node->scale);

        engine_node->set_name(vpet_node->name);

        sParentStack* parent = nullptr;
        if (!parent_stack.empty()) {
            sParentStack& parent = parent_stack.back();

            parent.children_counter++;
            parent.parent_node->add_child(engine_node);

            if (parent.children_counter == parent.children_size) {
                parent_stack.pop_back();
            }
        }
        else {
            main_scene->add_node(engine_node);
        }

        if (vpet_node->child_count > 0) {
            parent_stack.push_back({ engine_node, 0, vpet_node->child_count });
        }
    }

    spdlog::info("Tracer scene loaded!");
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

    vpet.clean();

    std::function<void(Node*)> recurse_tree = [&](Node* node) {
        EntityCamera* new_camera = dynamic_cast<EntityCamera*>(node);
        if (new_camera) {
            cameras.push_back(new_camera);
        }

        process_scene_object(vpet, node);

        if (!node->get_children().empty()) {
            for (auto child : node->get_children()) {
                recurse_tree(child);
            }
        }
    };

    if (main_scene->get_nodes().empty()) {
        return {};
    }

    // Each time we load entities, get vpet nodes and the cameras
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
