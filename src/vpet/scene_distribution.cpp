#include "scene_distribution.h"

#include "framework/nodes/mesh_instance_3d.h"
#include "framework/nodes/light_3d.h"
#include "framework/nodes/spot_light_3d.h"
#include "framework/nodes/camera.h"

#include "zmq.h"

#include "spdlog/spdlog.h"

uint32_t process_texture(sVPETContext& vpet, Texture* texture)
{
    std::string name = texture->get_name();

    // Check if already added
    auto it = std::find_if(vpet.texture_list.begin(), vpet.texture_list.end(), [name](sVPETTexture* n) {
        return n->name == name;
    });

    if (it != vpet.texture_list.end()) {
        return std::distance(vpet.texture_list.begin(), it);
    }

    sVPETTexture* vpet_texture = new sVPETTexture();

    vpet_texture->texture_data = texture->get_texture_data();
    vpet_texture->width = texture->get_width();
    vpet_texture->height = texture->get_height();
    vpet_texture->format = 0;
    vpet_texture->name = name;

    vpet.textures_byte_size += 4 * sizeof(uint32_t);
    vpet.textures_byte_size += vpet_texture->texture_data.size();

    vpet.texture_list.push_back(vpet_texture);

    return vpet.texture_list.size() - 1;
}

uint32_t process_material(sVPETContext& vpet, Surface* surface)
{
    Material* material = surface->get_material();

    if (!material) {
        return -1;
    }

    sVPETMaterial* vpet_material = new sVPETMaterial();
    vpet_material->material_id = vpet.material_list.size();

    vpet_material->name_size = material->get_name().size();
    memcpy(vpet_material->name, material->get_name().data(), std::min(vpet_material->name_size, 64u));

    const char* src = "Standard";
    vpet_material->src_size = strlen(src);
    memcpy(vpet_material->src, src, std::min(vpet_material->src_size, 64u));

    uint32_t texture_id_size = sizeof(uint32_t) + 2 * sizeof(glm::vec2);

    if (material->get_diffuse_texture()) {
        vpet_material->texture_id = process_texture(vpet, material->get_diffuse_texture());
        vpet_material->texture_offset = { 0.0f, 0.0f };
        vpet_material->texture_scale = { 1.0f, 1.0f };
        vpet.materials_byte_size += sizeof(sVPETMaterial);
    }
    else {
        vpet.materials_byte_size += sizeof(sVPETMaterial) - texture_id_size;
    }

    vpet.material_list.push_back(vpet_material);
}

std::string generate_mesh_identifier(Surface* surface)
{
    sSurfaceData* surface_data = surface->get_surface_data();
    return "Mesh_" + surface->get_name() + "_" + std::to_string(surface_data->vertices.size());;
}

uint32_t process_geo(sVPETContext& vpet, Surface* surface)
{
    sSurfaceData* surface_data = surface->get_surface_data();
    if (!surface_data) {
        assert(0);
        return -1;
    }

    // Check if already added
    std::string name = generate_mesh_identifier(surface);
    auto it = std::find_if(vpet.geo_list.begin(), vpet.geo_list.end(), [name](sVPETMesh* n) {
        return n->name == name;
        });

    if (it != vpet.geo_list.end()) {
        return std::distance(vpet.geo_list.begin(), it);
    }

    sVPETMesh* vpet_mesh = new sVPETMesh();

    vpet_mesh->name = name;

    vpet_mesh->vertex_array = surface_data->vertices;

    vpet.geos_byte_size += sizeof(uint32_t) + surface_data->vertices.size() * sizeof(glm::vec3);

    vpet_mesh->uv_array = surface_data->uvs;

    vpet.geos_byte_size += sizeof(uint32_t) + surface_data->uvs.size() * sizeof(glm::vec2);

    vpet_mesh->normal_array = surface_data->normals;

    vpet.geos_byte_size += sizeof(uint32_t) + surface_data->normals.size() * sizeof(glm::vec3);

    vpet_mesh->index_array = surface_data->indices;

    vpet.geos_byte_size += sizeof(uint32_t) + surface_data->indices.size() * sizeof(uint32_t);

    // bone weights & bone indices sizes
    vpet.geos_byte_size += sizeof(uint32_t);

    vpet.geo_list.push_back(vpet_mesh);

    return vpet.geo_list.size() - 1;
}

void add_scene_object(sVPETContext& vpet, sVPETNode* vpet_node, Node* node, uint32_t index)
{
    Node3D* node_3d = static_cast<Node3D*>(node);
    const Transform& transform = node_3d->get_transform();

    vpet_node->position = transform.get_position();
    vpet_node->rotation = transform.get_rotation();
    vpet_node->scale = transform.get_scale();

    vpet_node->child_count = node->get_children().size();

    vpet.nodes_byte_size += 3 * sizeof(float) + 2 * sizeof(glm::vec3) + sizeof(glm::quat);

    memcpy(vpet_node->name, node->get_name().c_str(), std::min(static_cast<uint32_t>(node->get_name().size()), 64u));
    vpet.nodes_byte_size += 64;

    vpet.node_list.push_back(vpet_node);
}

void process_scene_object(sVPETContext& vpet, Node* node, uint32_t index)
{
    MeshInstance3D* mesh_instance = dynamic_cast<MeshInstance3D*>(node);
    if (mesh_instance) {

        // Special case since MeshInstance3D may have several surfaces
        for (Surface* surface : mesh_instance->get_surfaces()) {

            sVPETGeoNode* geo_node = new sVPETGeoNode();
            geo_node->node_type = eVPETNodeType::GEO;

            geo_node->geo_id = process_geo(vpet, surface);
            geo_node->material_id = process_material(vpet, surface);

            geo_node = geo_node;
            geo_node->editable = true;

            vpet.nodes_byte_size += 2 * sizeof(uint32_t) + sizeof(glm::vec4);

            add_scene_object(vpet, geo_node, node, index);
        }

        return;
    }

    Light3D* light = dynamic_cast<Light3D*>(node);
    EntityCamera* camera = dynamic_cast<EntityCamera*>(node);

    if (light) {
        sVPETLightNode* light_node = new sVPETLightNode();
        light_node->node_type = eVPETNodeType::LIGHT;

        switch (light->get_type()) {
        case LightType::LIGHT_SPOT: {
            light_node->light_type = eVPETLightType::SPOT;

            SpotLight3D* spot_light = static_cast<SpotLight3D*>(light);
            light_node->angle = spot_light->get_outer_cone_angle();
            break;
        }
        case LightType::LIGHT_DIRECTIONAL:
            light_node->light_type = eVPETLightType::DIRECTIONAL;
            break;
        case LightType::LIGHT_OMNI:
            light_node->light_type = eVPETLightType::POINT;
            break;
        default:
            assert(0);
            break;
        }

        light_node->intensity = light->get_intensity();
        light_node->color = light->get_color();
        light_node->range = light->get_range();

        vpet.nodes_byte_size += 4 * sizeof(uint32_t) + sizeof(glm::vec3);

        add_scene_object(vpet, light_node, node, index);
    } else
    if (camera) {
        sVPETCamNode* cam_node = new sVPETCamNode();

        cam_node->node_type = eVPETNodeType::CAMERA;

        cam_node->fov = camera->get_fov();
        cam_node->aspect = camera->get_aspect();
        cam_node->near = camera->get_near();
        cam_node->far = camera->get_far();

        vpet.nodes_byte_size += 6 * sizeof(float);

        add_scene_object(vpet, cam_node, node, index);
    }
    else {
        sVPETNode* vpet_node = new sVPETNode();
        add_scene_object(vpet, vpet_node, node, index);
    }
}

void send_scene(void* distributor, const std::string& request, sVPETContext& vpet)
{
    spdlog::info("Requested: {}", request);
    std::vector<uint8_t> byte_array;

    if (request == "header") {
        sVPETHeader header = { .sender_id = 1 };

        byte_array.resize(sizeof(sVPETHeader));
        memcpy(&byte_array[0], &header, sizeof(sVPETHeader));
    } else
    if (request == "materials") {

        byte_array.resize(vpet.materials_byte_size);

        uint32_t texture_id_size = sizeof(uint32_t) + 2 * sizeof(glm::vec2);
        uint32_t buffer_ptr = 0;

        for (sVPETMaterial* material : vpet.material_list) {

            if (material->texture_id != -1) {
                memcpy(&byte_array[buffer_ptr], &material, sizeof(sVPETMaterial));
                buffer_ptr += sizeof(sVPETMaterial);
            }
            else {
                memcpy(&byte_array[buffer_ptr], &material, sizeof(sVPETMaterial) - texture_id_size);
                buffer_ptr += sizeof(sVPETMaterial) - texture_id_size;
            }
        }

        assert(buffer_ptr == vpet.materials_byte_size);

    } else
    if (request == "textures") {

        byte_array.resize(vpet.textures_byte_size);

        uint32_t buffer_ptr = 0;

        for (sVPETTexture* texture : vpet.texture_list) {
            memcpy(&byte_array[buffer_ptr], &texture->width, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], &texture->height, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], &texture->format, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            uint32_t texture_size = texture->texture_data.size();
            memcpy(&byte_array[buffer_ptr], &texture_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], texture->texture_data.data(), texture_size);
            buffer_ptr += texture_size;
        }

        assert(buffer_ptr == vpet.textures_byte_size);

    } else
    if (request == "objects") { // meshes

        byte_array.resize(vpet.geos_byte_size);

        uint32_t buffer_ptr = 0;

        for (sVPETMesh* mesh : vpet.geo_list) {
            uint32_t vertices_size = mesh->vertex_array.size();
            memcpy(&byte_array[buffer_ptr], &vertices_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], mesh->vertex_array.data(), vertices_size * sizeof(glm::vec3));
            buffer_ptr += vertices_size * sizeof(glm::vec3);

            uint32_t indices_size = mesh->index_array.size();
            memcpy(&byte_array[buffer_ptr], &indices_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], mesh->index_array.data(), indices_size * sizeof(uint32_t));
            buffer_ptr += indices_size * sizeof(uint32_t);

            uint32_t normals_size = mesh->normal_array.size();
            memcpy(&byte_array[buffer_ptr], &normals_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], mesh->normal_array.data(), normals_size * sizeof(glm::vec3));
            buffer_ptr += normals_size * sizeof(glm::vec3);

            uint32_t uvs_size = mesh->uv_array.size();
            memcpy(&byte_array[buffer_ptr], &uvs_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], mesh->uv_array.data(), uvs_size * sizeof(glm::vec2));
            buffer_ptr += uvs_size * sizeof(glm::vec2);

            uint32_t bone_weights_size = 0;
            memcpy(&byte_array[buffer_ptr], &bone_weights_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
        }

        assert(buffer_ptr == vpet.geos_byte_size);

    } else
    if (request == "nodes") {

        byte_array.resize(vpet.nodes_byte_size);

        uint32_t buffer_ptr = 0;

        for (sVPETNode* node : vpet.node_list) {

            memcpy(&byte_array[buffer_ptr], &node->node_type, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], &node->editable, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], &node->child_count, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&byte_array[buffer_ptr], &node->position, sizeof(glm::vec3));
            buffer_ptr += sizeof(glm::vec3);

            memcpy(&byte_array[buffer_ptr], &node->scale, sizeof(glm::vec3));
            buffer_ptr += sizeof(glm::vec3);

            memcpy(&byte_array[buffer_ptr], &node->rotation, sizeof(glm::quat));
            buffer_ptr += sizeof(glm::quat);

            memcpy(&byte_array[buffer_ptr], &node->name, 64);
            buffer_ptr += 64;

            switch (node->node_type)
            {
            case eVPETNodeType::GEO: {

                sVPETGeoNode* geo_node = static_cast<sVPETGeoNode*>(node);

                memcpy(&byte_array[buffer_ptr], &geo_node->geo_id, sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&byte_array[buffer_ptr], &geo_node->material_id, sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&byte_array[buffer_ptr], &geo_node->color, sizeof(glm::vec4));
                buffer_ptr += sizeof(glm::vec4);

                break;
            }
            case eVPETNodeType::LIGHT: {

                sVPETLightNode* light_node = static_cast<sVPETLightNode*>(node);

                memcpy(&byte_array[buffer_ptr], &light_node->light_type, sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&byte_array[buffer_ptr], &light_node->intensity, sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&byte_array[buffer_ptr], &light_node->angle, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&byte_array[buffer_ptr], &light_node->range, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&byte_array[buffer_ptr], &light_node->color, sizeof(glm::vec3));
                buffer_ptr += sizeof(glm::vec3);

                break;
            }
            case eVPETNodeType::CAMERA: {

                sVPETCamNode* camera_node = static_cast<sVPETCamNode*>(node);

                memcpy(&byte_array[buffer_ptr], &camera_node->fov, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&byte_array[buffer_ptr], &camera_node->aspect, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&byte_array[buffer_ptr], &camera_node->near, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&byte_array[buffer_ptr], &camera_node->far, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&byte_array[buffer_ptr], &camera_node->focal_dist, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&byte_array[buffer_ptr], &camera_node->aperture, sizeof(float));
                buffer_ptr += sizeof(float);

                break;
            }
            default:
                if (node->node_type != eVPETNodeType::GROUP) {
                    assert(0);
                }
                break;
            }
        }

        assert(buffer_ptr == vpet.nodes_byte_size);

    } else
    if (request == "characters") {

    } else
    if (request == "curve") {

    } else
    if (request == "parameterobjects") {

    }
    else {
        assert(0);
    }

    if (!byte_array.empty()) {
        zmq_send(distributor, &byte_array[0], byte_array.size(), 0);
    }
    else {
        zmq_send(distributor, nullptr, 0, 0);
    }
}
