#define TINYOBJLOADER_IMPLEMENTATION
#include "external/tiny_obj_loader.h"

#include "rtweekend.h"
#include "rtw_obj_loader.h"

#include <filesystem>

inline shared_ptr<material> translate_mtl(const tinyobj::material_t& mtl, const std::string& base_dir) {
    const double EPSILON = 1e-5; // 0.00001

    // 1. Is it a light source? (Check Emissive 'Ke')
    if (mtl.emission[0] > EPSILON || mtl.emission[1] > EPSILON || mtl.emission[2] > EPSILON) {
        return make_shared<diffuse_light>(color(mtl.emission[0], mtl.emission[1], mtl.emission[2]));
    }

    // 2. Is it glass / transparent? (Check Index of Refraction 'Ni' or Dissolve 'd')
    // Ni = 1.0 is air. Typical glass is ~1.5.
    // NOTE: I removed the condition `|| (mtl.ior > 1.05)` because Blender defaults Ni to 1.5
    if (mtl.dissolve < (1.0 - EPSILON)) {
        // Fallback to 1.5 if IOR isn't explicitly set but dissolve is used
        double ir = (mtl.ior > 1.0) ? mtl.ior : 1.5; 
        return make_shared<dielectric>(ir);
    }

    // 3. Is it metallic? (Check Illumination model or strong Specular 'Ks')
    // illum == 3 implies reflection. Or if the specular color is very bright and material is shiny.
    bool is_metal = (mtl.illum == 3) || (mtl.specular[0] > 0.5 && mtl.specular[1] > 0.5 && mtl.specular[2] > 0.5 && mtl.shininess > 0);
    if (is_metal) {
        color albedo(mtl.diffuse[0], mtl.diffuse[1], mtl.diffuse[2]);
        
        // Convert Specular Exponent 'Ns' (usually 0 to 1000) to 'fuzz' (0.0 to 1.0)
        // High Ns = smooth/shiny (low fuzz). Low Ns = rough (high fuzz).
        double fuzz = 1.0 - std::min(1.0, mtl.shininess / 1000.0);
        
        return make_shared<metal>(albedo, fuzz);
    }

    // 4. Default to Lambertian (Diffuse)
    if (!mtl.diffuse_texname.empty()) {
        std::filesystem::path tex_path = std::filesystem::path(base_dir) / mtl.diffuse_texname;
        return make_shared<lambertian>(make_shared<image_texture>(tex_path.string().c_str()));
    } else {
        // No texture, just a solid color
        color albedo(mtl.diffuse[0], mtl.diffuse[1], mtl.diffuse[2]);
        return make_shared<lambertian>(albedo);
    }
}

bool rtw_obj::load(const std::string& model_dir, const std::string& filename) {
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = model_dir; // Path to material files
    reader_config.triangulate = true;

    tinyobj::ObjReader reader;

    std::filesystem::path full_obj_path = std::filesystem::path(model_dir) / filename;

    if (!reader.ParseFromFile(full_obj_path.string(), reader_config)) {
        // if (!reader.Error().empty()) std::cerr << "TinyObjReader: " << reader.Error();
        return false;
    }

    if (!reader.Warning().empty()) { std::cerr << "TinyObjReader: " << reader.Warning(); }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &tiny_materials = reader.GetMaterials();
    
    // Translate all tinyobj materials to our engine's materials
    for (const auto& tm : tiny_materials) {
        materials.push_back(translate_mtl(tm, model_dir));
    }

    // Add a highly visible "fallback" material at the end of the array 
    // just in case a face asks for an invalid material ID.
    materials.push_back(make_shared<lambertian>(color(1, 0, 1))); // Magenta

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            // Create the face object.
            face_t face;
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
    
            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                double vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                double vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                double vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                face.vertices[v] = point3(vx, vy, vz);

                // Check if `normal_index` is zero or positive. negative = no normal data
                if (idx.normal_index >= 0) {
                    double nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    double ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    double nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                    face.normals[v] = vec3(nx, ny, nz);
                }

                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                if (idx.texcoord_index >= 0) {
                    double tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    double ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

                    face.tex_u[v] = tx;
                    face.tex_v[v] = ty;
                }
            }

            // Safely grab the material ID for this face
            int id = shapes[s].mesh.material_ids[f];
            if (id < 0 || id >= tiny_materials.size()) {
                id = materials.size() - 1; // Use the magenta fallback
            }
            face.mat_id = id;
            
            faces.push_back(face);
            index_offset += fv;
        }
    }

    return true;
}