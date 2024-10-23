#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

enum eVPETNodeType {
    GROUP, GEO, LIGHT, CAMERA
};

enum eVPETLightType {
    SPOT, DIRECTIONAL, POINT, AREA, NONE
};


struct sVPETHeader {
    float light_intensity_factor = 1.0;
    int texture_binary_type = 0;
};

struct sVPETNode {
    bool editable = false;
    int child_count = 0;
    glm::vec3 position = {};
    glm::vec3 scale = {1, 1, 1};
    glm::quat rotation = { 0.f, 0.f, 0.f, 1.f };
    char name[64] = "";
};

struct sVPETGeoNode : public sVPETNode {
    int geo_id = -1;
    int texture_id = -1;
    int material_id = -1; // maybe optional??
    float roughness = 1.0f;
    glm::vec3 color = { 1.0f, 1.0f, 1.0f };
};

struct sVPETLightNode : public sVPETNode {
    int type = SPOT;
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
    int number_vertices;
    int number_indices;
    int number_normals;
    int number_uvs;
    std::vector<float> vertex_array;
    std::vector<uint32_t> index_array;
    std::vector<float> normal_array;
    std::vector<float> uv_array;
};

struct sVPETTexture {
    uint32_t texture_width;
    uint32_t texture_height;
    uint32_t format;
    uint32_t size_color_data_array;
    std::vector<uint8_t> texture_data;
};