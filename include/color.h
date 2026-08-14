#ifndef COLOR_H
#define COLOR_H

#include "interval.h"
#include "vec3.h"

#include <iostream>
#include <cmath>

using color = vec3;

inline double linear_to_gamma(const double linear_component, const double gamma) {
    if (linear_component > 0)
        return std::pow(linear_component, 1.0 / gamma);
    
    return 0;
}

inline color linear_to_gamma(const color& c, const double gamma) {
    return color(linear_to_gamma(c.x(), gamma), linear_to_gamma(c.y(), gamma), linear_to_gamma(c.z(), gamma));
}

inline void write_color(std::ostream& out, const color& pixel_color) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

    // Apply a linear to gamma transformation for gamma 2.
    r = linear_to_gamma(r, 2.0);
    g = linear_to_gamma(g, 2.0);
    b = linear_to_gamma(b, 2.0);

    auto max = std::fmax(r, std::fmax(g, b));

    // Adjust and scale RGB if some component is larger than one,
    // to preserve non-white light source.
    if (max > 1) {
        r /= max;
        b /= max;
        g /= max;
    }

    // Translate the [0, 1] component values to the byte range [0, 255].
    static const interval internsity(0.000, 0.999);
    int rbyte = int(256 * internsity.clamp(r));
    int gbyte = int(256 * internsity.clamp(g));
    int bbyte = int(256 * internsity.clamp(b));

    // Write out the pixel color components.
    out << rbyte << ' ' << gbyte << ' ' << bbyte << '\n';
}

#endif