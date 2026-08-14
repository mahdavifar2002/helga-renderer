#include "scene_parser.h"
#include "external/json.hpp"
#include "external/portable-file-dialogs.h"
#include "external/imgui/imgui.h"
#include "external/imgui/backends/imgui_impl_sdl2.h"
#include "external/imgui/backends/imgui_impl_opengl3.h"
#include "embedded_font.h"
#include "default_settings.h"
#include "helga_version.h"
#include <SDL2/SDL.h>
#include <SDL_opengl.h>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <algorithm>
#include <iostream>
#include <fstream>

// --- Shared State between Render Thread and GUI Thread ---
std::mutex buffer_mutex;
std::vector<uint8_t> display_buffer;
std::atomic<bool> frame_ready{false};
std::atomic<int> current_sample{0};
int render_width = 0;
int render_height = 0;
std::string last_open_dir = ".";
std::string last_save_dir = ".";
std::string last_valid_scene_path = "";
std::string scene_load_error = "";

// --- Application State ---
enum class AppState { Idle, Rendering };
std::atomic<AppState> app_state{AppState::Idle};
std::atomic<bool> cancel_request{false};
std::thread render_thread;
nlohmann::json active_scene_json;

// --- User Configurable Parameters ---
char scene_filepath[512] = "";
char output_filepath[512] = "render.png";
int ui_samples = helga_defaults::samples_per_pixel;
int ui_width = helga_defaults::image_width;
int ui_max_depth = helga_defaults::max_depth;
float ui_exposure = helga_defaults::exposure;
float ui_gamma = helga_defaults::gamma;

int ui_integrator_idx = helga_defaults::integrator_idx;
const char* integrator_names[] = { "path_tracing", "mis_mixture", "nee", "mis_nee" };

int ui_tonemap_idx = helga_defaults::tonemap_idx;
const char* tonemap_names[] = { "reinhard", "none" };

// --- Main Function ---
int main(int argc, char* argv[]) {
    // 1. Setup SDL and OpenGL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return -1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    std::string window_title = std::string("Helga Renderer v") + HELGA_VERSION;
    SDL_Window* window = SDL_CreateWindow(window_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_MaximizeWindow(window);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable VSync

    // 2. Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;
    
    io.Fonts->AddFontFromMemoryTTF(
        (void*)assets_Roboto_Regular_ttf,      // Pointer to the array
        assets_Roboto_Regular_ttf_len,         // Length of the array
        30.0f,                                 // Font size in pixels
        &font_config                           // Configuration
    );
    
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    // 3. Setup OpenGL Texture
    GLuint render_texture;
    glGenTextures(1, &render_texture);
    glBindTexture(GL_TEXTURE_2D, render_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 5. Main GUI Loop
    bool done = false;
    bool is_fullscreen = false;

    auto load_scene_from_path = [&](const std::string& input_filename) {
        
        // Define the attempt logic
        auto try_load = [&](const std::string& path) -> bool {
            std::ifstream file(path);
            if (!file.is_open()) return false;
            
            try {
                file >> active_scene_json;
                
                // Extract defaults for UI
                if (active_scene_json.contains("render")) {
                    auto& r = active_scene_json["render"];
                    ui_width = r.value("image_width", helga_defaults::image_width);
                    ui_samples = r.value("samples_per_pixel", helga_defaults::samples_per_pixel);
                    ui_max_depth = r.value("max_depth", helga_defaults::max_depth);
                    
                    std::string integ = r.value("integrator", helga_defaults::integrator);
                    for (int i = 0; i < 4; i++) {
                        if (integ == integrator_names[i]) ui_integrator_idx = i;
                    }
                    
                    if (r.contains("post_processing")) {
                        auto& p = r["post_processing"];

                        ui_exposure = p.value("exposure", helga_defaults::exposure);
                        ui_gamma = p.value("gamma", helga_defaults::gamma);
                        std::string tm = p.value("tone_mapping", helga_defaults::tone_mapping);
                        ui_tonemap_idx = (tm == "none") ? 1 : 0;
                    }
                }

                // SUCCESS: Update the GUI text box to show the actual found path
                snprintf(scene_filepath, sizeof(scene_filepath), "%s", path.c_str());

                // Save this as our confirmed valid path
                last_valid_scene_path = path;

                // Clear any previous error
                scene_load_error = "";
                
                // Keep the 'Browse' dialog anchored to this successful directory
                std::filesystem::path p(path);
                last_open_dir = p.parent_path().string();
                if (last_open_dir.empty()) last_open_dir = ".";
                
                return true;

            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "JSON parsing error in '" << path << "': " << e.what() << '\n';
                return false;
            }
        };

        // Execute the hunting chain
        auto scene_dir = getenv("RTW_SCENES");

        if (scene_dir && try_load(std::string(scene_dir) + "/" + input_filename)) return;
        if (try_load(input_filename)) return;
        if (try_load("scenes/" + input_filename)) return;
        if (try_load("../scenes/" + input_filename)) return;
        if (try_load("../../scenes/" + input_filename)) return;
        if (try_load("../../../scenes/" + input_filename)) return;
        if (try_load("../../../../scenes/" + input_filename)) return;
        if (try_load("../../../../../scenes/" + input_filename)) return;
        if (try_load("../../../../../../scenes/" + input_filename)) return;

        std::cerr << "ERROR: Could not open or find scene file: '" << input_filename << "'.\n";
        scene_load_error = "File not found: " + input_filename;
        snprintf(scene_filepath, sizeof(scene_filepath), "%s", last_valid_scene_path.c_str());
    };

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) done = true;
            
            // --- Listen for F11 for fullscreen ---
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                is_fullscreen = !is_fullscreen;
                
                if (is_fullscreen) {
                    // Switch to borderless fullscreen matching the desktop resolution
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                } else {
                    // Revert to normal windowed mode
                    SDL_SetWindowFullscreen(window, 0); 
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 6. Update Texture if a new frame is ready
        if (frame_ready) {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            glBindTexture(GL_TEXTURE_2D, render_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, render_width, render_height, 0, GL_RGB, GL_UNSIGNED_BYTE, display_buffer.data());
            frame_ready = false; // Reset flag
        }

        // 7. Draw the UI
        
        // Force the main ImGui window to cover the entire SDL window
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGuiWindowFlags main_window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("Helga Main Workspace", nullptr, main_window_flags);

        // --- LEFT PANEL: Sidebar for Controls ---
        ImGui::BeginChild("Sidebar", ImVec2(550, 0), true);
        ImGui::TextDisabled("HELGA RENDERER");
        ImGui::Separator();
        ImGui::Spacing();

        bool is_ui_disabled = (app_state.load() == AppState::Rendering);

        if (is_ui_disabled) {
            ImGui::BeginDisabled();
        }
        
        // -- IDLE STATE: Setup and Configuration --
        ImGui::Text("Scene Configuration");
        
        bool has_error = !scene_load_error.empty();

        if (has_error) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##filepath", scene_filepath, IM_ARRAYSIZE(scene_filepath), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
            // User typed a path and hit Enter
            load_scene_from_path(scene_filepath);
        }

        if (ImGui::IsItemEdited()) {
            scene_load_error = "";
        }

        if (has_error) {
            ImGui::PopStyleColor(2);
            
            // Show the specific error message when they hover over the red box
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", scene_load_error.c_str());
            }
        } else {
            // Standard tooltip for showing long valid paths
            if (ImGui::IsItemHovered() && strlen(scene_filepath) > 0) {
                ImGui::SetTooltip("%s", scene_filepath);
            }
        }

        if (ImGui::Button("Browse##Input", ImVec2(-1, 40))) {
            auto selection = pfd::open_file("Select Scene JSON", last_open_dir, 
                                            { "JSON Files", "*.json", "All Files", "*" }).result();
            
            if (!selection.empty()) {
                snprintf(scene_filepath, sizeof(scene_filepath), "%s", selection[0].c_str());
                
                // Update last_open_dir with the selected file's parent folder
                std::filesystem::path p(selection[0]);
                last_open_dir = p.parent_path().string();

                // --- PRE-PARSE JSON TO EXTRACT DEFAULTS ---
                load_scene_from_path(scene_filepath);
            }
        }

        // --- OUTPUT CONFIGURATION ---
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Output Configuration");

        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##outpath", output_filepath, IM_ARRAYSIZE(output_filepath), ImGuiInputTextFlags_AutoSelectAll);
        
        // Show the full path if the user hovers over the truncated box
        if (ImGui::IsItemHovered() && strlen(output_filepath) > 0) {
            ImGui::SetTooltip("%s", output_filepath);
        }
        
        if (ImGui::Button("Browse##Output", ImVec2(-1, 40))) {
            // Default the dialog to "render.png" in the last used directory
            std::string default_name = last_save_dir + "/" + helga_defaults::output_file;
            
            auto destination = pfd::save_file("Save Render Output", default_name, 
                                                { "PNG Image", "*.png", "HDR Image", "*.hdr", "PPM Image", "*.ppm" }).result();
            
            if (!destination.empty()) {
                snprintf(output_filepath, sizeof(output_filepath), "%s", destination.c_str());
                
                // Remember this directory for next time
                std::filesystem::path p(destination);
                last_save_dir = p.parent_path().string();
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Render Settings");
        ImGui::Spacing();

        // Sliders
        ImGui::DragInt("Width", &ui_width, 10.0f, 100, 3840, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
        ImGui::DragInt("Samples", &ui_samples, 10.0f, 1, 10000, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
        ImGui::DragInt("Max Depth", &ui_max_depth, 0.1f, 1, 50, "%d", ImGuiSliderFlags_AlwaysClamp);

        // Dropdowns (Combo boxes)
        ImGui::Combo("Integrator", &ui_integrator_idx, integrator_names, IM_ARRAYSIZE(integrator_names));
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Post Processing");
        ImGui::Spacing();

        ImGui::Combo("Tone Mapping", &ui_tonemap_idx, tonemap_names, IM_ARRAYSIZE(tonemap_names));
        ImGui::DragFloat("Exposure", &ui_exposure, 0.1f, 0.1f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
        ImGui::DragFloat("Gamma", &ui_gamma, 0.005f, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

        ImGui::Spacing();
        const char* button_text = (app_state.load() == AppState::Rendering) ? "Rendering..." : "Start Render";
        if (ImGui::Button(button_text, ImVec2(-1, 40))) {
            
            if (strlen(scene_filepath) == 0 || active_scene_json.empty()) {
                std::cerr << "WARNING: Cannot start render. No valid scene file is loaded.\n";
            }
            else {
                // Inject the UI values back into the JSON object
                active_scene_json["render"]["image_width"] = ui_width;
                active_scene_json["render"]["samples_per_pixel"] = ui_samples;
                active_scene_json["render"]["max_depth"] = ui_max_depth;
                active_scene_json["render"]["integrator"] = integrator_names[ui_integrator_idx];
                
                active_scene_json["render"]["post_processing"]["tone_mapping"] = tonemap_names[ui_tonemap_idx];
                active_scene_json["render"]["post_processing"]["exposure"] = ui_exposure;
                active_scene_json["render"]["post_processing"]["gamma"] = ui_gamma;

                // Prepare state for rendering
                app_state = AppState::Rendering;
                cancel_request = false;
                current_sample = 0;
                
                // Clean up old thread if it exists
                if (render_thread.joinable()) render_thread.join();

                // Launch the render on a background thread
                render_thread = std::thread([scene_data = active_scene_json, out_file = std::string(output_filepath)]() {
                    scene_parser parser(scene_data);
                    parser.parse();

                    render_width = parser.get_camera().image_width;
                    render_height = parser.get_camera().image_height;
                    
                    // Resize buffer safely
                    {
                        std::lock_guard<std::mutex> lock(buffer_mutex);
                        display_buffer.resize(render_width * render_height * 3);
                        std::fill(display_buffer.begin(), display_buffer.end(), 0);
                        frame_ready = true;
                    }

                    parser.render_scene([&](const std::vector<std::vector<color>>& image, int sample_count) {
                            if (sample_count == 1 || sample_count == 2 || sample_count == 5 || sample_count % 10 == 0 || sample_count == parser.get_camera().samples_per_pixel)
                                parser.get_post_processor().save_image(image, sample_count, out_file);

                        std::lock_guard<std::mutex> lock(buffer_mutex);
                        int idx = 0;

                        static const interval intensity(0.000, 0.999);
                        for (int j = 0; j < render_height; j++) {
                            for (int i = 0; i < render_width; i++) {
                                color pixel_color = image[j][i] / sample_count;
                                pixel_color = parser.get_post_processor().process_pixel(pixel_color);
                                auto r = pixel_color.x();
                                auto g = pixel_color.y();
                                auto b = pixel_color.z();
                                display_buffer[idx++] = static_cast<uint8_t>(256 * intensity.clamp(r));
                                display_buffer[idx++] = static_cast<uint8_t>(256 * intensity.clamp(g));
                                display_buffer[idx++] = static_cast<uint8_t>(256 * intensity.clamp(b));
                            }
                        }
                        current_sample = sample_count;
                        frame_ready = true;
                    }, &cancel_request);

                    // When finished or halted, return to Idle state
                    app_state = AppState::Idle;
                });
            }
        }
        
        if (is_ui_disabled) {
            ImGui::EndDisabled();

            // -- RENDERING STATE: Progress and Halting --
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Status: RENDERING...");
            ImGui::Text("Progress: %d / %d Samples", current_sample.load(), ui_samples);
            
            // Progress bar
            float progress = (float)current_sample.load() / (float)ui_samples;
            ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Halt Render", ImVec2(-1, 40))) {
                cancel_request = true;
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::EndChild();

        ImGui::SameLine();

        // --- RIGHT PANEL: Render Viewport ---
        ImGui::BeginChild("Render View", ImVec2(0, 0), true); // 0,0 means "fill remaining space"
        
        if (render_width > 0 && render_height > 0) {
            // Calculate scaling to maintain aspect ratio
            ImVec2 avail_size = ImGui::GetContentRegionAvail();
            float aspect_ratio = (float)render_width / (float)render_height;
            
            ImVec2 image_size(avail_size.x, avail_size.x / aspect_ratio);
            if (image_size.y > avail_size.y) {
                image_size.y = avail_size.y;
                image_size.x = avail_size.y * aspect_ratio;
            }

            // Center the image in the available space
            ImVec2 cursor_pos = ImGui::GetCursorPos();
            cursor_pos.x += (avail_size.x - image_size.x) * 0.5f;
            cursor_pos.y += (avail_size.y - image_size.y) * 0.5f;
            ImGui::SetCursorPos(cursor_pos);

            // Draw the image
            ImGui::Image((void*)(intptr_t)render_texture, image_size);
        } else {
            // Draw a helpful placeholder text right in the middle of the empty space
            ImGui::SetWindowFontScale(2.5f);
            const char placehoder_text[] = "No Scene Rendered";
            ImVec2 text_size = ImGui::CalcTextSize(placehoder_text);
            ImVec2 avail_size = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos(ImVec2((avail_size.x - text_size.x) * 0.5f, (avail_size.y - text_size.y) * 0.5f));
            ImGui::TextDisabled(placehoder_text);
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::EndChild();

        ImGui::End(); // End Main Workspace

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    if (app_state.load() == AppState::Rendering) { 
        cancel_request = true; 
    }
    if (render_thread.joinable()) {
        render_thread.join();
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}