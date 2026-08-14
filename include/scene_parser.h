#ifndef SCENE_PARSER_H
#define SCENE_PARSER_H

#include "external/json.hpp"
#include "rtweekend.h"
#include "hittable_list.h"
#include "camera.h"
#include "post_processor.h"
#include "integrator.h"
#include <string>

class scene_parser {
  public:
    scene_parser(const nlohmann::json& scene_data);
    scene_parser(const std::string& filename);


    void set_samples_per_pixel(int samples);
    void set_width(int width);
    void set_integrator(std::string integrator_type);
    const camera& get_camera() const { return cam; }
    post_processor& get_post_processor() { return processor; }
    const shared_ptr<integrator> get_integrator() const { return integ; }

    void parse();

    // Helper to resolve and load the scene file across candidate directories
    bool load(const std::string& filename);

    void render_scene(std::function<void(const std::vector<std::vector<color>>&, int)> on_sample_complete = nullptr,
                      std::atomic<bool>* cancel_flag = nullptr);

  private:
    nlohmann::json scene_config;
    std::string scene_filepath;
    std::string scene_dir_path;
    
    camera cam;
    post_processor processor;
    shared_ptr<integrator> integ;
    hittable_list world;
    shared_ptr<hittable_list> lights;
    shared_ptr<texture> background = make_shared<solid_color>(color(0, 0, 0));
};

#endif