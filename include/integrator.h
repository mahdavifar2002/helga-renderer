#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "rtweekend.h"
#include "default_settings.h"
#include "hittable_list.h"
#include "texture.h"

class integrator {
  public:
    integrator() {};
    
    virtual color ray_color(const ray& r) const = 0;
    
    std::string label;
    int max_depth = helga_defaults::max_depth;
    hittable_list world;
    shared_ptr<texture> background;
    shared_ptr<hittable_list> lights;
};

// Path tracing with BSDF sampling only.
// No explicit light sampling / next-event estimation.
class path_tracing_integrator : public integrator {
  public:
    path_tracing_integrator() { label = "path_tracing"; }
    
    color ray_color(const ray& r) const override {
        return ray_color(r, max_depth);
    }

    color ray_color(const ray& r, int depth) const;
};

// Path tracing using a mixture PDF of BSDF and light sampling.
// One direction is sampled per bounce.
class MIS_mixture_integrator : public integrator {
  public:
    MIS_mixture_integrator() { label = "mis_mixture"; }
    
    color ray_color(const ray& r) const override {
        return ray_color(r, max_depth);
    }

    color ray_color(const ray& r, int depth) const;
};

// Path tracing with Next Event Estimation (NEE).
class NEE_integrator : public integrator {
  public:
    NEE_integrator() { label = "nee"; }

    color ray_color(const ray& r) const override {
        return ray_color(r, max_depth);
    }

    color ray_color(const ray& r, int depth, bool is_shadow = false, bool from_specular = false) const;
};

// Path tracing with Next Event Estimation (NEE)
// using Multiple Importance Sampling (MIS) between
// BSDF sampling and direct light sampling.
class MIS_NEE_integrator : public integrator {
  public:
    MIS_NEE_integrator() { label = "mis_nee"; }

    color ray_color(const ray& r) const override {
        return ray_color(r, max_depth);
    }

    color ray_color(const ray& r, int depth, bool from_specular = false, double p_bsdf_prev = 1.0) const;
};

#endif