#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include <string>

enum class eVPETNodeType : uint32_t {
    GROUP, GEO, LIGHT, CAMERA, SKINNED_MESH, CHARACTER
};

enum class eVPETLightType {
    SPOT, DIRECTIONAL, POINT, AREA, NONE
};

struct sVPETHeader {
    float light_intensity_factor = 1.0;
    uint8_t sender_id = 0;
    uint8_t frame_rate = 60;
};

struct sVPETNode {
    eVPETNodeType node_type;
    bool editable = false;
    uint32_t child_count = 0;
    glm::vec3 position = {};
    glm::vec3 scale = {1, 1, 1};
    glm::quat rotation = { 0.f, 0.f, 0.f, 1.f };
    char name[64] = "";
};

struct sVPETGeoNode : public sVPETNode {
    int32_t geo_id = -1;
    int32_t material_id = -1;
    glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct sVPETLightNode : public sVPETNode {
    eVPETLightType light_type = eVPETLightType::SPOT;
    float intensity = 1.0f;
    float angle = 60.0f;
    float range = 500.0f;
    float exposure = 3.0f;
    glm::vec3 color = { 1.0f, 1.0f, 1.0f };
};

struct sVPETCamNode : public sVPETNode {
    float fov = 70;
    float near = 1.0f;
    float far = 1000.0f;
    float aspect = 16.0f / 9.0f;
    float focal_dist = 1.0f;
    float aperture = 2.8f;
};

struct sVPETMesh {
    std::string name;
    std::vector<glm::vec3> vertex_array;
    std::vector<uint32_t> index_array;
    std::vector<glm::vec3> normal_array;
    std::vector<glm::vec2> uv_array;
};

struct sVPETTexture {
    std::string name;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    std::vector<uint8_t> texture_data;
};

struct sVPETMaterial {
    uint32_t type = 1;
    uint32_t name_size;
    char name[64];
    uint32_t src_size;
    char src[64];
    int32_t material_id = -1;
    // We'll assume one texture for now
    int32_t texture_id = -1;
    glm::vec2 texture_offset;
    glm::vec2 texture_scale;
    // Propably skipped
    //uint32_t shader_config_size = 0;
    //std::vector<bool> shader_configs;
    //uint32_t shader_properties_ids_size = 0;
    //std::vector<uint32_t> property_ids;
    //std::vector<uint32_t> property_types;
    //uint32_t shader_properties_data_size = 0;
    //std::vector<uint8_t> property_data;
};

struct sVPETContext {
    std::vector<sVPETNode*> node_list;
    std::vector<sVPETMesh*> geo_list;
    std::vector<sVPETTexture*> texture_list;
    std::vector<sVPETMaterial*> material_list;

    uint32_t nodes_byte_size = 0;
    uint32_t geos_byte_size = 0;
    uint32_t textures_byte_size = 0;
    uint32_t materials_byte_size = 0;

    ~sVPETContext() { clean(); }

    void clean() {
        for (sVPETNode* node : node_list) {
            delete node;
        }

        for (sVPETMesh* mesh : geo_list) {
            delete mesh;
        }

        for (sVPETTexture* texture : texture_list) {
            delete texture;
        }

        for (sVPETMaterial* material : material_list) {
            delete material;
        }

        node_list.clear();
        geo_list.clear();
        texture_list.clear();
        material_list.clear();

        nodes_byte_size = 0;
        geos_byte_size = 0;
        textures_byte_size = 0;
        materials_byte_size = 0;
    }
};

// Update messages

enum class eVPETMessageType : uint8_t {
    PARAMETER_UPDATE,
    LOCK,
    SYNC,
    PING,
    RESEND_UPDATE,
    UNDO_REDO_ADD,
    RESET_OBJECT,
    RPC,
    EMPTY = 255
};

enum class eVPETParameterType : uint8_t {
    NONE,
    ACTION,
    BOOL,
    INT,
    FLOAT,
    VECTOR2,
    VECTOR3,
    VECTOR4,
    QUATERNION,
    COLOR,
    STRING,
    LIST,
    UNKNOWN = 100
};

//struct sVPETUpdate {
//    uint8_t id;
//    uint8_t param_type;
//};
//
//struct sVPETUpdatePos : public sVPETUpdate {
//    glm::vec3 position;
//};
//
//struct sVPETUpdateRot : public sVPETUpdate {
//    glm::quat rotation;
//};
//
//struct sVPETUpdateScale : public sVPETUpdate {
//    glm::vec3 scale;
//};
