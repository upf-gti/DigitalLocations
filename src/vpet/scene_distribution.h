#pragma once

#include "structs.h"
#include "framework/nodes/node.h"
#include "framework/nodes/mesh_instance_3d.h"
#include "framework/nodes/light_3d.h"
#include "framework/nodes/spot_light_3d.h"

#include "graphics/texture.h"

#include <algorithm>

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
    vpet_texture->texture_width = texture->get_width();
    vpet_texture->texture_height = texture->get_height();
    vpet_texture->format = 0;
    vpet_texture->name = name;

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
    memcpy(vpet_material->name, material->get_name().data(), std::max(vpet_material->name_size, 64u));

    const char* src = "Standard";
    vpet_material->src_size = strlen(src);
    memcpy(vpet_material->src, src, std::max(vpet_material->src_size, 64u));

    if (material->get_diffuse_texture()) {
        vpet_material->texture_ids.push_back(process_texture(vpet, material->get_diffuse_texture()));
        vpet_material->texture_offsets.push_back( { 0.0f, 0.0f });
        vpet_material->texture_scales.push_back( { 1.0f, 1.0f });
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

    vpet_mesh->number_vertices = surface_data->vertices.size();
    vpet_mesh->vertex_array = surface_data->vertices;

    vpet_mesh->number_uvs = surface_data->uvs.size();
    vpet_mesh->uv_array = surface_data->uvs;

    vpet_mesh->number_normals = surface_data->normals.size();
    vpet_mesh->normal_array = surface_data->normals;

    vpet_mesh->index_array = surface_data->indices;

    vpet.geo_list.push_back(vpet_mesh);

    return vpet.geo_list.size() - 1;
}

void process_scene_object(sVPETContext& vpet, Node* node, uint32_t index)
{
    sVPETNode* vpet_node = nullptr;

    MeshInstance3D* mesh_instance = dynamic_cast<MeshInstance3D*>(node);
    Light3D* light = dynamic_cast<Light3D*>(node);

    if (mesh_instance) {

        for (Surface* surface : mesh_instance->get_surfaces()) {

            if (vpet_node) {
                assert(0);
                break;
            }

            sVPETGeoNode* geo_node = new sVPETGeoNode();
            geo_node->node_type = eVPETNodeType::GEO;

            geo_node->geo_id = process_geo(vpet, surface);
            geo_node->material_id = process_material(vpet, surface);

            vpet_node = geo_node;
        }

        vpet_node->editable = true;

    } else
    if (light) {
        sVPETLightNode* light_node = new sVPETLightNode();
        light_node->node_type = eVPETNodeType::LIGHT;

        switch (light->get_type()) {
        case LightType::LIGHT_SPOT: {
            light_node->type = eVPETLightType::SPOT;

            SpotLight3D* spot_light = static_cast<SpotLight3D*>(light);
            light_node->angle = spot_light->get_outer_cone_angle();

            break;
        }
        case LightType::LIGHT_DIRECTIONAL:
            light_node->type = eVPETLightType::DIRECTIONAL;
            break;
        case LightType::LIGHT_OMNI:
            light_node->type = eVPETLightType::POINT;
            break;
        default:
            assert(0);
            break;
        }

        light_node->intensity = light->get_intensity();
        light_node->color = light->get_color();
        light_node->range = light->get_range();

        vpet_node = light_node;
    }
    else {
        vpet_node = new sVPETNode();
    }

    Node3D* node_3d = static_cast<Node3D*>(node);
    const Transform& transform = node_3d->get_transform();

    vpet_node->position = transform.get_position();
    vpet_node->rotation = transform.get_rotation();
    vpet_node->scale = transform.get_scale();

    vpet_node->child_count = node->get_children().size();

    memcpy(vpet_node->name, node->get_name().c_str(), std::max(static_cast<uint32_t>(node->get_name().size()), 64u));

    vpet.node_list.push_back(vpet_node);
}
