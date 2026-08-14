#ifndef CAMERA_H
#define CAMERA_H

#include <vector>
#include <functional>
#include <atomic>

#include "external/tqdm.h"

#include "default_settings.h"
#include "sphere.h"
#include "hittable_list.h"
#include "material.h"
#include "integrator.h"

class camera {
  public:
    double  aspect_ratio      = helga_defaults::aspect_ratio;      // Ration of image width over height 
    int     image_width       = helga_defaults::image_width;       // Rendered image width in pixel count
    int     image_height;                                          // Rendered image height
    int     samples_per_pixel = helga_defaults::samples_per_pixel; // Count of random samples for each pixel

    double vfov     = helga_defaults::vfov; // Vertical view angle (field of view)
    point3 lookfrom = point3(0, 0, 0);      // Point camera is looking from
    point3 lookat   = point3(0, 0, -1);     // Point camera is looking at
    vec3 vup        = vec3(0, 1, 0);        // Camera-relative "up" direction

    double defocus_angle = helga_defaults::defocus_angle; // Variation angle of rays through each pixel
    double focus_dist    = helga_defaults::focus_dist;    // Distance from camera lookfrom point to plane of perfect focus

    void initialize();

    void render(const shared_ptr<integrator> integ,
                std::function<void(const std::vector<std::vector<color>>&, int)> on_sample_complete = nullptr,
                std::atomic<bool>* cancel_flag = nullptr);

  private:
    double pixel_samples_scale; // Color scale factor for a sum of pixel samples
    point3 center;              // Camera center
    point3 pixel00_loc;         // location of pixel (0, 0)
    vec3   pixel_delta_u;       // Offset to pixel to the right
    vec3   pixel_delta_v;       // Offset to pixel below
    vec3 u, v, w;               // Camera frame basis vectors
    vec3 defocus_disk_u;        // Defocus disk horizontal radius
    vec3 defocus_disk_v;        // Defocus dik vertical radius

    // Returns a random point in the camera defocus disk around the camera center.
    vec3 defocus_disk_sample() const;

    // Constructs a camera ray originating from the origin and directed at randomly sampled
    // point around the pixel location i, j.
    ray get_ray(int i, int j) const;
};

#endif