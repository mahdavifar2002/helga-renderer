#ifndef RTW_OBJ_LOADER_H
#define RTW_OBJ_LOADER_H

#include <iostream>
#include <filesystem>

#include "material.h"

class face_t {
  public:
    point3 vertices[3];
    vec3 normals[3];
    double tex_u[3];
    double tex_v[3];
    int mat_id;
};

class rtw_obj {
  public:
    std::vector<face_t> faces;
    std::vector<shared_ptr<material>> materials;

    rtw_obj() {}

    // Loads obj data from the specified file.
    rtw_obj(const std::string& filepath) {
        std::filesystem::path path_obj(filepath);
        
        // Extract parent directory and filename natively
        std::string model_dir = path_obj.parent_path().string();
        std::string filename = path_obj.filename().string();

        if (!load(model_dir, filename)) {
            std::cerr << "ERROR: Could not load obj file: '" << filepath << "'.\n";
        }
    }

    // Implementation in the `rtw_obj_loader.cc`
    bool load(const std::string& model_dir, const std::string& filename);
};

#endif