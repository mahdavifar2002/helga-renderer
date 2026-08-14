#include "rtweekend.h"
#include "color.h"
#include "camera.h"

void camera::initialize() {
    // Calculate the image height, and ensure that it's at least 1.
    image_height = int (image_width / aspect_ratio);
    image_height = (image_height < 1) ? 1 : image_height;

    // Calulate the inverse value once for the pixel sample count.
    pixel_samples_scale = 1.0 / samples_per_pixel;

    center = lookfrom;

    // Determine viewport dimensions.
    auto theta = degrees_to_radians(vfov);
    auto h = std::tan(theta/2);
    auto viewport_height = 2 * h * focus_dist;
    auto viewport_width = viewport_height * (double(image_width) / image_height);

    // Calculate the u,v,w unit base vectors for the camera coordinate frame.
    w = unit_vector(lookfrom - lookat);
    u = unit_vector(cross(vup, w));
    v = cross(w, u);
    // Calculate the vectors across the horizontal and down the vertical viewport edges.
    auto viewport_u = viewport_width * u;
    auto viewport_v = -viewport_height * v;

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    pixel_delta_u = viewport_u / image_width;
    pixel_delta_v = viewport_v / image_height;

    // Calculate the location of the upper left pixel.
    auto viewport_upper_left = center - (focus_dist * w) - viewport_u/2 - viewport_v/2;
    pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // Calculate the camera defocus disk basis vectors
    auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
    defocus_disk_u = defocus_radius * u;
    defocus_disk_v = defocus_radius * v;

    // Check existance of lights.
}

void camera::render(const shared_ptr<integrator> integ,
                    std::function<void(const std::vector<std::vector<color>>&, int)> on_sample_complete,
                    std::atomic<bool>* cancel_flag)
{
    std::cerr << "Rendering with '" << integ->label << "' integrator." << std::endl;

    initialize();
    
    std::vector<std::vector<color>> image(image_height, std::vector<color>(image_width));

    int lines_completed = 0;
    tqdm bar;
    bar.progress(0, samples_per_pixel);

    bool finished = true;

    for (int s = 1; s <= samples_per_pixel; s++) {
        #pragma omp parallel for schedule(dynamic)
        for (int j = 0; j < image_height; j++) {
            if (cancel_flag && cancel_flag->load(std::memory_order_relaxed)) {
                continue;
            }

            for (int i = 0; i < image_width; i++) {
                ray r = get_ray(i, j);
                image[j][i] += integ->ray_color(r);
            }
        }

        if (cancel_flag && cancel_flag->load(std::memory_order_relaxed)) {
            std::cerr << "\nRender halted by user.\n";
            finished = false;
            break;
        }

        if (on_sample_complete)
            on_sample_complete(image, s);
        
        bar.progress(s, samples_per_pixel);
    }

    if (finished) {
        bar.finish();
        std::cerr << "\nRender finished succesfully.\n";
    }
}

vec3 camera::defocus_disk_sample() const {
    auto p = random_in_unit_disk();
    return center + p[0]*defocus_disk_u + p[1]*defocus_disk_v;
}

ray camera::get_ray(int i, int j) const {
    auto offset = random_in_unit_square() - vec3(0.5, 0.5, 0);
    auto pixel_sample = pixel00_loc
                        + ((i+offset.x()) * pixel_delta_u)
                        + ((j+offset.y()) * pixel_delta_v);

    auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
    auto ray_direction = pixel_sample - ray_origin;
    auto ray_time = random_double();

    return ray(ray_origin, ray_direction, ray_time);
}
