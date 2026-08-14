#include "scene_parser.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string scene_file = "";
    std::string output_file = helga_defaults::output_file;
    std::string integrator_type = "";
    int samples = 0;
    int width = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if ((arg == "-c" || arg == "--scene") && i + 1 < argc) {
            scene_file = argv[++i];
        } else if ((arg == "-s" || arg == "--samples") && i + 1 < argc) {
            samples = std::stoi(argv[++i]);
        } else if ((arg == "-w" || arg == "--width") && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if ((arg == "-o" || arg == "--integrator") && i + 1 < argc) {
            integrator_type = argv[++i];
        }
    }

    std::cerr << "Loading scene from: " << scene_file << "\n";

    // Build the scene
    scene_parser parser(scene_file);
    parser.parse();

    // Override render parameters if provided
    if (samples) parser.set_samples_per_pixel(samples);
    if (width) parser.set_width(width);
    if (!integrator_type.empty()) parser.set_integrator(integrator_type);
    
    // Callback function to save the rendered image progressively
    auto callback = [&parser, &output_file](const std::vector<std::vector<color>>& buffer, int sample_count) {
        if (sample_count == 1 || sample_count == 2 || sample_count == 5 || sample_count % 10 == 0 || sample_count == parser.get_camera().samples_per_pixel)
            parser.get_post_processor().save_image(buffer, sample_count, output_file);
    };
    
    // Render the scene
    parser.render_scene(callback);

    return 0;
}