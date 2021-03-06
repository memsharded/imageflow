#include "helpers_visual.h"

struct named_checksum {
    char * name;
    char * checksum;
};

int64_t flow_getline(char ** lineptr, size_t * n, FILE * stream)
{
    char * bufptr = NULL;
    char * temp_bufptr = NULL;
    char * p = bufptr;
    size_t size;
    int c;

    if (lineptr == NULL) {
        return -1;
    }
    if (stream == NULL) {
        return -1;
    }
    if (n == NULL) {
        return -1;
    }
    bufptr = *lineptr;
    size = *n;

    c = fgetc(stream);
    if (c == EOF) {
        return -1;
    }
    if (bufptr == NULL) {
        bufptr = (char *)malloc(128);
        if (bufptr == NULL) {
            return -1;
        }
        size = 128;
    }
    p = bufptr;
    while (c != EOF) {
        if ((p - bufptr + 1) > (int64_t)size) {
            size = size + 128;
            temp_bufptr = (char *)realloc(bufptr, size);
            if (temp_bufptr == NULL) {
                return -1;
            } else {
                bufptr = temp_bufptr;
                p = p + (int64_t)(temp_bufptr - bufptr);
            }
        }
        *p++ = c;
        if (c == '\n') {
            break;
        }
        c = fgetc(stream);
    }

    *p++ = '\0';
    *lineptr = bufptr;
    *n = size;

    return p - bufptr - 1;
}

static bool load_checksums(flow_c * c, struct named_checksum ** checksums, size_t * checksum_count)
{
    static struct named_checksum * list = NULL;
    static size_t list_size = 0;

    if (list == NULL) {
        char filename[2048];
        if (!create_relative_path(c, false, filename, 2048, "/visuals/checksums.list")) {
            FLOW_add_to_callstack(c);
            return false;
        }
        FILE * fp;
        char * line_a = NULL;
        size_t len_a = 0;
        int64_t read_a;
        char * line_b = NULL;
        size_t len_b = 0;
        int64_t read_b;

        fp = fopen(filename, "r");
        if (fp == NULL) {
            FLOW_error(c, flow_status_IO_error);
            return false;
        }

        list_size = 200;
        list = (struct named_checksum *)calloc(list_size, sizeof(struct named_checksum));

        size_t index = 0;
        while (true) {
            // Read lines in pairs
            read_a = flow_getline(&line_a, &len_a, fp);
            if (read_a == -1) {
                break;
            }
            read_b = flow_getline(&line_b, &len_b, fp);
            if (read_b == -1) {
                free(line_a);
                break;
            }
            // Drop newlines if present
            if (line_a[read_a - 1] == '\n') {
                line_a[read_a - 1] = '\0';
            }
            if (line_b[read_b - 1] == '\n') {
                line_b[read_b - 1] = '\0';
            }
            // Save
            list[index].name = line_a;
            list[index].checksum = line_b;
            line_a = NULL;
            line_b = NULL;
            index++;
            if (index >= list_size) {
                FLOW_error_msg(c, flow_status_IO_error,
                               "Could not read in entire checksum file. Please increase list_size above %ul.",
                               list_size);
                fclose(fp);
                return false;
            }
        }
        list_size = index;
        fclose(fp);
    }
    *checksum_count = list_size;
    *checksums = list;

    return true;
}
bool append_checksum(flow_c * c, char checksum[34], const char * name);
bool append_checksum(flow_c * c, char checksum[34], const char * name)
{
    char filename[2048];
    if (!create_relative_path(c, true, filename, 2048, "/visuals/checksums.list")) {
        FLOW_add_to_callstack(c);
        return false;
    }
    FILE * fp = fopen(filename, "a");
    if (fp == NULL) {
        FLOW_error(c, flow_status_IO_error);
        return false;
    }
    fprintf(fp, "%s\n%s\n", name, &checksum[0]);
    fclose(fp);
    return true;
}

static bool checksum_bitmap(flow_c * c, struct flow_bitmap_bgra * bitmap, char * checksum_buffer,
                            size_t checksum_buffer_length)
{
    char info_buffer[256];
    flow_snprintf(&info_buffer[0], sizeof(info_buffer), "%dx%d fmt=%d alpha=%d", bitmap->w, bitmap->h, bitmap->fmt,
                  bitmap->alpha_meaningful);
    int64_t printed_chars = (flow_snprintf(checksum_buffer, checksum_buffer_length, "%016X_%016X",
                                           djb2_buffer((uint8_t *)bitmap->pixels, bitmap->stride * bitmap->h),
                                           djb2((unsigned const char *)&info_buffer[0])));

    return printed_chars != -1;
}

static char * get_checksum_for(flow_c * c, const char * name)
{
    struct named_checksum * checksums = NULL;
    size_t checksum_count = 0;

    if (!load_checksums(c, &checksums, &checksum_count)) {
        FLOW_add_to_callstack(c);
        return NULL;
    }
    for (size_t i = 0; i < checksum_count; i++) {
        if (strcmp(checksums[i].name, name) == 0) {
            return checksums[i].checksum;
        }
    }
    return NULL;
}

static bool download_by_checksum(flow_c * c, char * checksum)
{
    char filename[2048];
    if (!create_relative_path(c, true, filename, 2048, "/visuals/%s.png", checksum)) {
        FLOW_add_to_callstack(c);
        return false;
    }

    fprintf(stderr, "%s (trusted)\n", &filename[0]);
    if (access(filename, F_OK) != -1) {
        return true; // Already exists!
    }
    char url[2048];
    flow_snprintf(url, 2048, "http://s3-us-west-2.amazonaws.com/imageflow-resources/visual_test_checksums/%s.png",
                  checksum); // TODO: fix actual URL
    if (!fetch_image(url, filename)) {
        FLOW_add_to_callstack(c);
        return false;
    }
    return true;
}

static bool save_bitmap_to_visuals(flow_c * c, struct flow_bitmap_bgra * bitmap, char * checksum, char * name)
{
    char filename[2048];
    if (!create_relative_path(c, true, filename, 2048, "/visuals/%s.png", checksum)) {
        FLOW_add_to_callstack(c);
        return false;
    }
    if (access(filename, F_OK) != -1) {
        return true; // Already exists!
    }

    if (!write_frame_to_disk(c, &filename[0], bitmap)) {
        FLOW_add_to_callstack(c);
        return false;
    }
    fprintf(stderr, "%s (%s)\n", &filename[0], name);
    return true;
}

static double get_dssim_from_command(flow_c * c, const char * command)
{
    FILE * fd;
#ifdef _MSC_VER
    fd = _popen(command, "r");
#else
    fd = popen(command, "r");
#endif
    if (!fd)
        return 200;

    char buffer[256];
    size_t chread;
    /* String to store entire command contents in */
    size_t comalloc = 256;
    size_t comlen = 0;
    char * comout = (char *)FLOW_malloc(c, comalloc);

    /* Use fread so binary data is dealt with correctly */
    while ((chread = fread(buffer, 1, sizeof(buffer), fd)) != 0) {
        if (comlen + chread >= comalloc) {
            comalloc *= 2;
            comout = (char *)FLOW_realloc(c, comout, comalloc);
        }
        memmove(comout + comlen, buffer, chread);
        comlen += chread;
    }
#ifdef _MSC_VER
    int exit_code = _pclose(fd);
#else
    int exit_code = pclose(fd);
#endif
    /* We can now work with the output as we please. Just print
     * out to confirm output is as expected */
    // fwrite(comout, 1, comlen, stdout);
    double result = 125;
    if (exit_code == 0) {
        result = strtold(comout, NULL);
    }

    FLOW_free(c, comout);

    return result;
}

bool load_image(flow_c * c, char * checksum, struct flow_bitmap_bgra ** ref, void * bitmap_owner)
{
    char filename[2048];
    if (!create_relative_path(c, false, filename, 2048, "/visuals/%s.png", checksum)) {
        FLOW_add_to_callstack(c);
        return false;
    }

    struct flow_job * job = flow_job_create(c);
    if (job == NULL) {
        FLOW_error_return(c);
    }
    size_t bytes_count;
    uint8_t * bytes = read_all_bytes(c, &bytes_count, filename);
    if (bytes == NULL) {
        FLOW_error_return(c);
    }
    struct flow_io * input = flow_io_create_from_memory(c, flow_io_mode_read_seekable, bytes, bytes_count, job, NULL);
    if (input == NULL) {
        FLOW_error_return(c);
    }
    if (!flow_job_add_io(c, job, input, 0, FLOW_INPUT)) {
        FLOW_error_return(c);
    }

    struct flow_graph * g = flow_graph_create(c, 10, 10, 200, 2.0);
    if (g == NULL) {
        FLOW_add_to_callstack(c);
        return false;
    }
    int32_t last;
    last = flow_node_create_decoder(c, &g, -1, 0);
    last = flow_node_create_bitmap_bgra_reference(c, &g, last, ref);

    if (flow_context_has_error(c)) {
        FLOW_add_to_callstack(c);
        return false;
    }
    if (!flow_job_execute(c, job, &g)) {
        FLOW_add_to_callstack(c);
        return false;
    }

    // Let the bitmap last longer than the job
    if (!flow_set_owner(c, *ref, bitmap_owner)) {
        FLOW_add_to_callstack(c);
        return false;
    }

    if (!flow_job_destroy(c, job)) {
        FLOW_error_return(c);
    }
    FLOW_free(c, bytes);
    return true;
}
static bool diff_images(flow_c * c, char * checksum_a, char * checksum_b, double * out_dssim, bool generate_visual_diff)
{
    char filename_a[2048];
    if (!create_relative_path(c, false, filename_a, 2048, "/visuals/%s.png", checksum_a)) {
        FLOW_add_to_callstack(c);
        return false;
    }
    char filename_b[2048];
    if (!create_relative_path(c, false, filename_b, 2048, "/visuals/%s.png", checksum_b)) {
        FLOW_add_to_callstack(c);
        return false;
    }
    char filename_c[2048];
    if (!create_relative_path(c, false, filename_c, 2048, "/visuals/compare_%s_vs_%s.png", checksum_a, checksum_b)) {
        FLOW_add_to_callstack(c);
        return false;
    }

    char magick_command[4096];
    flow_snprintf(magick_command, 4096, "dssim %s %s", filename_b, filename_a);
    *out_dssim = get_dssim_from_command(c, magick_command);
    if (*out_dssim > 10 || *out_dssim < 0) {
        fprintf(stderr, "Failed to execute: %s", magick_command);
        *out_dssim = 2.23456;
        // FLOW_error(c, flow_status_IO_error);
    };
    if (generate_visual_diff) {
        fprintf(stderr, "%s\n", &filename_c[0]);

        if (access(filename_c, F_OK) != -1) {
            return true; // Already exists!
        }
        flow_snprintf(magick_command, 4096, "composite %s %s -compose difference %s", filename_a, filename_b,
                      filename_c);

        int result = system(magick_command);
        if (result != 0) {
            fprintf(stderr, "unhappy imagemagick\n");
        }
    }
    return true;
}

static bool append_html(flow_c * c, const char * name, const char * checksum_a, const char * checksum_b)
{
    char filename[2048];
    if (!create_relative_path(c, true, filename, 2048, "/visuals/visuals.html")) {
        FLOW_add_to_callstack(c);
        return false;
    }
    static bool first_write = true;

    FILE * fp = fopen(filename, first_write ? "w" : "a");
    if (fp == NULL) {
        FLOW_error(c, flow_status_IO_error);
        return false;
    }
    if (first_write) {
        // Write the header here
    }
    if (checksum_b == NULL) {
        fprintf(fp, "<h1>%s</h2>\n<img class=\"current\" src=\"%s.png\"/>", name, checksum_a);
    } else {
        fprintf(fp, "<h1>%s</h2>\n<img class=\"current\" src=\"%s.png\"/><img class=\"correct\" src=\"%s.png\"/><img "
                    "class=\"diff\" src=\"compare_%s_vs_%s.png\"/>",
                name, checksum_a, checksum_b, checksum_a, checksum_b);
    }

    fclose(fp);
    first_write = false;
    return true;
}

bool diff_image_pixels(flow_c * c, struct flow_bitmap_bgra * a, struct flow_bitmap_bgra * b, size_t * diff_count,
                       size_t * total_delta, size_t print_this_many_differences, size_t stop_counting_at)
{
    if (a->w != b->w || a->h != b->h || a->fmt != b->fmt || a->fmt != flow_bgra32) {
        FLOW_error(c, flow_status_Invalid_argument);
        return false;
    }
    *diff_count = 0;
    *total_delta = 0;
    for (size_t pixel_index = 0; pixel_index < a->h * a->stride; pixel_index++) {
        if (a->pixels[pixel_index] != b->pixels[pixel_index]) {
            int x = (pixel_index % (a->stride)) / 4;
            int y = pixel_index / a->stride;
            int channel = pixel_index % 4;
            const char channels[] = { "BGRA" };
            if (*diff_count < print_this_many_differences) {
                fprintf(stderr, " (%d,%d) %ca=%d %cb=%d ", x, y, channels[channel], a->pixels[pixel_index],
                        channels[channel], b->pixels[pixel_index]);
            }
            (*diff_count)++;
            (*total_delta) += abs(a->pixels[pixel_index] - b->pixels[pixel_index]);
            if (stop_counting_at == *diff_count)
                return true; // We stop comparing after X many differences
        }
    }
    return true;
}

bool visual_compare(flow_c * c, struct flow_bitmap_bgra * bitmap, const char * name, bool store_checksums,
                    size_t off_by_one_byte_differences_permitted, const char * file_, const char * func_,
                    int line_number)
{

    char checksum[34];

    // compute checksum of bitmap (two checksums, actually - one for configuration, another for bitmap bytes)
    if (!checksum_bitmap(c, bitmap, checksum, 34)) {
        FLOW_error(c, flow_status_Invalid_argument);
        return false;
    }
    // Load stored checksum
    char * stored_checksum = get_checksum_for(c, name);

    // Compare
    if (stored_checksum != NULL && strcmp(checksum, stored_checksum) == 0) {
        // Make sure the file is created for later
        if (!save_bitmap_to_visuals(c, bitmap, checksum, "trusted")) {
            FLOW_error_return(c);
        }
        return true; // It matches!
    }

    if (stored_checksum == NULL) {
        // No stored checksum for this name
        if (store_checksums) {
            if (!append_checksum(c, checksum, name)) {
                FLOW_error_return(c);
            }
            fprintf(stderr, "===============\n%s\nStoring checksum %s, since FLOW_STORE_CHECKSUMS was set.\n ", name,
                    &checksum[0]);
        } else {
            fprintf(stderr, "===============\n%s\nThere is no stored checksum for this test; #define "
                            "FLOW_STORE_CHECKSUMS and rerun to set the initial value to %s.\n ",
                    name, &checksum[0]);
        }

        fprintf(stderr, "%s:%d in function %s\n", file_, line_number, func_);
    } else {
        fprintf(stderr, "===============\n%s\nThe stored checksum [%s] differs from the current result [%s]. Open "
                        "visuals/visuals.html to comapre.\n ",
                name, stored_checksum, checksum);
        fprintf(stderr, "%s:%d in function %s\n", file_, line_number, func_);
    }

    // The hash differs
    // Save ours so we can see it
    if (!save_bitmap_to_visuals(c, bitmap, checksum, "current")) {
        FLOW_error_return(c);
    }

    if (stored_checksum != NULL) {
        // Try to download "old" png from S3 using the checksums as an address.
        if (!download_by_checksum(c, stored_checksum)) {
            FLOW_error_return(c);
        }

        // First try a byte-by-byte comparison to eliminate floating-point error
        struct flow_bitmap_bgra * old;
        if (!load_image(c, stored_checksum, &old, c)) {
            FLOW_error_return(c);
        }
        size_t differences;
        size_t total_delta;
        if (!diff_image_pixels(c, old, bitmap, &differences, &total_delta, 0,
                               off_by_one_byte_differences_permitted + 4096)) {
            FLOW_error_return(c);
        }
        // If the difference is a handful of off-by-one rounding changes, just print the different bytes.
        if (differences < off_by_one_byte_differences_permitted && total_delta == differences) {
            if (!diff_image_pixels(c, old, bitmap, &differences, &total_delta, 100,
                                   off_by_one_byte_differences_permitted + 4096)) {
                FLOW_error_return(c);
            }
            fprintf(stderr, "\nA total of %lu bytes (of %lu) differed between the bitmaps (off by one). Less than "
                            "failure threshold of %lu\n",
                    differences, (size_t)old->stride * (size_t)old->h, off_by_one_byte_differences_permitted);
            return true;
        } else {

            double dssim;
            // Diff the two, generate a third PNG. Also get PSNR metrics from imagemagick
            if (!diff_images(c, checksum, stored_checksum, &dssim, true)) {
                FLOW_error_return(c);
            }
            fprintf(stdout, "DSSIM %.20f between %s and %s\n", dssim, stored_checksum, checksum);

            // Dump to HTML=
            if (!append_html(c, name, checksum, stored_checksum)) {
                FLOW_error_return(c);
            }
        }
        flow_bitmap_bgra_destroy(c, old);
    }

    return false;
}

bool visual_compare_two(flow_c * c, struct flow_bitmap_bgra * a, struct flow_bitmap_bgra * b,
                        const char * comparison_title, double * out_dssim, bool save_bitmaps, bool generate_visual_diff,
                        const char * file_, const char * func_, int line_number)
{

    char checksum_a[34];

    char checksum_b[34];
    // compute checksum of bitmap (two checksums, actually - one for configuration, another for bitmap bytes)
    if (!checksum_bitmap(c, a, checksum_a, 34)) {
        FLOW_error(c, flow_status_Invalid_argument);
        return false;
    }
    if (!checksum_bitmap(c, b, checksum_b, 34)) {
        FLOW_error(c, flow_status_Invalid_argument);
        return false;
    }

    // Compare
    if (strcmp(checksum_a, checksum_b) == 0) {
        if (memcmp(a->pixels, b->pixels, a->stride * a->h) == 0) {
            *out_dssim = 0;
            return true; // It matches!
        } else {
            // Checksum collsion
            exit(901);
        }
    }
    if (save_bitmaps) {
        // They differ
        if (!save_bitmap_to_visuals(c, a, checksum_a, "A")) {
            FLOW_error_return(c);
        }
        if (!save_bitmap_to_visuals(c, b, checksum_b, "B")) {
            FLOW_error_return(c);
        }
        // Diff the two, generate a third PNG. Also get PSNR metrics from imagemagick
        if (!diff_images(c, checksum_a, checksum_b, out_dssim, generate_visual_diff && save_bitmaps)) {
            FLOW_error_return(c);
        }
        // Dump to HTML=
        if (!append_html(c, comparison_title, checksum_a, checksum_b)) {
            FLOW_error_return(c);
        }
    }

    return false;
}

bool get_image_dimensions(flow_c * c, uint8_t * bytes, size_t bytes_count, int32_t * width, int32_t * height)
{
    struct flow_job * job = flow_job_create(c);
    if (job == NULL) {
        FLOW_error_return(c);
    }

    struct flow_io * input = flow_io_create_from_memory(c, flow_io_mode_read_seekable, bytes, bytes_count, job, NULL);
    if (input == NULL) {
        FLOW_error_return(c);
    }
    if (!flow_job_add_io(c, job, input, 0, FLOW_INPUT)) {
    }
    struct flow_decoder_info info;
    if (!flow_job_get_decoder_info(c, job, 0, &info)) {
        FLOW_error_return(c);
    }
    *width = info.frame0_width;
    *height = info.frame0_height;
    if (!flow_job_destroy(c, job)) {
        FLOW_error_return(c);
    }
    return true;
}
