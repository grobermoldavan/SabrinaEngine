#ifndef _SE_COLOR_HPP_
#define _SE_COLOR_HPP_

using SeColorPacked = uint32_t;
using SeColorUnpacked = SeFloat4;

namespace col
{
    inline constexpr SeColorPacked pack(const SeColorUnpacked& color)
    {
        const uint32_t r = (uint32_t)(se_clamp(color.r, 0.0f, 1.0f) * 255.0f);
        const uint32_t g = (uint32_t)(se_clamp(color.g, 0.0f, 1.0f) * 255.0f);
        const uint32_t b = (uint32_t)(se_clamp(color.b, 0.0f, 1.0f) * 255.0f);
        const uint32_t a = (uint32_t)(se_clamp(color.a, 0.0f, 1.0f) * 255.0f);
        return r | (g << 8) | (b << 16) | (a << 24);
    }

    inline constexpr SeColorUnpacked unpack(SeColorPacked color)
    {
        return
        {
            ((float)(color       & 0xFF)) / 255.0f,
            ((float)(color >> 8  & 0xFF)) / 255.0f,
            ((float)(color >> 16 & 0xFF)) / 255.0f,
            ((float)(color >> 24 & 0xFF)) / 255.0f,
        };
    }
}

#endif
