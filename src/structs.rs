
struct BitmapBgra {
  w: u32;
  h: u32;
  
}


//non-indexed bitmap
typedef struct BitmapBgraStruct {

    //bitmap width in pixels
    uint32_t w;
    //bitmap height in pixels
    uint32_t h;
    //byte length of each row (may include any amount of padding)
    uint32_t stride;
    //pointer to pixel 0,0; should be of length > h * stride
    unsigned char *pixels;
    //If true, we don't dispose of *pixels when we dispose the struct
    bool borrowed_pixels;
    //If false, we can even ignore the alpha channel on 4bpp
    bool alpha_meaningful;
    //If false, we can edit pixels without affecting the stride
    bool pixels_readonly;
    //If false, we can change the stride of the image.
    bool stride_readonly;

    //If true, we can reuse the allocated memory for other purposes.
    bool can_reuse_space;

    BitmapPixelFormat fmt;

    //When using compositing mode blend_with_matte, this color will be used. We should probably define this as always being sRGBA, 4 bytes.
    uint8_t matte_color[4];

    BitmapCompositingMode compositing_mode;

} BitmapBgra;



typedef struct InterpolationDetailsStruct {
    //1 is the default; near-zero overlapping between windows. 2 overlaps 50% on each side.
    double window;
    //Coefficients for bucubic weighting
    double p1, p2, p3, q1, q2, q3, q4;
    //Blurring factor when > 1, sharpening factor when < 1. Applied to weights.
    double blur;

    //pointer to the weight calculation function
    detailed_interpolation_method filter;
    //How much sharpening we are requesting
    float sharpen_percent_goal;

} InterpolationDetails;


typedef struct ConvolutionKernelStruct {
    float * kernel;
    uint32_t width;
    uint32_t radius;
    float threshold_min_change; //These change values are on a somewhat arbitrary scale between 0 and 4;
    float threshold_max_change;
    float * buffer;
} ConvolutionKernel;



//floating-point bitmap, typically linear RGBA, premultiplied
typedef struct BitmapFloatStruct {
    //buffer width in pixels
    uint32_t w;
    //buffer height in pixels
    uint32_t h;
    //The number of floats per pixel
    uint32_t channels;
    //The pixel data
    float *pixels;
    //If true, don't dispose the buffer with the struct
    bool pixels_borrowed;
    //The number of floats in the buffer
    uint32_t float_count;
    //The number of floats betwen (0,0) and (0,1)
    uint32_t float_stride;

    //If true, alpha has been premultiplied
    bool alpha_premultiplied;
    //If true, the alpha channel holds meaningful data
    bool alpha_meaningful;
} BitmapFloat;


typedef struct _ColorspaceInfo {
    float byte_to_float[256]; //Converts 0..255 -> 0..1, but knowing that 0.255 has sRGB gamma.
    WorkingFloatspace floatspace;
    bool apply_srgb;
    bool apply_gamma;
    float gamma;
    float gamma_inverse;
#ifdef EXPOSE_SIGMOID
    SigmoidInfo sigmoid;
    bool apply_sigmoid;
#endif

} ColorspaceInfo;





typedef struct RenderDetailsStruct {
    //Interpolation and scaling details
    InterpolationDetails * interpolation;
    //How large the interoplation window needs to be before we even attempt to apply a sharpening
    //percentage to the given filter
    float minimum_sample_window_to_interposharpen;


    // If possible to do correctly, halve the image until it is [interpolate_last_percent] times larger than needed. 3 or greater reccomended. Specify -1 to disable halving.
    float interpolate_last_percent;

    //The number of pixels (in target canvas coordinates) that it is acceptable to discard for better halving performance
    float havling_acceptable_pixel_loss;

    //The actual halving factor to use.
    uint32_t halving_divisor;

    //The first convolution to apply
    ConvolutionKernel * kernel_a;
    //A second convolution to apply
    ConvolutionKernel * kernel_b;


    //If greater than 0, a percentage to sharpen the result along each axis;
    float sharpen_percent_goal;

    //If true, we should apply the color matrix
    bool apply_color_matrix;

    float color_matrix_data[25];
    float *color_matrix[5];

    //Transpose, flipx, flipy - combined, these give you all 90 interval rotations
    bool post_transpose;
    bool post_flip_x;
    bool post_flip_y;

    //Enables profiling
    bool enable_profiling;

} RenderDetails;
