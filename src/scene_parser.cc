#include "rtweekend.h"
#include "default_settings.h"
#include "scene_parser.h"
#include "sphere.h"
#include "bvh.h"
#include "quad.h"
#include "constant_medium.h"
#include "material.h"
#include "texture.h"

// Include the heavy JSON library ONLY in the translation unit
#include "external/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

// --- Anonymous Namespace ---
// Everything in here is hidden from the rest of the C++ project.
namespace {

    vec3 parse_vec3(const json& j) {
        return vec3(j[0].get<double>(), j[1].get<double>(), j[2].get<double>());
    }

    color parse_color(const json& j) {
        return color(j[0].get<double>(), j[1].get<double>(), j[2].get<double>());
    }

    // Forward declare parse_texture if it needs to call itself recursively
    shared_ptr<texture> parse_texture(const json& j);

    shared_ptr<texture> parse_texture(const json& j) {
        std::string type = j["type"].get<std::string>();
        if (type == "solid") {
            return make_shared<solid_color>(parse_color(j["color"]));
        }
        else if (type == "checker") {
            if (j.contains("even"))
                return make_shared<checker_texture>(j["scale"].get<double>(), parse_texture(j["even"]), parse_texture(j["odd"]));
            if (j.contains("c1"))
                return make_shared<checker_texture>(j["scale"].get<double>(), parse_color(j["c1"]), parse_color(j["c2"]));
        }
        else if (type == "image") {
            return make_shared<image_texture>(j["filename"].get<std::string>().c_str());
        }
        else if (type == "noise") {
            return make_shared<noise_texture>(j["scale"].get<double>());
        }
        return make_shared<solid_color>(color(1, 0, 1));
    }

    shared_ptr<material> parse_material(const json& j) {
        std::string type = j["type"].get<std::string>();
        if (type == "lambertian") {
            if (j.contains("albedo")) return make_shared<lambertian>(parse_color(j["albedo"]));
            if (j.contains("texture")) return make_shared<lambertian>(parse_texture(j["texture"]));
        }
        else if (type == "metal") {
            return make_shared<metal>(parse_color(j["albedo"]), j["fuzz"].get<double>());
        }
        else if (type == "dielectric") {
            return make_shared<dielectric>(j["refraction_index"].get<double>());
        }
        else if (type == "diffuse_light") {
            if (j.contains("emit")) return make_shared<diffuse_light>(parse_color(j["emit"]));
            return make_shared<diffuse_light>(parse_texture(j["texture"]));
        }
        return make_shared<lambertian>(color(1, 0, 1));
    }

    // Forward declare parse_hittable if it needs to call itself recursively
    shared_ptr<hittable> parse_hittable(const json& j, shared_ptr<hittable>& lights);

    shared_ptr<hittable> parse_hittable(const json& j, shared_ptr<hittable>& lights) {
        lights = nullptr;
        shared_ptr<hittable> result = nullptr;
        std::string type = j["type"].get<std::string>();

        if (type == "sphere") {
            auto mat = parse_material(j["material"]);
            if (j.contains("center2")) {
                result = make_shared<sphere>(parse_vec3(j["center"]), parse_vec3(j["center2"]), j["radius"].get<double>(), mat);
            } else {
                result = make_shared<sphere>(parse_vec3(j["center"]), j["radius"].get<double>(), mat);
            }
            if (mat->emits())
                lights = result;
        }
        else if (type == "quad") {
            auto mat = parse_material(j["material"]);
            result = make_shared<quad>(parse_vec3(j["Q"]), parse_vec3(j["u"]), parse_vec3(j["v"]), mat);
            if (mat->emits())
                lights = result;
        }
        else if (type == "box") {
            auto mat = parse_material(j["material"]);
            result = box(parse_vec3(j["a"]), parse_vec3(j["b"]), mat);
            if (mat->emits())
                lights = result;
        }
        else if (type == "mesh") {
            shared_ptr<material> mat_override = nullptr;
    
            // Only parse the material if it explicitly exists in the JSON
            if (j.contains("material")) {
                mat_override = parse_material(j["material"]);
            }

            result = mesh(j["filename"].get<std::string>().c_str(), lights, mat_override, j.value("scale", 1.0));
        }
        else if (type == "translate") {
            auto offset = parse_vec3(j["offset"]);
            shared_ptr<hittable> object_lights;
            result = make_shared<translate>(parse_hittable(j["object"], object_lights), offset);
            if (object_lights != nullptr)
                lights = make_shared<translate>(object_lights, offset);
        }
        else if (type == "rotate_x") {
            auto angle = j["angle"].get<double>();
            shared_ptr<hittable> object_lights;
            result = make_shared<rotate_x>(parse_hittable(j["object"], object_lights), angle);
            if (object_lights != nullptr)
                lights = make_shared<rotate_x>(object_lights, angle);
        }
        else if (type == "rotate_y") {
            auto angle = j["angle"].get<double>();
            shared_ptr<hittable> object_lights;
            result = make_shared<rotate_y>(parse_hittable(j["object"], object_lights), angle);
            if (object_lights != nullptr)
                lights = make_shared<rotate_y>(object_lights, angle);
        }
        else if (type == "rotate_z") {
            auto angle = j["angle"].get<double>();
            shared_ptr<hittable> object_lights;
            result = make_shared<rotate_z>(parse_hittable(j["object"], object_lights), angle);
            if (object_lights != nullptr)
                lights = make_shared<rotate_z>(object_lights, angle);
        }
        else if (type == "constant_medium") {
            auto density = j["density"].get<double>();
            result = make_shared<constant_medium>(parse_hittable(j["object"], lights), density, parse_color(j["color"]));
        }
        else if (type == "bvh_node") {
            hittable_list objects_list;
            hittable_list lights_list;
            for (const auto& item : j["objects"]) {
                shared_ptr<hittable> item_lights;
                objects_list.add(parse_hittable(item, item_lights));
                if (item_lights != nullptr)
                    lights_list.add(item_lights);
            }
            result = make_shared<bvh_node>(objects_list);
        }

        if (result == nullptr)
            std::cerr << "Unknown hittable type: " << type << "\n";
        
        return result;
    }

    shared_ptr<integrator> parse_integrator(const std::string& value) {
        shared_ptr<integrator> integ;

        if (value == "path_tracing")
            integ = make_shared<path_tracing_integrator>();
        else if (value == "mis_mixture")
            integ = make_shared<MIS_mixture_integrator>();
        else if (value == "nee")
            integ = make_shared<NEE_integrator>();
        else if (value == "mis_nee")
            integ = make_shared<MIS_NEE_integrator>();
        else {
            std::cerr << "Unknown integrator: " << value << "\n";
            integ = make_shared<MIS_NEE_integrator>();
        }

        return integ;
    }
} // --- End Anonymous Namespace ---


// --- Class Implementation ---

scene_parser::scene_parser(const nlohmann::json& scene_data) {
    scene_config = scene_data;
}

scene_parser::scene_parser(const std::string& filename) {
    auto scene_dir = getenv("RTW_SCENES");

    // Hunt for the scene file in some likely locations.
    if (scene_dir && load(std::string(scene_dir) + "/" + filename)) return;
    if (load(filename)) return;
    if (load("scenes/" + filename)) return;
    if (load("../scenes/" + filename)) return;
    if (load("../../scenes/" + filename)) return;
    if (load("../../../scenes/" + filename)) return;
    if (load("../../../../scenes/" + filename)) return;
    if (load("../../../../../scenes/" + filename)) return;
    if (load("../../../../../../scenes/" + filename)) return;

    std::cerr << "ERROR: Could not open scene file: '" << filename << "'.\n";
}

bool scene_parser::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    try {
        file >> scene_config;
        scene_filepath = path;

        // Extract parent directory path (e.g., "scenes/bedroom.json" -> "scenes")
        std::filesystem::path p(path);
        scene_dir_path = p.parent_path().string();
        if (scene_dir_path.empty()) scene_dir_path = ".";

        return true;
    } catch (...) {
        return false;
    }
}

void scene_parser::parse() {
    // 1. Parse Render Parameters
    if (scene_config.contains("render")) {
        auto r = scene_config["render"];
        integ = parse_integrator(r.value("integrator", helga_defaults::integrator));
        cam.image_width       = r.value("image_width", helga_defaults::image_width);
        cam.samples_per_pixel = r.value("samples_per_pixel", helga_defaults::samples_per_pixel);
        integ->max_depth      = r.value("max_depth", helga_defaults::max_depth);

        // 1.2. Parse Post-Processing Parameters
        if (r.contains("post_processing")) {
            auto p = r["post_processing"];

            processor.tone_mapping = p.value("tone_mapping", helga_defaults::tone_mapping);
            processor.exposure     = p.value("exposure", helga_defaults::exposure);
            processor.gamma        = p.value("gamma", helga_defaults::gamma);
        }
    }

    // 2. Parse Camera
    if (scene_config.contains("camera")) {
        auto c = scene_config["camera"];
        cam.aspect_ratio = c.value("aspect_ratio", helga_defaults::aspect_ratio);
        cam.vfov         = c.value("vfov", helga_defaults::vfov);
       
        if (c.contains("lookfrom")) cam.lookfrom = parse_vec3(c["lookfrom"]);
        if (c.contains("lookat"))   cam.lookat = parse_vec3(c["lookat"]);
        if (c.contains("vup"))      cam.vup = parse_vec3(c["vup"]);
       
        cam.defocus_angle = c.value("defocus_angle", helga_defaults::defocus_angle);
        cam.focus_dist    = c.value("focus_dist", helga_defaults::focus_dist);
    }
    cam.initialize();

    // 3. Parse Background Texture
    if (scene_config.contains("background")) {
        background = parse_texture(scene_config["background"]);
    } else {
        background = make_shared<solid_color>(color(0,0,0));
    }

    // 4. Parse World and Extract Lights
    lights = make_shared<hittable_list>();
    if (scene_config.contains("world")) {
        for (const auto& item : scene_config["world"]) {
            shared_ptr<hittable> item_lights;
            world.add(parse_hittable(item, item_lights)); // Calls the anonymous namespace function
            if (item_lights != nullptr)
                lights->add(item_lights);
        }
    }
   
    world = hittable_list(make_shared<bvh_node>(world));

    std::cerr << lights->size() << " light primitive(s) found in the scene.\n";
    std::cerr << world.size() << " total primitive(s) found in the scene.\n";

    integ->world = world;
    integ->lights = lights;
    integ->background = background;
}

void scene_parser::set_samples_per_pixel(int samples) {
    cam.samples_per_pixel = samples;
    cam.initialize();
}

void scene_parser::set_width(int width) {
    cam.image_width = width;
    cam.initialize();
}

void scene_parser::set_integrator(std::string integrator_type) {
    shared_ptr<integrator> new_integ = parse_integrator(integrator_type);
    
    new_integ->max_depth = integ->max_depth;
    new_integ->world = world;
    new_integ->lights = lights;
    new_integ->background = background;

    integ = new_integ;
}

void scene_parser::render_scene(std::function<void(const std::vector<std::vector<color>>&, int)> on_sample_complete,
                                std::atomic<bool>* cancel_flag) {
    cam.render(integ, on_sample_complete, cancel_flag);
}