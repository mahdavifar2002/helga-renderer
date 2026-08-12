#include "external/stb_image_write.h"

#include "rtweekend.h"
#include "color.h"
#include "camera.h"

#include <vector>
#include <string>

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

void camera::render(const shared_ptr<integrator> integ, const std::string filename) {
    std::cerr << "Rendering with '" << integ->label << "' integrator." << std::endl;

    initialize();
    
    std::vector<std::vector<color>> image(image_height, std::vector<color>(image_width));

    int lines_completed = 0;
    tqdm bar;

    for (int s = 1; s <= samples_per_pixel; s++) {
        #pragma omp parallel for schedule(dynamic)
        for (int j = 0; j < image_height; j++) {
            auto pixel_color = color(0, 0, 0);

            for (int i = 0; i < image_width; i++) {
                ray r = get_ray(i, j);
                image[j][i] += integ->ray_color(r);
            }
        }

        if (s == 1 || s == 2 || s == 5 || s % 10 == 0 || s == samples_per_pixel)
            save_image(image, s, filename, bar.time());
        
        bar.progress(s, samples_per_pixel);
    }

    bar.finish();
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

void camera::save_image(const std::vector<std::vector<color>>& image, int current_samples,
                const std::string filename, double time)
{
    // Strip the extension off the provided filename (e.g., "image.ppm" -> "image")
    std::string base_filename = filename;
    size_t dot_pos = filename.find_last_of(".");
    if (dot_pos != std::string::npos) {
        base_filename = filename.substr(0, dot_pos);
    }

    // 1. Allocate continuous memory buffers for stb_image_write
    // 3 components per pixel (R, G, B)
    std::vector<float> hdr_data(image_width * image_height * 3);
    std::vector<uint8_t> png_data(image_width * image_height * 3);

    double current_scale = 1.0 / current_samples;
    static const interval intensity(0.000, 0.999);

    for (int j = 0; j < image_height; j++) {
        for (int i = 0; i < image_width; i++) {
            
            // Calculate the 1D array index for this pixel
            int index = (j * image_width + i) * 3;
            
            // Get the raw linear color accumulated so far
            color pixel_color = current_scale * image[j][i];
            double r = pixel_color.x();
            double g = pixel_color.y();
            double b = pixel_color.z();

            // --- HDR PIPELINE (Raw, Linear, Unclamped) ---
            hdr_data[index + 0] = static_cast<float>(r);
            hdr_data[index + 1] = static_cast<float>(g);
            hdr_data[index + 2] = static_cast<float>(b);

            // --- PNG PIPELINE (Tone mapped, Gamma corrected, Clamped) ---
            // 0. Camera Exposure (Driven by JSON)
            r *= exposure;
            g *= exposure;
            b *= exposure;

            // 1. Tone Mapping
            if (tone_mapping == "reinhard") {
                // Apply x2 because tone mapping darkens the image
                r *= 2;
                g *= 2;
                b *= 2;
                
                // Hue-Preserving Reinhard Tone Mapping
                double max_component = std::fmax(r, std::fmax(g, b));
                double scale = 1.0 / (1.0 + max_component);
                r *= scale;
                g *= scale;
                b *= scale;
            }

            // 2. Gamma Correction
            r = linear_to_gamma(r, gamma);
            g = linear_to_gamma(g, gamma);
            b = linear_to_gamma(b, gamma);

            // 3. Clamp and convert to bytes
            png_data[index + 0] = static_cast<uint8_t>(256 * intensity.clamp(r));
            png_data[index + 1] = static_cast<uint8_t>(256 * intensity.clamp(g));
            png_data[index + 2] = static_cast<uint8_t>(256 * intensity.clamp(b));
        }
    }

    // 2. Save the files
    std::string hdr_filename = base_filename + ".hdr";
    std::string png_filename = base_filename + ".png";

    // Write HDR
    if (!stbi_write_hdr(hdr_filename.c_str(), image_width, image_height, 3, hdr_data.data())) {
        std::cerr << "Error: Could not write HDR file " << hdr_filename << "\n";
    }

    // Write PNG (Stride is width * 3 bytes)
    if (!stbi_write_png(png_filename.c_str(), image_width, image_height, 3, png_data.data(), image_width * 3)) {
        std::cerr << "Error: Could not write PNG file " << png_filename << "\n";
    }
}