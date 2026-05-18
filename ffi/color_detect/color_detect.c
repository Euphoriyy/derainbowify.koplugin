#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#ifdef __GNUC__
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif

static inline bool is_pixel_colored(uint8_t r, uint8_t g, uint8_t b, int tolerance)
{
    return (abs(r - g) > tolerance) || (abs(r - b) > tolerance) || (abs(g - b) > tolerance);
}

/* ============================================================
 * Scalar path
 * ============================================================
 */

static bool is_block_colored_scalar(const uint8_t *data, int stride, int x_start, int y_start,
                                    int block_width, int block_height, int img_width,
                                    int img_height, int bpp, int tolerance)
{
    for (int y = y_start; y < y_start + block_height && y < img_height; y++)
    {
        const uint8_t *row = data + (y * stride);

        for (int x = x_start; x < x_start + block_width && x < img_width; x++)
        {
            const uint8_t *px = row + (x * bpp);

            /*
             * RGBA ordering
             */
            uint8_t r = px[0];
            uint8_t g = px[1];
            uint8_t b = px[2];

            if (is_pixel_colored(r, g, b, tolerance))
            {
                return true;
            }
        }
    }

    return false;
}

#ifdef __ARM_NEON

/* ============================================================
 * NEON path
 * ============================================================
 */

static bool is_block_colored_neon(const uint8_t *data, int stride, int x_start, int y_start,
                                  int block_width, int block_height, int img_width, int img_height,
                                  int tolerance)
{
    const uint8_t tol = (uint8_t)tolerance;

    uint8x8_t tolerance_vec = vdup_n_u8(tol);

    for (int y = y_start; y < y_start + block_height && y < img_height; y++)
    {
        const uint8_t *row = data + (y * stride);

        for (int x = x_start; x < x_start + block_width && x < img_width; x += 8)
        {
            if (x + 8 > img_width)
            {

                for (int i = x; i < img_width; i++)
                {

                    const uint8_t *px = row + (i * 4);

                    uint8_t r = px[0];
                    uint8_t g = px[1];
                    uint8_t b = px[2];

                    if (is_pixel_colored(r, g, b, tolerance))
                    {
                        return true;
                    }
                }

                break;
            }

            __builtin_prefetch(row + ((x + 16) * 4), 0, 0);

            /*
             * 4 channels interleaved
             */
            uint8x8x4_t pixels = vld4_u8(row + (x * 4));

            /*
             * RGBA ordering
             */
            uint8x8_t r_channel = pixels.val[0];
            uint8x8_t g_channel = pixels.val[1];
            uint8x8_t b_channel = pixels.val[2];

            uint8x8_t diff_rg = vabd_u8(r_channel, g_channel);
            uint8x8_t diff_rb = vabd_u8(r_channel, b_channel);
            uint8x8_t diff_gb = vabd_u8(g_channel, b_channel);

            uint8x8_t mask_rg = vcgt_u8(diff_rg, tolerance_vec);
            uint8x8_t mask_rb = vcgt_u8(diff_rb, tolerance_vec);
            uint8x8_t mask_gb = vcgt_u8(diff_gb, tolerance_vec);

            uint8x8_t mask = vorr_u8(vorr_u8(mask_rg, mask_rb), mask_gb);

            uint64_t mask_val = vget_lane_u64(vreinterpret_u64_u8(mask), 0);

            if (mask_val != 0)
            {
                return true;
            }
        }
    }

    return false;
}

#endif

/* ============================================================
 * Public API
 * ============================================================
 */

EXPORT bool is_page_colored(uint8_t *data, int width, int height, int stride, int tolerance)
{
    const int BLOCK_WIDTH = 8;
    const int BLOCK_HEIGHT = 16;
    const int bpp = stride / width; // 1 for BB8, 4 for BBRGB32
    
    if (bpp == 1)
    {
        return false;
    }

    for (int y = 0; y < height; y += BLOCK_HEIGHT)
    {
        for (int x = 0; x < width; x += BLOCK_WIDTH)
        {
#ifdef __ARM_NEON
            if (is_block_colored_neon(data, stride, x, y, BLOCK_WIDTH, BLOCK_HEIGHT, width, height,
                                      tolerance))
#else
            if (is_block_colored_scalar(data, stride, x, y, BLOCK_WIDTH, BLOCK_HEIGHT, width,
                                        height, bpp, tolerance))
#endif
                return true;
        }
    }

    return false;
}
