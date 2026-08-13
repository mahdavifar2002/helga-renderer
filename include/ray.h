#ifndef RAY_H
#define RAY_H

#include "vec3.h"

class ray {
  public:
    ray() {}

    ray(const point3& origin, const vec3& direction, double time, bool skip_normalize = false)
      : orig(origin), dir(direction), tm(time)
    {
        if (!skip_normalize && dir.length_squared() > 0.0)
            dir = unit_vector(dir);
    }

    ray(const point3& origin, const vec3& direction, bool skip_normalize = false)
      : ray(origin, direction, 0, skip_normalize) {}
    
    const point3& origin() const { return orig; }
    const vec3& direction() const { return dir; }
    double time() const { return tm; }

    point3 at(double t) const {
        return orig + t*dir;
    }

  private:
    point3 orig;
    vec3 dir;
    double tm;
};

#endif