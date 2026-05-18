#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pocketfft_hdronly.h"

#ifdef __GNUC__
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif

#define PI 3.14159265358979323846f

using cpx = std::complex<float>;

/* Globals */
static cpx *g_buf = nullptr;
static uint8_t *g_mask = nullptr;
static int g_width = 0, g_height = 0, g_initialized = 0;

// Precomputed spectral mask
static void build_mask(int width, int height)
{
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

            if (fx * fx + fy * fy < freq_thresh_sq)
            {
                g_mask[y * width + x] = 0;
                continue;
            }

            float ax = fabsf(fx);
            float ay = fabsf(fy);
            float ratio = (ax > ay) ? ay / (ax + 1e-12f) : ax / (ay + 1e-12f);

            g_mask[y * width + x] = (ratio > 0.85f) ? 1 : 0;
        }
    }
}

extern "C"
{

    void cleanup_resources(void)
    {
        free(g_buf);
        g_buf = nullptr;
        free(g_mask);
        g_mask = nullptr;
        g_width = 0;
        g_height = 0;
        g_initialized = 0;
    }

    int init_resources(int width, int height)
    {
        if (g_initialized && g_width == width && g_height == height)
            return 0;

        cleanup_resources();

        g_buf = (cpx *)malloc(sizeof(cpx) * width * height);
        g_mask = (uint8_t *)malloc((size_t)width * height);

        if (!g_buf || !g_mask)
        {
            cleanup_resources();
            return -1;
        }

        build_mask(width, height);

        g_width = width;
        g_height = height;
        g_initialized = 1;
        return 0;
    }

    // Main function
    EXPORT void remove_moire(unsigned char *fb_data, int width, int height, int stride,
                             float strength)
    {
        if (!fb_data)
            return;

        strength = strength < 0.f ? 0.f : strength > 1.f ? 1.f : strength;

        if (init_resources(width, height) != 0)
        {
            fprintf(stderr, "pocketfft init failed\n");
            return;
        }

        const int bpp = stride / width; // 1 for BB8, 4 for BBRGB32

        // Load grayscale + (-1)^(x+y) centering so DC lands at (W/2, H/2)
        for (int y = 0; y < height; y++)
        {
            const unsigned char *row = fb_data + y * stride;
            for (int x = 0; x < width; x++)
            {
                const unsigned char *px = row + x * bpp;
                float val;
                if (bpp == 1)
                    val = (float)px[0];
                else
                    val = (float)((77 * px[0] + 150 * px[1] + 29 * px[2]) >> 8);
                if ((x + y) & 1)
                    val = -val;
                g_buf[y * width + x] = cpx(val, 0.f);
            }
        }

        // Native 2D forward FFT over both axes in one call
        pocketfft::shape_t shape = {(size_t)height, (size_t)width};
        pocketfft::stride_t strd = {(ptrdiff_t)(width * sizeof(cpx)), (ptrdiff_t)sizeof(cpx)};
        pocketfft::shape_t axes = {0, 1};

        pocketfft::c2c(shape, strd, strd, axes, true, g_buf, g_buf, 1.0f);

        // Apply precomputed diagonal attenuation mask
        const float atten = 1.0f - (1.0f - 0.10f) * strength;
        const int total = width * height;

        for (int i = 0; i < total; i++)
            if (g_mask[i])
                g_buf[i] *= atten;

        // Native 2D inverse FFT — normalization (1/total) is passed
        // directly as the scale factor
        pocketfft::c2c(shape, strd, strd, axes, false, g_buf, g_buf, 1.0f / (float)total);

        // Write back - undo centering, clamp, store to RGBA
        for (int y = 0; y < height; y++)
        {
            unsigned char *row = fb_data + y * stride;
            for (int x = 0; x < width; x++)
            {
                float val = g_buf[y * width + x].real();
                if ((x + y) & 1)
                    val = -val;
                if (std::isnan(val) || std::isinf(val))
                    val = 0.f;

                int p = (int)val;
                if (p < 0)
                    p = 0;
                if (p > 255)
                    p = 255;

                unsigned char *px = row + x * bpp;
                if (bpp == 1)
                    px[0] = (unsigned char)p;
                else
                    px[0] = px[1] = px[2] = (unsigned char)p;
            }
        }
    }

    EXPORT int init_moire_resources(void) { return 0; }
    EXPORT void cleanup_moire_resources(void) { cleanup_resources(); }
}
