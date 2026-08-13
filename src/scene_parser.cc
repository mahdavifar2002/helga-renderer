#include "rtweekend.h"
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

scene_parser::scene_parser(const std::string& filename) : filename(filename) {}

bool scene_parser::parse() {
    auto scene_dir = getenv("RTW_SCENES");

    // Hunt for the scene file in some likely locations.
    if (scene_dir && load(std::string(scene_dir) + "/" + filename)) return true;
    if (load(filename)) return true;
    if (load("scenes/" + filename)) return true;
    if (load("../scenes/" + filename)) return true;
    if (load("../../scenes/" + filename)) return true;
    if (load("../../../scenes/" + filename)) return true;
    if (load("../../../../scenes/" + filename)) return true;
    if (load("../../../../../scenes/" + filename)) return true;
    if (load("../../../../../../scenes/" + filename)) return true;

    std::cerr << "ERROR: Could not open scene file: '" << filename << "'.\n";

    return false;
}

bool scene_parser::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
   
    // Load JSON locally inside the function!
    json scene_data;
    file >> scene_data;

    // 1. Parse Render Parameters
    if (scene_data.contains("render")) {
        auto r = scene_data["render"];
        integ = parse_integrator(r.value("integrator", "mis_nee"));
        cam.image_width       = r.value("image_width", 400);
        cam.samples_per_pixel = r.value("samples_per_pixel", 10);
        integ->max_depth      = r.value("max_depth", 6);

        // 1.2. Parse Post-Processing Parameters
        if (r.contains("post_processing")) {
            auto p = r["post_processing"];

            processor.tone_mapping = p.value("tone_mapping", "reinhard");
            processor.exposure     = p.value("exposure", 1.0);
            processor.gamma        = p.value("gamma", 2.2);
        }
    }

    // 2. Parse Camera
    if (scene_data.contains("camera")) {
        auto c = scene_data["camera"];
        cam.aspect_ratio = c.value("aspect_ratio", 1.0);
        cam.vfov         = c.value("vfov", 90.0);
       
        if (c.contains("lookfrom")) cam.lookfrom = parse_vec3(c["lookfrom"]);
        if (c.contains("lookat"))   cam.lookat = parse_vec3(c["lookat"]);
        if (c.contains("vup"))      cam.vup = parse_vec3(c["vup"]);
       
        cam.defocus_angle = c.value("defocus_angle", 0.0);
        cam.focus_dist    = c.value("focus_dist", 10.0);
    }

    // 3. Parse Background Texture
    if (scene_data.contains("background")) {
        background = parse_texture(scene_data["background"]);
    } else {
        background = make_shared<solid_color>(color(0,0,0));
    }

    // 4. Parse World and Extract Lights
    lights = make_shared<hittable_list>();
    if (scene_data.contains("world")) {
        for (const auto& item : scene_data["world"]) {
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
    return true;
}

void scene_parser::set_samples_per_pixel(int samples) {
    cam.samples_per_pixel = samples;
}

void scene_parser::set_width(int width) {
    cam.image_width = width;
}

void scene_parser::set_integrator(std::string integrator_type) {
    shared_ptr<integrator> new_integ = parse_integrator(integrator_type);
    
    new_integ->max_depth = integ->max_depth;
    new_integ->world = world;
    new_integ->lights = lights;
    new_integ->background = background;

    integ = new_integ;
}

void scene_parser::render_scene(std::function<void(const std::vector<std::vector<color>>&, int)> on_sample_complete) {
    cam.render(integ, on_sample_complete);
}