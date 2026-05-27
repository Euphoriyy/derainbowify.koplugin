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

// U and V chrominance channels preserved across FFT for color images
static float *g_u_channel = nullptr;
static float *g_v_channel = nullptr;

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
        free(g_u_channel);
        g_u_channel = nullptr;
        free(g_v_channel);
        g_v_channel = nullptr;
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

        // Chrominance buffers sized to the actual image pixels (not padded)
        g_u_channel = (float *)malloc(sizeof(float) * width * height);
        g_v_channel = (float *)malloc(sizeof(float) * width * height);

        if (!g_buf || !g_mask || !g_u_channel || !g_v_channel)
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
                             bool is_colored, float strength)
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

        // Native 2D forward FFT over both axes in one call
        pocketfft::shape_t shape = {(size_t)height, (size_t)width};
        pocketfft::stride_t strd = {(ptrdiff_t)(width * sizeof(cpx)), (ptrdiff_t)sizeof(cpx)};
        pocketfft::shape_t axes = {0, 1};

        if (is_colored && bpp == 4)
        {
            // Color path: RGB -> YUV, filter only Y (luminance), reconstruct RGB.
            // U and V are stored in side buffers and recombined after the IFFT.
            for (int y = 0; y < height; y++)
            {
                const unsigned char *row = fb_data + y * stride;
                for (int x = 0; x < width; x++)
                {
                    const unsigned char *px = row + x * 4;
                    float r = (float)px[0];
                    float g = (float)px[1];
                    float b = (float)px[2];

                    float Y = 0.299f * r + 0.587f * g + 0.114f * b;
                    float U = -0.14713f * r - 0.28886f * g + 0.436f * b;
                    float V = 0.615f * r - 0.51499f * g - 0.10001f * b;

                    int idx = y * width + x;
                    g_u_channel[idx] = U;
                    g_v_channel[idx] = V;

                    float val = ((x + y) & 1) ? -Y : Y;
                    g_buf[idx] = cpx(val, 0.f);
                }
            }

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

            // Normalize, undo centering, recombine YUV -> RGB
            for (int y = 0; y < height; y++)
            {
                unsigned char *row = fb_data + y * stride;
                for (int x = 0; x < width; x++)
                {
                    float Y = g_buf[y * width + x].real();
                    if ((x + y) & 1)
                        Y = -Y;
                    if (std::isnan(Y) || std::isinf(Y))
                        Y = 0.f;

                    int idx = y * width + x;
                    float U = g_u_channel[idx];
                    float V = g_v_channel[idx];

                    // YUV -> RGB
                    float rf = Y + 1.13983f * V;
                    float gf = Y - 0.39465f * U - 0.58060f * V;
                    float bf = Y + 2.03211f * U;

                    unsigned char *px = row + x * 4;
                    px[0] = (unsigned char)(rf < 0.f ? 0 : rf > 255.f ? 255 : (int)rf);
                    px[1] = (unsigned char)(gf < 0.f ? 0 : gf > 255.f ? 255 : (int)gf);
                    px[2] = (unsigned char)(bf < 0.f ? 0 : bf > 255.f ? 255 : (int)bf);
                    // px[3] alpha unchanged
                }
            }
        }
        else
        {
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
    }

    EXPORT int init_moire_resources(void) { return 0; }
    EXPORT void cleanup_moire_resources(void) { cleanup_resources(); }
}
