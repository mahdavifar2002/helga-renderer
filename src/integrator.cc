#include "rtweekend.h"
#include "hittable_list.h"
#include "texture.h"
#include "integrator.h"
#include "sphere.h"
#include "material.h"

color path_tracing_integrator::ray_color(const ray& r, int depth) const {
    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return color(0, 0, 0);

    hit_record rec;

    if (!world.hit(r, interval(0.001, infinity), rec)) {
        double u, v;
        sphere::get_sphere_uv(r.direction(), u, v);
        return background->value(u, v, r.direction());
    }
    
    color color_from_emission = rec.mat->emitted(rec);

    scatter_record srec;
    if (!rec.mat->scatter(r, rec, srec))
        return color_from_emission;
    
    color color_from_scatter = srec.attenuation * ray_color(srec.skip_pdf_ray, depth - 1);
    
    return color_from_emission + color_from_scatter;
}

color MIS_mixture_integrator::ray_color(const ray& r, int depth) const {
    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return color(0, 0, 0);

    hit_record rec;

    if (!world.hit(r, interval(0.001, infinity), rec)) {
        double u, v;
        sphere::get_sphere_uv(r.direction(), u, v);
        return background->value(u, v, r.direction());
    }
    
    color color_from_emission = rec.mat->emitted(rec);

    scatter_record srec;
    if (!rec.mat->scatter(r, rec, srec))
        return color_from_emission;
    
    color color_from_scatter;

    if (srec.skip_pdf) { // For specular surfaces (dielectric and metal)
        color sample_color = ray_color(srec.skip_pdf_ray, depth - 1);
        color_from_scatter = srec.attenuation * sample_color;
    } else {
        auto bsdf_pdf   = srec.pdf_ptr;
        auto lights_pdf = make_shared<hittable_pdf>(make_shared<hittable_list>(lights), rec.p);
        auto mixed_pdf  = make_shared<mixture_pdf>(bsdf_pdf, lights_pdf);

        ray scattered = ray(rec.p, mixed_pdf->generate(), r.time());
        color sample_color = ray_color(scattered, depth - 1);

        auto scattering_pdf =  bsdf_pdf->value(scattered.direction());
        auto pdf_value      = mixed_pdf->value(scattered.direction());

        color_from_scatter = srec.attenuation * scattering_pdf * sample_color / pdf_value;
    }
    
    return color_from_emission + color_from_scatter;
}

color NEE_integrator::ray_color(const ray& r, int depth, bool is_shadow, bool from_specular) const {
    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return color(0, 0, 0);

    hit_record rec;

    if (!world.hit(r, interval(0.001, infinity), rec)) {
        if (is_shadow)
            return color(0, 0, 0);
        
        double u, v;
        sphere::get_sphere_uv(r.direction(), u, v);
        return background->value(u, v, r.direction());
    }
    
    color color_from_emission = rec.mat->emitted(rec);

    // Only capture emission for:
    // 1. shadow rays,
    // 2. first bounce from camera to the light source
    // 3. bounces from specular to the light source
    if (is_shadow || (rec.mat->emits() && depth == max_depth) || (rec.mat->emits() && from_specular))
        return color_from_emission;
    
    scatter_record srec;
    if (!rec.mat->scatter(r, rec, srec))
        return color(0, 0, 0);

    color color_from_scatter;

    if (srec.skip_pdf) { // For specular surfaces (dielectric and metal)
        color sample_color = ray_color(srec.skip_pdf_ray, depth - 1, false, true);
        color_from_scatter = srec.attenuation * sample_color;
    } else {
        auto bsdf_pdf   = srec.pdf_ptr;
        ray indirect_ray = ray(rec.p, bsdf_pdf->generate(), r.time());
        color indirect_color = ray_color(indirect_ray, depth - 1, false, false);
        color indirect_contribution = srec.attenuation * indirect_color;
        
        color direct_contribution = color(0, 0, 0);

        if (!lights->objects.empty()) {
            auto lights_pdf = make_shared<hittable_pdf>(lights, rec.p);
            ray shadow_ray   = ray(rec.p, lights_pdf->generate(), r.time());
            color   direct_color = ray_color(shadow_ray, 1, true, false);

            auto bsdf_pdf_value   =   bsdf_pdf->value(shadow_ray.direction());
            auto lights_pdf_value = lights_pdf->value(shadow_ray.direction());

            if (lights_pdf_value > 0.0) {
                auto weight = bsdf_pdf_value / lights_pdf_value;
                // weight = std::fmin(10.0, weight); // WARNING: it is biased, but dampens the 1/d^2 singularity
                direct_contribution = srec.attenuation * direct_color * weight;
            }
        }

        color_from_scatter = indirect_contribution + direct_contribution;
    }

    return color_from_scatter;
}

inline double power_heuristic(double pdf1, double pdf2) {
    double pdf1_2 = pdf1 * pdf1;
    double pdf2_2 = pdf2 * pdf2;
    return (pdf1_2) / (pdf1_2 + pdf2_2);
}

color MIS_NEE_integrator::ray_color(const ray& r, int depth, bool from_specular, double p_bsdf_prev) const {
    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return color(0, 0, 0);

    hit_record rec;

    if (!world.hit(r, interval(0.001, infinity), rec)) {
        double u, v;
        sphere::get_sphere_uv(r.direction(), u, v);
        return background->value(u, v, r.direction());
    }
    
    color color_from_emission = rec.mat->emitted(rec);

    // Only return full emission for:
    // 1. shadow rays,
    // 2. first bounce from camera to the light source
    // 3. bounces from specular to the light source
    if (rec.mat->emits() && (depth == max_depth || from_specular))
        return color_from_emission;
    
    if (rec.mat->emits()) {
        // An indirect ray hit a light.
        double p_light = lights->pdf_value(r.origin(), r.direction());
        double mis_weight = power_heuristic(p_bsdf_prev, p_light);
        return color_from_emission * mis_weight;
    }

    scatter_record srec;
    if (!rec.mat->scatter(r, rec, srec))
        return color(0, 0, 0);

    color color_from_scatter;

    if (srec.skip_pdf) { // For specular surfaces (dielectric and metal)
        color sample_color = ray_color(srec.skip_pdf_ray, depth - 1, true, 1.0);
        color_from_scatter = srec.attenuation * sample_color;
    } else {
        auto bsdf_pdf   = srec.pdf_ptr;

        // ---------------------------------------------------------
        // 1. INDIRECT CONTRIBUTION (BSDF Sampling Strategy)
        // ---------------------------------------------------------
        ray indirect_ray = ray(rec.p, bsdf_pdf->generate(), r.time());
        auto p_bsdf_indirect = bsdf_pdf->value(indirect_ray.direction());

        double scatter_pdf_indirect = rec.mat->scattering_pdf(r, rec, indirect_ray);

        color indirect_color = ray_color(indirect_ray, depth - 1, false, p_bsdf_indirect);
        
        color indirect_contribution = color(0, 0, 0);
        if (p_bsdf_indirect > 0.0)
            indirect_contribution = srec.attenuation * indirect_color * scatter_pdf_indirect / p_bsdf_indirect;

        // ---------------------------------------------------------
        // 2. DIRECT CONTRIBUTION (Light Sampling Strategy)
        // ---------------------------------------------------------
        color direct_contribution = color(0, 0, 0);

        if (!lights->objects.empty()) {
            auto lights_pdf = make_shared<hittable_pdf>(lights, rec.p);
            ray shadow_ray = ray(rec.p, lights_pdf->generate(), r.time());

            auto p_bsdf_shadow  =   bsdf_pdf->value(shadow_ray.direction());
            auto p_light_shadow = lights_pdf->value(shadow_ray.direction());

            auto scatter_pdf_shadow = rec.mat->scattering_pdf(r, rec, shadow_ray);

            // color direct_color = ray_color_MIS_NEE(shadow_ray, 1, world, lights, true, false, 0.0);

            color direct_color(0, 0, 0);

            hit_record light_rec;
            if (world.hit(shadow_ray, interval(0.001, infinity), light_rec))
                if (light_rec.mat->emits())
                    direct_color = light_rec.mat->emitted(light_rec);
            
            auto mis_weight = power_heuristic(p_light_shadow, p_bsdf_shadow);
            if (p_light_shadow > 0.0) {
                direct_contribution = srec.attenuation * direct_color * mis_weight * scatter_pdf_shadow / p_light_shadow;
            }
        }

        color_from_scatter = indirect_contribution + direct_contribution;
    }

    return color_from_scatter;
}
