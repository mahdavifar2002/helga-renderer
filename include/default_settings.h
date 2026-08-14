#ifndef DEFAULT_SETTINGS_H
#define DEFAULT_SETTINGS_H

namespace helga_defaults {
    constexpr int image_width = 300;
    constexpr int samples_per_pixel = 100;
    constexpr int max_depth = 6;
    constexpr double exposure = 1.0;
    constexpr double gamma = 2.2;
    constexpr double aspect_ratio = 1.0;
    constexpr double vfov = 90.0;
    constexpr double defocus_angle = 0.0;
    constexpr double focus_dist = 10.0;
    
    inline constexpr const char* integrator = "mis_nee";
    inline constexpr const char* tone_mapping = "reinhard";
    inline constexpr const char* output_file = "render.png";
    
    // GUI indices
    constexpr int integrator_idx = 3; // "mis_nee"
    constexpr int tonemap_idx = 0;    // "reinhard"
}

#endif