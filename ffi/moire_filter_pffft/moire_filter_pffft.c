#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pffft.h"

#ifdef __GNUC__
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif

#define PI 3.14159265358979323846f

/* Globals */

static PFFFT_Setup *g_setup_w = NULL;
static PFFFT_Setup *g_setup_h = NULL;

static float *g_buf = NULL;
static float *g_transpose = NULL;
static float *g_work = NULL;

// Precomputed spectral mask
static uint8_t *g_mask = NULL;

static int g_width = 0;
static int g_height = 0;
static int g_initialized = 0;

/* Cleanup + Init */

void cleanup_resources(void)
{
    if (g_setup_w)
    {
        pffft_destroy_setup(g_setup_w);
        g_setup_w = NULL;
    }
    if (g_setup_h)
    {
        pffft_destroy_setup(g_setup_h);
        g_setup_h = NULL;
    }
    if (g_buf)
    {
        pffft_aligned_free(g_buf);
        g_buf = NULL;
    }
    if (g_transpose)
    {
        pffft_aligned_free(g_transpose);
        g_transpose = NULL;
    }
    if (g_work)
    {
        pffft_aligned_free(g_work);
        g_work = NULL;
    }
    if (g_mask)
    {
        free(g_mask);
        g_mask = NULL;
    }

    g_width = 0;
    g_height = 0;
    g_initialized = 0;
}

static int next_pffft_len(int n)
{
    int p = 32;
    while (p < n)
        p <<= 1;
    return p;
}

// Precompute spectral mask
static void build_mask(int width, int height)
{
    if (g_mask)
    {
        free(g_mask);
        g_mask = NULL;
    }

    g_mask = malloc(width * height);

    if (!g_mask)
        return;

    const int cx = width / 2;
    const int cy = height / 2;
    const float freq_threshold = 0.30f;
    const float freq_thresh_sq = freq_threshold * freq_threshold;
    for (int y = 0; y < height; y++)
    {
        float fy = (float)(y - cy) / (float)height;
        for (int x = 0; x < width; x++)
        {
            float fx = (float)(x - cx) / (float)width;
            float mag2 = fx * fx + fy * fy;

            if (mag2 < freq_thresh_sq)
            {
                g_mask[y * width + x] = 0;
                continue;
            }

            float ax = fabsf(fx);
            float ay = fabsf(fy);
            float ratio;

            if (ax > ay)
                ratio = ay / (ax + 1e-12f);
            else
                ratio = ax / (ay + 1e-12f);

            // Near diagonal
            g_mask[y * width + x] = (ratio > 0.85f);
        }
    }
}

int init_resources(int width, int height)
{
    if (g_initialized && g_width == width && g_height == height)
    {
        return 0;
    }

    cleanup_resources();

    int pw = next_pffft_len(width);
    int ph = next_pffft_len(height);

    g_setup_w = pffft_new_setup(pw, PFFFT_COMPLEX);
    g_setup_h = pffft_new_setup(ph, PFFFT_COMPLEX);

    if (!g_setup_w || !g_setup_h)
        goto fail;

    int total = pw * ph;
    int maxdim = (pw > ph) ? pw : ph;

    g_buf = pffft_aligned_malloc(sizeof(float) * total * 2);
    g_transpose = pffft_aligned_malloc(sizeof(float) * total * 2);
    g_work = pffft_aligned_malloc(sizeof(float) * maxdim * 2);

    if (!g_buf || !g_transpose || !g_work)
    {
        goto fail;
    }

    build_mask(pw, ph);

    g_width = pw;
    g_height = ph;
    g_initialized = 1;

    return 0;

fail:
    cleanup_resources();
    return -1;
}

/* Helpers */

static inline float *row_ptr(float *buf, int y, int width) { return &buf[y * width * 2]; }

// Cache-friendly transpose
static void transpose_complex(float *src, float *dst, int width, int height)
{
    const int BLOCK = 16;
    for (int by = 0; by < height; by += BLOCK)
    {
        for (int bx = 0; bx < width; bx += BLOCK)
        {
            int ymax = (by + BLOCK < height) ? by + BLOCK : height;
            int xmax = (bx + BLOCK < width) ? bx + BLOCK : width;
            for (int y = by; y < ymax; y++)
            {
                for (int x = bx; x < xmax; x++)
                {
                    int s = (y * width + x) * 2;
                    int d = (x * height + y) * 2;
                    dst[d] = src[s];
                    dst[d + 1] = src[s + 1];
                }
            }
        }
    }
}

// Fast spectral filter
static void filter_spectrum_angle(int width, int height, float strength)
{
    const float base_atten = 0.10f;
    const float atten = 1.0f - (1.0f - base_atten) * strength;
    const int total = width * height;
    for (int i = 0; i < total; i++)
    {
        if (g_mask[i])
        {
            g_buf[i * 2] *= atten;
            g_buf[i * 2 + 1] *= atten;
        }
    }
}

// Main function
EXPORT void remove_moire(unsigned char *fb_data, int width, int height, int stride, float strength)
{
    if (!fb_data)
        return;

    if (strength < 0.0f)
        strength = 0.0f;

    if (strength > 1.0f)
        strength = 1.0f;

    if (init_resources(width, height) != 0)
    {
        fprintf(stderr, "pffft init failed\n");
        return;
    }

    const int bpp = stride / width; // 1 for BB8, 4 for BBRGB32
    const int W = g_width;
    const int H = g_height;
    const int total = W * H;

    memset(g_buf, 0, sizeof(float) * total * 2);

    // Load grayscale + FFT centering
    for (int y = 0; y < height; y++)
    {
        unsigned char *row = fb_data + y * stride;
        for (int x = 0; x < width; x++)
        {
            unsigned char *px = row + x * bpp;

            // Integer grayscale approximation
            float value;
            if (bpp == 1)
                value = (float)px[0];
            else
                value = (float)((77 * px[0] + 150 * px[1] + 29 * px[2]) >> 8);

            // FFT centering
            if ((x + y) & 1)
                value = -value;
            int base = (y * W + x) * 2;
            g_buf[base] = value;
            g_buf[base + 1] = 0.0f;
        }
    }

    // Forward FFT
    for (int y = 0; y < H; y++)
    {
        float *r = &g_buf[y * W * 2];
        pffft_transform_ordered(g_setup_w, r, r, g_work, PFFFT_FORWARD);
    }

    transpose_complex(g_buf, g_transpose, W, H);

    for (int y = 0; y < W; y++)
    {
        float *r = &g_transpose[y * H * 2];
        pffft_transform_ordered(g_setup_h, r, r, g_work, PFFFT_FORWARD);
    }

    transpose_complex(g_transpose, g_buf, H, W);

    // Filter
    filter_spectrum_angle(W, H, strength);

    // Inverse FFT
    transpose_complex(g_buf, g_transpose, W, H);

    for (int y = 0; y < W; y++)
    {
        float *r = &g_transpose[y * H * 2];
        pffft_transform_ordered(g_setup_h, r, r, g_work, PFFFT_BACKWARD);
    }

    transpose_complex(g_transpose, g_buf, H, W);

    for (int y = 0; y < H; y++)
    {
        float *r = &g_buf[y * W * 2];
        pffft_transform_ordered(g_setup_w, r, r, g_work, PFFFT_BACKWARD);
    }

    // Normalize
    const float norm = 1.0f / (float)total;
    for (int y = 0; y < height; y++)
    {
        unsigned char *row = fb_data + y * stride;
        for (int x = 0; x < width; x++)
        {
            float value = g_buf[(y * W + x) * 2] * norm;

            if ((x + y) & 1)
                value = -value;

            if (isnan(value) || isinf(value))
            {
                value = 0.0f;
            }

            int pixel = (int)value;

            if (pixel < 0)
                pixel = 0;
            if (pixel > 255)
                pixel = 255;

            unsigned char *px = row + x * bpp;

            if (bpp == 1)
                px[0] = pixel;
            else
            {
                px[0] = pixel;
                px[1] = pixel;
                px[2] = pixel;
            }
        }
    }
}

EXPORT int init_moire_resources(void) { return 0; }

EXPORT void cleanup_moire_resources(void) { cleanup_resources(); }
