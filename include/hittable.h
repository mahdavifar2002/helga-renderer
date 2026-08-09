#ifndef HITTABLE_H
#define HITTABLE_H

#include "ray.h"
#include "aabb.h"

class material;

class hit_record {
  public:
    point3 p;
    vec3 normal;
    shared_ptr<material> mat;
    double t; // the parameteric value at the point which ray hits object
    double u; // the `u` coordinate of the ray-object hit point
    double v; // the `v` coordinate of the ray-object hit point
    bool front_face;

    // Sets the hit record normal vector.
    // NOTE: the parameter `outward_normal` is assumed to have unit length.
    void set_face_normal(const ray& r, const vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class hittable {
  public:
    virtual ~hittable() = default;

    virtual bool hit(const ray& r, const interval& ray_t, hit_record& rec) const = 0;

	virtual double surface() const = 0;

    virtual aabb bounding_box() const = 0;
};

class translate : public hittable {
  public:
    translate(shared_ptr<hittable> object, const vec3& offset)
      : object(object), offset(offset)
    {
        bbox = object->bounding_box() + offset;
    }

    bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
        // Move the ray backwards by the offset
        ray offset_r(r.origin() - offset, r.direction(), r.time());

        // Determine whether an intersection exists along the offset ray (and if so, where)
        if (!object->hit(offset_r, ray_t, rec))
            return false;
        
        // Move the intersection point forwards by the offset
        rec.p += offset;

        return true;
    }

	double surface() const override { return object->surface(); }

    aabb bounding_box() const override {
        return bbox;
    }

  private:
    shared_ptr<hittable> object;
    vec3 offset;
    aabb bbox;
};

class rotate : public hittable {
  public:
    rotate(shared_ptr<hittable> object, double angle, int axis)
      : object(object), axis(axis)
    {
        auto radians = degrees_to_radians(angle);
        sin_theta = std::sin(radians);
        cos_theta = std::cos(radians);

        // Set bouding box

        bbox = object->bounding_box();
        point3 min( infinity,  infinity,  infinity);
        point3 max(-infinity, -infinity, -infinity);

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                for (int k = 0; k < 2; k++) {
                    auto corner = point3(i*bbox.x.max + (1-i)*bbox.x.min,
                                         j*bbox.y.max + (1-j)*bbox.y.min,
                                         k*bbox.z.max + (1-k)*bbox.z.min);
                    
                    auto rotated_corner = rotate_vec(corner, sin_theta, cos_theta, axis);

                    for (int c = 0; c < 3; c++) {
                        min[c] = std::fmin(min[c], rotated_corner[c]);
                        max[c] = std::fmax(max[c], rotated_corner[c]);
                    }
                }
            }
        }

        bbox = aabb(min, max);
    }

    bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {

        // Transform the ray from world space to object space.

        auto origin = rotate_vec(r.origin(), -sin_theta, cos_theta, axis);
        auto direction = rotate_vec(r.direction(), -sin_theta, cos_theta, axis);
        
        ray rotated_r(origin, direction, r.time());

        // Determine whether an intersection exists in object space (and if so, where).
        if (!object->hit(rotated_r, ray_t, rec))
            return false;
        
        // Transform the intersection from object space back to world space.

        rec.p = rotate_vec(rec.p, sin_theta, cos_theta, axis);
        rec.normal = rotate_vec(rec.normal, sin_theta, cos_theta, axis);

        return true;
    }

	double surface() const override { return object->surface(); }

    aabb bounding_box() const override {
        return bbox;
    }

  private:
    shared_ptr<hittable> object;
    double sin_theta;
    double cos_theta;
    int axis;
    aabb bbox;
};

class rotate_x : public rotate {
  public:
    rotate_x(shared_ptr<hittable> object, double angle) : rotate(object, angle, 0) {}
};

class rotate_y : public rotate {
  public:
    rotate_y(shared_ptr<hittable> object, double angle) : rotate(object, angle, 1) {}
};

class rotate_z : public rotate {
  public:
    rotate_z(shared_ptr<hittable> object, double angle) : rotate(object, angle, 2) {}
};

#endif