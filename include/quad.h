#ifndef QUAD_H
#define QUAD_H

#include <iostream>

#include "hittable.h"
#include "hittable_list.h"
#include "rtw_obj_loader.h"

class quad : public hittable {
  public:
    quad(const point3& Q, const vec3& u, const vec3& v, shared_ptr<material> mat)
      : Q(Q), u(u), v(v), mat(mat)
    {
        auto n = cross(u, v);
        normal = unit_vector(n);
        D = dot(normal, Q);
        w = n / dot(n, n);

        quad_area = n.length();

        set_bounding_box();
    }

    // Computes the bounding box of all four vertices.
    virtual void set_bounding_box() {
        auto bbox_diagonal1 = aabb(Q, Q + u + v);
        auto bbox_diagonal2 = aabb(Q + u, Q + v);
        bbox = aabb(bbox_diagonal1, bbox_diagonal2);
    }

    bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
        auto denom = dot(normal, r.direction());

        // No hit if the ray is parallel to the plane.
        if (std::fabs(denom) < 1e-8)
            return false;
        
        // Return false if the hit parameter t is outside the ray interval.
        auto t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t))
            return false;
        
        // Determine if the hit point lies within the planar shape using its plane coordinates.
        auto intersection = r.at(t);
        vec3 planar_hitpt_vector = intersection - Q;
        auto alpha = dot(w, cross(planar_hitpt_vector, v));
        auto beta = dot(w, cross(u, planar_hitpt_vector));

        if (!is_interior(alpha, beta, rec))
            return false;

        // Ray hits the 2D shape; set the rest of the hit record and return true.
        rec.t = t;
        rec.p = intersection;
        rec.set_face_normal(r, normal);
        rec.mat = mat; 
        
        return true;
    }

    // Given the hit point in the plane coordinates, returns false if it is outside the
    // primitive, otherwise sets the hit record UV coordinates and returns true.
    virtual bool is_interior(double a, double b, hit_record& rec) const {
        interval unit_interval = interval(0, 1);

        if (!unit_interval.contains(a) || !unit_interval.contains(b))
            return false;
        
        rec.u = a;
        rec.v = b;
        return true;
    }
    
    double surface() const override { return quad_area; }

    aabb bounding_box() const override { return bbox; }

  protected:
    point3 Q;
    vec3 u, v;
    shared_ptr<material> mat;
    aabb bbox;
    double quad_area;

  private:
    // normal.point = A.x+B.y+C.z = D, for all points in quad.
    vec3 normal;
    double D;
    vec3 w;
};

// Returns the 3D box (six sides) that contains two opposite vertices a & b.
inline shared_ptr<hittable_list> box(const point3& a, const point3& b, shared_ptr<material> mat) {
    auto sides = make_shared<hittable_list>();

    // Construct two opposite vertices with the minimum and maximum coordinates.
    auto min = point3(std::fmin(a.x(), b.x()), std::fmin(a.y(), b.y()), std::fmin(a.z(), b.z()));
    auto max = point3(std::fmax(a.x(), b.x()), std::fmax(a.y(), b.y()), std::fmax(a.z(), b.z()));

    auto dx = vec3(max.x() - min.x(), 0, 0);
    auto dy = vec3(0, max.y() - min.y(), 0);
    auto dz = vec3(0, 0, max.z() - min.z());

    sides->add(make_shared<quad>(point3(min.x(), min.y(), max.z()),  dx,  dy, mat)); // front
    sides->add(make_shared<quad>(point3(max.x(), min.y(), max.z()), -dz,  dy, mat)); // right
    sides->add(make_shared<quad>(point3(max.x(), min.y(), min.z()), -dx,  dy, mat)); // back
    sides->add(make_shared<quad>(point3(min.x(), min.y(), min.z()),  dz,  dy, mat)); // left
    sides->add(make_shared<quad>(point3(min.x(), max.y(), max.z()),  dx, -dz, mat)); // top
    sides->add(make_shared<quad>(point3(min.x(), min.y(), min.z()),  dx,  dz, mat)); // bottom

    return sides;
}

class tri : public quad {
  public:
  // Constructor accepts the base quad arguments, plus the 3 UV pairs
    tri(const point3& Q, const vec3& u, const vec3& v, shared_ptr<material> mat,
        double u0 = 0, double v0 = 0,
        double u1 = 1, double v1 = 0,
        double u2 = 0, double v2 = 1)
      : quad(Q, u, v, mat)
    {
        set_bounding_box();
        
        uv[0][0] = u0; uv[0][1] = v0;
        uv[1][0] = u1; uv[1][1] = v1;
        uv[2][0] = u2; uv[2][1] = v2;
    }
    
    double surface() const override { return quad::surface() / 2; }
    
    void set_bounding_box() override {
        auto bbox_diagonal1 = aabb(Q, Q + u);
        auto bbox_diagonal2 = aabb(Q, Q + v);
        bbox = aabb(bbox_diagonal1, bbox_diagonal2);
    }
    
    bool is_interior(double a, double b, hit_record& rec) const override {
        if (a < 0 || b < 0 || a + b > 1)
            return false;
        
        // Then, for given hit point P,
        // P = Q + a.u + b.v
        // P = Q0 + a.(Q1 - Q0) + b.(Q2 - Q0)
        // P = (1.0 - a - b) Q0 + a Q1 + b Q2
        //     ^^^^^^^^^^^^^      ^      ^
        double gamma = 1.0 - a - b;

        rec.u = gamma * uv[0][0] + a * uv[1][0] + b * uv[2][0];
        rec.v = gamma * uv[0][1] + a * uv[1][1] + b * uv[2][1];

        return true;
    }
  private:
    // Storage for the UV coordinates of the 3 vertices
    double uv[3][2];
};

// Helper to extract directory from a full filepath (e.g., "models/bunny/file.obj" -> "models/bunny/")
inline std::string get_base_dir(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos != std::string::npos) return filepath.substr(0, pos + 1);
    return "";
}


inline shared_ptr<bvh_node> mesh(const char* filepath, shared_ptr<material> override_mat = nullptr, double scale = 1) {
    std::string path_str(filepath);
    std::string base_dir = get_base_dir(path_str);
    std::string filename = path_str.substr(base_dir.length());

    auto obj = rtw_obj(base_dir, filename);
    hittable_list faces;
    
    for (const auto& face : obj.faces) {
        shared_ptr<material> face_mat = override_mat ? override_mat : obj.materials[face.mat_id];

        faces.add(make_shared<tri>(scale * face.vertices[0],
                                    scale * (face.vertices[1] - face.vertices[0]),
                                    scale * (face.vertices[2] - face.vertices[0]),
                                    face_mat,
                                    // Pass the UV coordinates from tinyobjloader
                                    face.tex_u[0], face.tex_v[0],
                                    face.tex_u[1], face.tex_v[1],
                                    face.tex_u[2], face.tex_v[2]
                                ));        
    }

    std::cerr << "> Object '" << filename << "' loaded into the scene with " << obj.faces.size() << " triangles.\n";
    std::cerr << "> Bounding box: " << faces.bounding_box() << "\n";

    return make_shared<bvh_node>(faces);
}

class ellipse : public quad {
  public:
    ellipse(const point3& center, const vec3& u, const vec3& v, shared_ptr<material> mat)
      : quad(center, u, v, mat)
    {
        set_bounding_box();
    }

    double surface() const override { return pi*quad::surface(); }
    
    void set_bounding_box() override {
        auto bbox_diagonal1 = aabb(Q - u - v, Q + u + v);
        auto bbox_diagonal2 = aabb(Q - u + v, Q + u - v);
        bbox = aabb(bbox_diagonal1, bbox_diagonal2);
    }

    bool is_interior(double a, double b, hit_record& rec) const override {
        if (a*a + b*b > 1)
            return false;
        
        rec.u = (a + 1) / 2;
        rec.v = (b + 1) / 2;
        return true;
    }
};

#endif