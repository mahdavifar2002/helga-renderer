#ifndef POST_PROCESSOR_H
#define POST_PROCESSOR_H

#include "rtweekend.h"

#include <vector>
#include <string>

class post_processor {
  public:
    std::string tone_mapping = "reinhard"; // Method for tone mapping
    double      exposure     = 1.0;        // Used in tone mapping for the png result
    double      gamma        = 2.2;        // Value for gamma correction

    // Applies exposure, tone mapping, and gamma correction for raw pixel
    const color process_pixel(color raw_pixel) const;

    // Saves the rendered image as PNG and HDR
    void save_image(const std::vector<std::vector<color>>& image,
                    int current_samples,
                    const std::string filename) const;
};

#endif