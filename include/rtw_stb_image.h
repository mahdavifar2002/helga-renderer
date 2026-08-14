#ifndef RTW_STB_IMAGE_H
#define RTW_STB_IMAGE_H

#include <cstdlib>
#include <iostream>

class rtw_image {
  public:
    rtw_image() {}
    
    // Loads image data from the specified file.
    rtw_image(const char* image_filename) {
        if (!load(image_filename)) {
            std::cerr << "ERROR: Could not load image file: '" << image_filename << "'.\n";
        }
    }
    
    // Implementation in the `rtw_stb_image.cc`
    ~rtw_image();

    // Loads the linear (gamma=1) image data from the given file name. Returns true if the
    // load succeeded. The resulting data buffer contains the three [0.0, 1.0]
    // floating-point values for the first pixel (red, then green, then blue). Pixels are
    // contiguous, going left to right for the width of the image, followed by the next row
    // below, for the full height of the image.
    // Implementation in the `rtw_stb_image.cc`
    bool load(const std::string& filename);

    int width()  const {return (fdata == nullptr) ? 0 : image_width; }
    int height() const {return (fdata == nullptr) ? 0 : image_height; }

    // Returns the address of the three RGB bytes of the pixel at x,y. If there is no image
    // data, returns magenta.
    const unsigned char* pixel_data(int x, int y) const {
        static unsigned char magenta[] = { 255, 0, 255 };
        if (bdata == nullptr) return magenta;

        x = clamp(x, 0, image_width);
        y = clamp(y, 0, image_height);

        return bdata + y*bytes_per_scanline + x*bytes_per_pixel;
    }

  private:
    const int       bytes_per_pixel = 3;
    float          *fdata = nullptr;        // Linear floating point pixel data
    unsigned char  *bdata = nullptr;        // Linear 8-bit pixel data
    int             image_width = 0;        // Loaded image width
    int             image_height = 0;       // Loaded image height
    int             bytes_per_scanline = 0;

    // Returns the value clamped to the range [low, high).
    static int clamp(int x, int low, int high) {
        if (x < low) return low;
        if (x < high) return x;
        return high - 1;
    }

    static unsigned char float_to_byte(float value) {
        if (value <= 0.0)
            return 0;
        if (1.0 <= value)
            return 255;
        return static_cast<unsigned char>(256.0 * value);
    }

    // Convert the linear floating point pixel data to bytes, storing the resulting byte
    // data in the `bdata` member.
    void convert_to_bytes() {
        int total_bytes = image_width * image_height * bytes_per_pixel;
        bdata = new unsigned char[total_bytes];

        // Iterate through all pixel components, converting from [0.0, 1.0] float values to
        // unsigned [0, 255] byte values.

        auto *bptr = bdata;
        auto *fptr = fdata;
        for (auto i = 0; i < total_bytes; i++, bptr++, fptr++)
            *bptr = float_to_byte(*fptr);
    }
};

#endif