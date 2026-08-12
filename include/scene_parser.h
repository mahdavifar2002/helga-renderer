#ifndef SCENE_PARSER_H
#define SCENE_PARSER_H

#include "rtweekend.h"
#include "hittable_list.h"
#include "camera.h"
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
    void render_scene(const std::string& output_filename);

  private:
    std::string filename;
    camera cam;
    shared_ptr<integrator> integ;
    hittable_list world;
    shared_ptr<hittable_list> lights;
    shared_ptr<texture> background = make_shared<solid_color>(color(0, 0, 0));
};

#endif