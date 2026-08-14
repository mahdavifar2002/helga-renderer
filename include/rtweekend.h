#ifndef RTWEEKEND_H
#define RTWEEKEND_H

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <limits>
#include <memory>
#include <random>

// C++ Std Usings

using std::make_shared;
using std::shared_ptr;

// Constants
const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.1415926535897932385;

// Utility Functions

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

// Returns a random real in [0, 1)
inline double random_double() {
    thread_local std::uniform_real_distribution<double> distribution(0.0, 1.0);
    // Seed the generator with unique hardware entropy upon thread creation
    thread_local std::mt19937 generator(std::random_device{}());
    return distribution(generator);
}

// Returns a random real in [min, max)
inline double random_double(double min, double max) {
    return min + (max-min)*random_double();
}

// Returns a random integere in [min, max]
inline int random_int(int min, int max) {
    return int(random_double(min, max + 1));
}

// Common headers

#include "vec3.h"
#include "ray.h"
#include "interval.h"
#include "color.h"

#endif