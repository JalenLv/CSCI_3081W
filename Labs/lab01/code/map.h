#ifndef MAP_H
#define MAP_H

#include "download.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void download_image(const char* filename, const char* url) {
    std::string buffer = download(url);
    int width, height, components;
    unsigned char *data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(buffer.c_str()), buffer.size(), &width, &height, &components, STBI_rgb_alpha);
    components = 4;
    stbi_write_png(filename, width, height, components, data, width * components);
    stbi_image_free(data);
}

void download_map_image(const char* filename, int zoom, int tileX, int tileY, const char* type) {
    std::string url = "https://server.arcgisonline.com/arcgis/rest/services/" + std::string(type) + "/MapServer/tile/" + 
        std::to_string(zoom) + "/" + std::to_string(tileY) + "/" + std::to_string(tileX);
    download_image(filename, url.c_str());
}

#endif