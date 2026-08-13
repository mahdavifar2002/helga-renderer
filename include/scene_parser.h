#ifndef SCENE_PARSER_H
#define SCENE_PARSER_H

#include "rtweekend.h"
#include "hittable_list.h"
#include "camera.h"
#include "post_processor.h"
#include "integrator.h"
#include <string>

class scene_parser {
  public:
    scene_parser(const std::string& filename);

    bool parse();
    bool load(const std::string& filename);
    void set_samples_per_pixel(int samples);
    void set_width(int width);
    void set_integrator(std::string integrator_type);
    const post_processor get_post_processor() const { return processor; }
    const camera get_camera() const { return cam; }
    void render_scene(std::function<void(const std::vector<std::vector<color>>&, int)> on_sample_complete = nullptr);

  private:
    std::string filename;
    camera cam;
    post_processor processor;
    shared_ptr<integrator> integ;
    hittable_list world;
    shared_ptr<hittable_list> lights;
    shared_ptr<texture> background = make_shared<solid_color>(color(0, 0, 0));
};

#endif