#ifndef AABB_H
#define AABB_H

#include "interval.h"

class aabb {
  public:
    interval x, y, z;

    aabb() {} // The default AABB is empty, since intervals are empty by default.

    aabb(const interval& x, const interval& y, const interval& z)
      : x(x), y(y), z(z)
    {
        pad_to_minimums();
    }
    
    aabb(const point3& a, const point3& b) {
        // Treat the two points a and b as extrema for the bounding box, so we don't require a
        // particular minimum/maximum coordinate order.

        x = (a[0] <= b[0]) ? interval(a[0], b[0]) : interval(b[0], a[0]);
        y = (a[1] <= b[1]) ? interval(a[1], b[1]) : interval(b[1], a[1]);
        z = (a[2] <= b[2]) ? interval(a[2], b[2]) : interval(b[2], a[2]);

        pad_to_minimums();
    }

    aabb(const aabb& box1, const aabb& box2) {
        x = interval(box1.x, box2.x);
        y = interval(box1.y, box2.y);
        z = interval(box1.z, box2.z);
    }

    const point3 min() const {
        return point3(x.min, y.min, z.min);
    }

    const point3 max() const {
        return point3(x.max, y.max, z.max);
    }

    const interval& axis_interval(int n) const {
        if (n == 1) return y;
        if (n == 2) return z;
        return x;
    }

    double volume() const {
        return x.size() * y.size() * z.size();
    }

    bool hit(const ray& r, interval ray_t) const {
        const point3& ray_orig = r.origin();
        const vec3&   ray_dir  = r.direction();

        for (int axis = 0; axis < 3; axis++) {
            const interval& ax = axis_interval(axis);
            const double adinv = 1.0 / ray_dir[axis];

            auto t0 = (ax.min - ray_orig[axis]) * adinv;
            auto t1 = (ax.max - ray_orig[axis]) * adinv;

            if (t0 < t1) {
                if (t0 > ray_t.min) ray_t.min = t0;
                if (t1 < ray_t.max) ray_t.max = t1;
            } else {
                if (t1 > ray_t.min) ray_t.min = t1;
                if (t0 < ray_t.max) ray_t.max = t0;
            }

            if (ray_t.max <= ray_t.min)
                return false;
        }

        return true;
    }

    // Returns the index of the longest axis of the bounding box.
    int longest_axis() const {
        if (x.size() > y.size())
            return x.size() > z.size() ? 0 : 2;
        else
            return y.size() > z.size() ? 1 : 2;
    }

    static const aabb empty, universe;

  private:

    // Adjusts the AABB so that no side is narrower than some delta, padding if necessary.
    void pad_to_minimums() {
        double delta = 0.0001;
        
        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }
};

inline const aabb aabb::empty    = aabb(interval::empty,    interval::empty,    interval::empty);
inline const aabb aabb::universe = aabb(interval::universe, interval::universe, interval::universe);

inline aabb operator+(const aabb& bbox, const vec3& offset) {
    return aabb(bbox.x + offset.x(), bbox.y + offset.y(), bbox.z + offset.z());
}

inline aabb operator+(const vec3& offset, const aabb& bbox) {
    return bbox + offset;
}

inline std::ostream& operator<<(std::ostream& os, const aabb& bbox) {
    os << bbox.min() << ", " << bbox.max();
    return os;
}

#endif