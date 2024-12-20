#include "scene_distribution.h"

#include "framework/nodes/mesh_instance_3d.h"
#include "framework/nodes/light_3d.h"
#include "framework/nodes/spot_light_3d.h"
#include "framework/nodes/camera.h"

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
    vpet_texture->format = 4; // RGBA32
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
    vpet_material->type = 1;
    vpet.materials_byte_size += sizeof(uint32_t);

    vpet_material->name_size = material->get_name().size();
    vpet.materials_byte_size += sizeof(uint32_t);

    memcpy(vpet_material->name, material->get_name().data(), std::min(vpet_material->name_size, 64u));
    vpet.materials_byte_size += vpet_material->name_size;

    const char* src = "Standard";
    vpet_material->src_size = strlen(src);
    vpet.materials_byte_size += sizeof(uint32_t);

    memcpy(vpet_material->src, src, std::min(vpet_material->src_size, 64u));
    vpet.materials_byte_size += vpet_material->src_size;

    vpet_material->material_id = vpet.material_list.size();
    vpet.materials_byte_size += sizeof(uint32_t);

    if (material->get_diffuse_texture()) {

        vpet_material->texture_ids_size = 1;
        vpet.materials_byte_size += sizeof(uint32_t);

        vpet_material->texture_ids.resize(vpet_material->texture_ids_size);
        vpet_material->texture_offsets.resize(vpet_material->texture_ids_size);
        vpet_material->texture_scales.resize(vpet_material->texture_ids_size);

        for (uint32_t i = 0; i < vpet_material->texture_ids_size; ++i) {
            vpet_material->texture_ids[i] = process_texture(vpet, material->get_diffuse_texture());
            vpet.materials_byte_size += sizeof(uint32_t);

            vpet_material->texture_offsets[i] = { 0.0f, 0.0f };
            vpet.materials_byte_size += sizeof(glm::vec2);

            vpet_material->texture_scales[i] = { 1.0f, 1.0f };
            vpet.materials_byte_size += sizeof(glm::vec2);
        }
    }
    else {
        vpet_material->texture_ids_size = 0;
        vpet.materials_byte_size += sizeof(uint32_t);
    }

    vpet.material_list.push_back(vpet_material);

    return vpet_material->material_id;
}

std::string generate_mesh_identifier(Surface* surface)
{
    sSurfaceData& surface_data = surface->get_surface_data();
    return "Mesh_" + surface->get_name() + "_" + std::to_string(surface_data.vertices.size());
}

uint32_t process_geo(sVPETContext& vpet, Surface* surface)
{
    sSurfaceData& surface_data = surface->get_surface_data();
    if (surface_data.size() == 0u) {
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

    // Transform to unity coordinate system
    vpet_mesh->vertex_array.resize(surface_data.vertices.size());
    for (uint32_t idx = 0; idx < surface_data.vertices.size(); ++idx) {
        vpet_mesh->vertex_array[idx] = surface_data.vertices[idx];
        vpet_mesh->vertex_array[idx].z = -vpet_mesh->vertex_array[idx].z;
    }

    vpet.geos_byte_size += sizeof(uint32_t) + vpet_mesh->vertex_array.size() * sizeof(glm::vec3);

    vpet_mesh->uv_array = surface_data.uvs;
    vpet.geos_byte_size += sizeof(uint32_t) + vpet_mesh->uv_array.size() * sizeof(glm::vec2);

    // Transform to unity coordinate system
    vpet_mesh->normal_array.resize(surface_data.normals.size());
    for (uint32_t idx = 0; idx < surface_data.normals.size(); ++idx) {
        vpet_mesh->normal_array[idx] = surface_data.normals[idx];
        vpet_mesh->normal_array[idx].z = -vpet_mesh->normal_array[idx].z;
    }

    vpet.geos_byte_size += sizeof(uint32_t) + vpet_mesh->normal_array.size() * sizeof(glm::vec3);

    // Transform triangle winding after vertex transform
    vpet_mesh->index_array.resize(surface_data.indices.size());
    uint32_t add_idx = 0;
    for (uint32_t idx = surface_data.indices.size(); idx > 0; --idx) {
        vpet_mesh->index_array[add_idx] = surface_data.indices[idx - 1];
        add_idx++;
    }

    vpet.geos_byte_size += sizeof(uint32_t) + vpet_mesh->index_array.size() * sizeof(uint32_t);

    // bone weights & bone indices sizes
    vpet.geos_byte_size += sizeof(uint32_t);

    vpet.geo_list.push_back(vpet_mesh);

    return vpet.geo_list.size() - 1;
}

void add_scene_object(sVPETContext& vpet, sVPETNode* vpet_node, Node* node)
{
    Node3D* node_3d = static_cast<Node3D*>(node);
    const Transform& transform = node_3d->get_transform();

    vpet_node->position = transform.get_position();
    vpet_node->rotation = transform.get_rotation();
    vpet_node->scale = transform.get_scale();

    vpet_node->child_count = node->get_children().size();

    vpet.nodes_byte_size += 3 * sizeof(float) + 2 * sizeof(glm::vec3) + sizeof(glm::quat);

    uint32_t node_name_size = node->get_name().size();
    memcpy(vpet_node->name, node->get_name().c_str(), std::min(node_name_size, 64u));
    vpet.nodes_byte_size += 64;

    vpet_node->node_ref = node_3d;

    vpet.node_list.push_back(vpet_node);

    if (vpet_node->editable) {
        vpet.editables_node_list.push_back(vpet_node);
    }
}

void process_scene_object(sVPETContext& vpet, Node* node)
{
    MeshInstance3D* mesh_instance = dynamic_cast<MeshInstance3D*>(node);
    if (mesh_instance) {

        sVPETNode* vpet_node = new sVPETNode();
        vpet_node->editable = true;
        add_scene_object(vpet, vpet_node, node);

        int idx = 0;
        // Special case since MeshInstance3D may have several surfaces
        for (Surface* surface : mesh_instance->get_surfaces()) {

            Node3D tmp_node = {};
            tmp_node.set_name(node->get_name() + "_surface_" + std::to_string(idx));

            sVPETGeoNode* geo_node = new sVPETGeoNode();
            geo_node->node_type = eVPETNodeType::GEO;

            geo_node->geo_id = process_geo(vpet, surface);
            geo_node->material_id = process_material(vpet, surface);

            geo_node->editable = false;

            vpet.nodes_byte_size += 2 * sizeof(uint32_t) + sizeof(glm::vec4);

            add_scene_object(vpet, geo_node, &tmp_node);

            idx++;
        }

        // Surfaces are added as child nodes
        vpet_node->child_count = mesh_instance->get_surfaces().size();

        return;
    }

    Light3D* light = dynamic_cast<Light3D*>(node);
    EntityCamera* camera = dynamic_cast<EntityCamera*>(node);

    if (light) {
        sVPETLightNode* light_node = new sVPETLightNode();
        light_node->node_type = eVPETNodeType::LIGHT;

        light_node->editable = true;

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
        light_node->range = light->get_range() * 0.5f;

        vpet.nodes_byte_size += 4 * sizeof(uint32_t) + sizeof(glm::vec3);

        add_scene_object(vpet, light_node, node);
    } else
    if (camera) {
        sVPETCamNode* cam_node = new sVPETCamNode();

        cam_node->node_type = eVPETNodeType::CAMERA;

        cam_node->fov = glm::degrees(camera->get_fov());
        cam_node->aspect = camera->get_aspect();
        cam_node->near = camera->get_near();
        cam_node->far = camera->get_far();

        cam_node->editable = true;

        vpet.nodes_byte_size += 6 * sizeof(float);

        add_scene_object(vpet, cam_node, node);
    }
    else {
        sVPETNode* vpet_node = new sVPETNode();
        add_scene_object(vpet, vpet_node, node);
    }
}

uint32_t get_scene_request_buffer(void* distributor, const std::string& request, sVPETContext& vpet, uint8_t** byte_array)
{
    uint32_t byte_array_size = 0;

    if (request == "header") {
        sVPETHeader header = { .sender_id = 1 };

        byte_array_size = sizeof(sVPETHeader);
        *byte_array = new uint8_t[byte_array_size];

        memcpy(&(*byte_array)[0], &header, sizeof(sVPETHeader));
    } else
    if (request == "materials") {

        byte_array_size = vpet.materials_byte_size;
        *byte_array = new uint8_t[byte_array_size];

        uint32_t buffer_ptr = 0;

        for (sVPETMaterial* material : vpet.material_list) {

            memcpy(&(*byte_array)[buffer_ptr], &material->type, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&(*byte_array)[buffer_ptr], &material->name_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&(*byte_array)[buffer_ptr], &material->name, material->name_size);
            buffer_ptr += material->name_size;

            memcpy(&(*byte_array)[buffer_ptr], &material->src_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&(*byte_array)[buffer_ptr], &material->src, material->src_size);
            buffer_ptr += material->src_size;

            memcpy(&(*byte_array)[buffer_ptr], &material->material_id, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&(*byte_array)[buffer_ptr], &material->texture_ids_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            if (material->texture_ids_size > 0) {
                memcpy(&(*byte_array)[buffer_ptr], material->texture_ids.data(), sizeof(uint32_t) * material->texture_ids_size);
                buffer_ptr += sizeof(uint32_t) * material->texture_ids_size;

                memcpy(&(*byte_array)[buffer_ptr], material->texture_offsets.data(), sizeof(glm::vec2) * material->texture_ids_size);
                buffer_ptr += sizeof(glm::vec2) * material->texture_ids_size;

                memcpy(&(*byte_array)[buffer_ptr], material->texture_scales.data(), sizeof(glm::vec2) * material->texture_ids_size);
                buffer_ptr += sizeof(glm::vec2) * material->texture_ids_size;
            }
        }

        assert(buffer_ptr == vpet.materials_byte_size);

    } else
    if (request == "textures") {

        byte_array_size = vpet.textures_byte_size;
        *byte_array = new uint8_t[byte_array_size];

        uint32_t buffer_ptr = 0;

        for (sVPETTexture* texture : vpet.texture_list) {
            memcpy(&(*byte_array)[buffer_ptr], &texture->width, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&(*byte_array)[buffer_ptr], &texture->height, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&(*byte_array)[buffer_ptr], &texture->format, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            uint32_t texture_size = texture->texture_data.size();
            memcpy(&(*byte_array)[buffer_ptr], &texture_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&(*byte_array)[buffer_ptr], texture->texture_data.data(), texture_size);
            buffer_ptr += texture_size;
        }

        assert(buffer_ptr == vpet.textures_byte_size);

    } else
    if (request == "objects") { // meshes

        byte_array_size = vpet.geos_byte_size;
        *byte_array = new uint8_t[byte_array_size];

        uint32_t buffer_ptr = 0u;

        for (sVPETMesh* mesh : vpet.geo_list) {
            uint32_t vertices_size = mesh->vertex_array.size();
            memcpy(&(*byte_array)[buffer_ptr], &vertices_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            memcpy(&(*byte_array)[buffer_ptr], mesh->vertex_array.data(), vertices_size * sizeof(glm::vec3));
            buffer_ptr += vertices_size * sizeof(glm::vec3);

            uint32_t indices_size = mesh->index_array.size();
            memcpy(&(*byte_array)[buffer_ptr], &indices_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            memcpy(&(*byte_array)[buffer_ptr], mesh->index_array.data(), indices_size * sizeof(uint32_t));
            buffer_ptr += indices_size * sizeof(uint32_t);

            uint32_t normals_size = mesh->normal_array.size();
            memcpy(&(*byte_array)[buffer_ptr], &normals_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            memcpy(&(*byte_array)[buffer_ptr], mesh->normal_array.data(), normals_size * sizeof(glm::vec3));
            buffer_ptr += normals_size * sizeof(glm::vec3);

            uint32_t uvs_size = mesh->uv_array.size();
            memcpy(&(*byte_array)[buffer_ptr], &uvs_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            memcpy(&(*byte_array)[buffer_ptr], mesh->uv_array.data(), uvs_size * sizeof(glm::vec2));
            buffer_ptr += uvs_size * sizeof(glm::vec2);

            uint32_t bone_weights_size = mesh->bone_weights_array.size();
            memcpy(&(*byte_array)[buffer_ptr], &bone_weights_size, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            memcpy(&(*byte_array)[buffer_ptr], mesh->bone_weights_array.data(), bone_weights_size * sizeof(glm::vec4));
            buffer_ptr += bone_weights_size * sizeof(glm::vec4);

            uint32_t bone_indices_size = bone_weights_size;
            memcpy(&(*byte_array)[buffer_ptr], mesh->bone_indices_array.data(), bone_indices_size * sizeof(uint32_t));
            buffer_ptr += bone_indices_size * sizeof(uint32_t);
        }

        assert(buffer_ptr == vpet.geos_byte_size);

    } else
    if (request == "nodes") {

        byte_array_size = vpet.nodes_byte_size;
        *byte_array = new uint8_t[byte_array_size];

        uint32_t buffer_ptr = 0;

        for (sVPETNode* node : vpet.node_list) {

            memcpy(&(*byte_array)[buffer_ptr], &node->node_type, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&(*byte_array)[buffer_ptr], &node->editable, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            memcpy(&(*byte_array)[buffer_ptr], &node->child_count, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);

            // Transform to unity coordinate system
            glm::vec3 transformed_pos = node->position;
            transformed_pos.z = -transformed_pos.z;
            memcpy(&(*byte_array)[buffer_ptr], &transformed_pos, sizeof(glm::vec3));
            buffer_ptr += sizeof(glm::vec3);

            memcpy(&(*byte_array)[buffer_ptr], &node->scale, sizeof(glm::vec3));
            buffer_ptr += sizeof(glm::vec3);

            glm::quat transformed_rot = node->rotation;
            transformed_rot.x = -transformed_rot.x;
            transformed_rot.y = -transformed_rot.y;
            memcpy(&(*byte_array)[buffer_ptr], &transformed_rot, sizeof(glm::quat));
            buffer_ptr += sizeof(glm::quat);

            memcpy(&(*byte_array)[buffer_ptr], &node->name, 64);
            buffer_ptr += 64;

            switch (node->node_type)
            {
            case eVPETNodeType::GEO: {

                sVPETGeoNode* geo_node = static_cast<sVPETGeoNode*>(node);

                memcpy(&(*byte_array)[buffer_ptr], &geo_node->geo_id, sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&(*byte_array)[buffer_ptr], &geo_node->material_id, sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&(*byte_array)[buffer_ptr], &geo_node->color, sizeof(glm::vec4));
                buffer_ptr += sizeof(glm::vec4);

                break;
            }
            case eVPETNodeType::LIGHT: {

                sVPETLightNode* light_node = static_cast<sVPETLightNode*>(node);

                memcpy(&(*byte_array)[buffer_ptr], &light_node->light_type, sizeof(uint32_t));
                buffer_ptr += sizeof(uint32_t);

                memcpy(&(*byte_array)[buffer_ptr], &light_node->intensity, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&(*byte_array)[buffer_ptr], &light_node->angle, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&(*byte_array)[buffer_ptr], &light_node->range, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&(*byte_array)[buffer_ptr], &light_node->color, sizeof(glm::vec3));
                buffer_ptr += sizeof(glm::vec3);

                break;
            }
            case eVPETNodeType::CAMERA: {

                sVPETCamNode* camera_node = static_cast<sVPETCamNode*>(node);

                memcpy(&(*byte_array)[buffer_ptr], &camera_node->fov, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&(*byte_array)[buffer_ptr], &camera_node->aspect, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&(*byte_array)[buffer_ptr], &camera_node->near, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&(*byte_array)[buffer_ptr], &camera_node->far, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&(*byte_array)[buffer_ptr], &camera_node->focal_dist, sizeof(float));
                buffer_ptr += sizeof(float);

                memcpy(&(*byte_array)[buffer_ptr], &camera_node->aperture, sizeof(float));
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

    return byte_array_size;
}
