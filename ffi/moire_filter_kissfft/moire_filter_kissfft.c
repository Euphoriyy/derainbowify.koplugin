#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kiss_fft.h"
#include "kiss_fftnd.h"

#ifdef __GNUC__
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif

#define PI 3.14159265358979323846f

/*
 * ============================================================
 * Globals
 * ============================================================
 */

static kiss_fftnd_cfg g_fft_cfg = NULL;
static kiss_fftnd_cfg g_ifft_cfg = NULL;

static kiss_fft_cpx *g_fft_input = NULL;
static kiss_fft_cpx *g_fft_output = NULL;

// U and V chrominance channels preserved across FFT for color images
static float *g_u_channel = NULL;
static float *g_v_channel = NULL;

static int g_width = 0;
static int g_height = 0;
static int g_initialized = 0;

/*
 * ============================================================
 * Cleanup / Init
 * ============================================================
 */

void cleanup_resources(void)
{
    if (g_fft_cfg)
    {
        free(g_fft_cfg);
        g_fft_cfg = NULL;
    }
    if (g_ifft_cfg)
    {
        free(g_ifft_cfg);
        g_ifft_cfg = NULL;
    }
    if (g_fft_input)
    {
        free(g_fft_input);
        g_fft_input = NULL;
    }
    if (g_fft_output)
    {
        free(g_fft_output);
        g_fft_output = NULL;
    }
    if (g_u_channel)
    {
        free(g_u_channel);
        g_u_channel = NULL;
    }
    if (g_v_channel)
    {
        free(g_v_channel);
        g_v_channel = NULL;
    }
    g_width = 0;
    g_height = 0;
    g_initialized = 0;
}

int init_resources(int width, int height)
{
    if (g_initialized && g_width == width && g_height == height)
        return 0;

    cleanup_resources();

    int dims[2] = {height, width};

    g_fft_cfg = kiss_fftnd_alloc(dims, 2, 0, NULL, NULL);
    g_ifft_cfg = kiss_fftnd_alloc(dims, 2, 1, NULL, NULL);

    if (!g_fft_cfg || !g_ifft_cfg)
        goto fail;

    int total = width * height;
    g_fft_input = malloc(sizeof(kiss_fft_cpx) * total);
    g_fft_output = malloc(sizeof(kiss_fft_cpx) * total);

    // Chrominance buffers sized to the actual image pixels (not padded)
    g_u_channel = malloc(sizeof(float) * total);
    g_v_channel = malloc(sizeof(float) * total);

    if (!g_fft_input || !g_fft_output || !g_u_channel || !g_v_channel)
        goto fail;

    g_width = width;
    g_height = height;
    g_initialized = 1;
    return 0;

fail:
    cleanup_resources();
    return -1;
}

/*
 * ============================================================
 * Angle-based diagonal frequency filter
 *
 * Attenuates frequencies whose radial magnitude >= freq_threshold
 * AND whose angle is within angle_tolerance of the Kaleido 3 CFA
 * diagonal directions: 45 deg, 135 deg, 225 deg, 315 deg.
 * ============================================================
 */

static void filter_spectrum_angle(kiss_fft_cpx *spectrum, int width, int height, float strength)
{
    const int cx = width / 2;
    const int cy = height / 2;

    const float freq_threshold = 0.30f;
    const float freq_thresh_sq = freq_threshold * freq_threshold;
    const float base_atten = 0.10f; /* 90% reduction at strength=1 */
    const float angle_tol = 10.0f * (PI / 180.0f);

    /* strength=0 -> atten=1.0 (no change), strength=1 -> atten=0.10 */
    const float atten = 1.0f - (1.0f - base_atten) * strength;

    const float targets[4] = {
        45.0f * (PI / 180.0f),
        135.0f * (PI / 180.0f),
        225.0f * (PI / 180.0f),
        315.0f * (PI / 180.0f),
    };

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float fx = (float)(x - cx) / (float)width;
            float fy = (float)(y - cy) / (float)height;

            if (fx * fx + fy * fy < freq_thresh_sq)
                continue;

            float angle = atan2f(fy, fx);
            if (angle < 0.0f)
                angle += 2.0f * PI;

            for (int a = 0; a < 4; a++)
            {
                float diff = fabsf(angle - targets[a]);
                if (diff > PI)
                    diff = 2.0f * PI - diff;
                if (diff <= angle_tol)
                {
                    int idx = y * width + x;
                    spectrum[idx].r *= atten;
                    spectrum[idx].i *= atten;
                    break;
                }
            }
        }
    }
}

/*
 * ============================================================
 * Main entry point
 * ============================================================
 */

EXPORT void remove_moire(unsigned char *fb_data, int width, int height, int stride, bool is_colored,
                         float strength)
{
    if (!fb_data)
        return;

    strength = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength);

    if (init_resources(width, height) != 0)
    {
        fprintf(stderr, "kissfft init failed\n");
        return;
    }

    int total = width * height;
    int bpp = stride / width; // 1 for BB8, 4 for BBRGB32

    if (is_colored && bpp == 4)
    {
        // Color path: RGB -> YUV, filter only Y (luminance), reconstruct RGB.
        // U and V are stored in side buffers and recombined after the IFFT.
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                unsigned char *px = &fb_data[y * stride + x * 4];
                float r = (float)px[0];
                float g = (float)px[1];
                float b = (float)px[2];

                float Y = 0.299f * r + 0.587f * g + 0.114f * b;
                float U = -0.14713f * r - 0.28886f * g + 0.436f * b;
                float V = 0.615f * r - 0.51499f * g - 0.10001f * b;

                int idx = y * width + x;
                g_u_channel[idx] = U;
                g_v_channel[idx] = V;

                g_fft_input[idx].r = ((x + y) & 1) ? -Y : Y;
                g_fft_input[idx].i = 0.0f;
            }
        }

        kiss_fftnd(g_fft_cfg, g_fft_input, g_fft_output);

        filter_spectrum_angle(g_fft_output, width, height, strength);

        kiss_fftnd(g_ifft_cfg, g_fft_output, g_fft_input);

        // Normalize, undo centering, recombine YUV -> RGB
        float norm = 1.0f / (float)total;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                int idx = y * width + x;

                float Y = g_fft_input[idx].r * norm;
                if ((x + y) & 1)
                    Y = -Y;
                if (isnan(Y) || isinf(Y))
                    Y = 0.0f;

                float U = g_u_channel[idx];
                float V = g_v_channel[idx];

                // YUV -> RGB
                float rf = Y + 1.13983f * V;
                float gf = Y - 0.39465f * U - 0.58060f * V;
                float bf = Y + 2.03211f * U;

                unsigned char *px = &fb_data[y * stride + x * 4];
                px[0] = (unsigned char)(rf < 0.0f ? 0 : rf > 255.0f ? 255 : (int)rf);
                px[1] = (unsigned char)(gf < 0.0f ? 0 : gf > 255.0f ? 255 : (int)gf);
                px[2] = (unsigned char)(bf < 0.0f ? 0 : bf > 255.0f ? 255 : (int)bf);
            }
        }
    }
    else
    {
        // RGB -> grayscale, load into FFT input with (-1)^(x+y) centering
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                unsigned char *px = &fb_data[y * stride + x * bpp];
                float gray;
                if (bpp == 1)
                    gray = px[0];
                else
                    gray = 0.299f * px[0] + 0.587f * px[1] + 0.114f * px[2];

                int idx = y * width + x;
                g_fft_input[idx].r = ((x + y) & 1) ? -gray : gray;
                g_fft_input[idx].i = 0.0f;
            }
        }

        kiss_fftnd(g_fft_cfg, g_fft_input, g_fft_output);

        filter_spectrum_angle(g_fft_output, width, height, strength);

        kiss_fftnd(g_ifft_cfg, g_fft_output, g_fft_input);

        float norm = 1.0f / (float)total;

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                int idx = y * width + x;

                float value = g_fft_input[idx].r * norm;
                if ((x + y) & 1)
                    value = -value;
                if (isnan(value) || isinf(value))
                    value = 0.0f;

                int pixel = (int)value;
                if (pixel < 0)
                    pixel = 0;
                if (pixel > 255)
                    pixel = 255;

                unsigned char *px = &fb_data[y * stride + x * bpp];
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
}

/*
 * ============================================================
 * Optional init / cleanup
 * ============================================================
 */

EXPORT int init_moire_resources(void) { return 0; }
EXPORT void cleanup_moire_resources(void) { cleanup_resources(); }
