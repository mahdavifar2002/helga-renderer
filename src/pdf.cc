#include "pdf.h"

double sphere_pdf::value(const vec3& direction) const {
    return 1 / (4 * pi);
}

vec3 sphere_pdf::generate() const {
    return random_unit_vector();
}


double cosine_pdf::value(const vec3& direction) const {
    auto cosine_theta = dot(direction, uvw.w());
    return std::fmax(0, cosine_theta / pi);
}

vec3 cosine_pdf::generate() const {
    return uvw.transform(random_cosine_direction());
}

double mixture_pdf::value(const vec3& direction) const {
    return probability * p[0]->value(direction) + (1.0 - probability) * p[1]->value(direction);
}

vec3 mixture_pdf::generate() const {
    if (random_double() < probability)
        return p[0]->generate();
    else
        return p[1]->generate();
}


double hittable_pdf::value(const vec3& direction) const {
    return objects->pdf_value(origin, direction);
}

vec3 hittable_pdf::generate() const {
    return objects->random_direction(origin);
}