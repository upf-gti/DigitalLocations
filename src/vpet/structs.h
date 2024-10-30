#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

enum class eVPETNodeType {
    GROUP, GEO, LIGHT, CAMERA, SKINNED_MESH, CHARACTER
};

enum class eVPETLightType {
    SPOT, DIRECTIONAL, POINT, AREA, NONE
};

struct sVPETHeader {
    float light_intensity_factor = 1.0;
    int texture_binary_type = 0;
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
    uint32_t geo_id = -1;
    uint32_t material_id = -1;
    glm::vec3 color = { 1.0f, 1.0f, 1.0f };
};

struct sVPETLightNode : public sVPETNode {
    eVPETLightType type = eVPETLightType::SPOT;
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
};

struct sVPETMesh {
    std::string name;
    uint32_t number_vertices;
    uint32_t number_indices;
    uint32_t number_normals;
    uint32_t number_uvs;
    std::vector<glm::vec3> vertex_array;
    std::vector<uint32_t> index_array;
    std::vector<glm::vec3> normal_array;
    std::vector<glm::vec2> uv_array;
};

struct sVPETTexture {
    std::string name;
    uint32_t texture_width;
    uint32_t texture_height;
    uint32_t format;
    std::vector<uint8_t> texture_data;
};

struct sVPETMaterial {
    uint32_t type = 1;
    uint32_t name_size;
    char name[64];
    uint32_t src_size;
    char src[64];
    uint32_t material_id = -1;
    uint32_t texture_id_size = 0;
    std::vector<uint32_t> texture_ids;
    std::vector<glm::vec2> texture_offsets;
    std::vector<glm::vec2> texture_scales;
    // Propably skipped
    uint32_t shader_config_size = 0;
    std::vector<bool> shader_configs;
    uint32_t shader_properties_ids_size = 0;
    std::vector<uint32_t> property_ids;
    std::vector<uint32_t> property_types;
    uint32_t shader_properties_data_size = 0;
    std::vector<uint8_t> property_data;
};

struct sVPETContext {
    std::vector<sVPETNode*> node_list;
    std::vector<sVPETMesh*> geo_list;
    std::vector<sVPETTexture*> texture_list;
    std::vector<sVPETMaterial*> material_list;
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
