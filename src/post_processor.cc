#include "external/stb_image_write.h"
#include "post_processor.h"
#include "color.h"

#include <vector>
#include <string>
#include <cstdint>

void post_processor::save_image(const std::vector<std::vector<color>>& image,
                                int current_samples,
                                const std::string filename) const
{
    // Strip the extension off the provided filename (e.g., "render.png" -> "render")
    std::string base_filename = filename;
    size_t dot_pos = filename.find_last_of(".");
    if (dot_pos != std::string::npos) {
        base_filename = filename.substr(0, dot_pos);
    }

    // 1. Allocate continuous memory buffers for stb_image_write
    // 3 components per pixel (R, G, B)
    auto image_height = image.size();
    auto image_width = image[0].size();
    
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
