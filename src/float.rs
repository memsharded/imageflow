
struct FloatBuffer {
  w, h, channels, float_count, float_stride: u32;
  alpha_premultiplied: bool;
  alpha_meaningful: bool;
  pixels_borrowed: bool;
  
}
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

