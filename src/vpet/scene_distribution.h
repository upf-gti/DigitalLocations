#pragma once

#include "structs.h"
#include "framework/nodes/node.h"

#include "graphics/texture.h"

#include <algorithm>

class Surface;

uint32_t process_texture(sVPETContext& vpet, Texture* texture);

uint32_t process_material(sVPETContext& vpet, Surface* surface);

uint32_t process_geo(sVPETContext& vpet, Surface* surface);

void add_scene_object(sVPETContext& vpet, sVPETNode* vpet_node, Node* node, uint32_t index);

void process_scene_object(sVPETContext& vpet, Node* node);

void send_scene(void* distributor, const std::string& request, sVPETContext& vpet);
