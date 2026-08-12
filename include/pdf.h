#ifndef PDF_H
#define PDF_H

#include "rtweekend.h"
#include "hittable_list.h"
#include "onb.h"

class pdf {
  public:
    virtual ~pdf() {}

    virtual double value(const vec3& direction) const = 0;
    virtual vec3 generate() const = 0;
};

class sphere_pdf : public pdf {
  public:
    sphere_pdf() {}

    double value(const vec3& direction) const override;
    vec3 generate() const override;
};

class cosine_pdf : public pdf {
  public:
    cosine_pdf(const vec3& w) : uvw(w) {}

    double value(const vec3& direction) const override;
    vec3 generate() const override;

  private:
    onb uvw;
};

class mixture_pdf : public pdf {
  public:
    mixture_pdf(shared_ptr<pdf> p0, shared_ptr<pdf> p1, double probability = 0.5)
      : p{p0, p1} , probability(probability) {}

    double value(const vec3& direction) const override;
    vec3 generate() const override;

  private:
    shared_ptr<pdf> p[2];
    double probability;
};

class hittable_pdf : public pdf {
  public:
    hittable_pdf(shared_ptr<hittable> objects, const point3& origin)
      : objects(objects), origin(origin)
    {}

    double value(const vec3& direction) const override;
    vec3 generate() const override;

  private:
    shared_ptr<hittable> objects;
    point3 origin;
};

#endif