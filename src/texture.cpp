#include "texture.h"
#include "CGL/color.h"

#include <cmath>
#include <algorithm>

namespace CGL {

Color Texture::sample(const SampleParams &sp) {
  // Parts 5 and 6: Fill this in.
  // Should return a color sampled based on the psm and lsm parameters given

  if (sp.lsm == L_ZERO) {
    return sample_level(sp.p_uv, 0, sp.psm);
  }

  float level = get_level(sp);
  if (sp.lsm == L_NEAREST) {
    int level = round(get_level(sp));
    return sample_level(sp.p_uv, level, sp.psm);
  } else if (sp.lsm == L_LINEAR) {
    float level = get_level(sp);
    int level1 = floor(level);
    int level2 = ceil(level);
    Color c1 = sample_level(sp.p_uv, level1, sp.psm);
    Color c2 = sample_level(sp.p_uv, level2, sp.psm);
    return c1 + (c2 - c1) * (level - level1) / (level2 - level1);
  }

}

Color Texture::sample_level(Vector2D uv, int level, const PixelSampleMethod method) {
  if (method == P_NEAREST) {
    return sample_nearest(uv, level);
  } else if (method == P_LINEAR) {
    return sample_bilinear(uv, level);
  }
}

float Texture::get_level(const SampleParams &sp) {
  // Optional helper function for Parts 5 and 6
  Vector2D dx = sp.p_dx_uv - sp.p_uv;
  Vector2D scaled_dx = Vector2D(dx.x * width, dx.y * height);
  Vector2D dy = sp.p_dy_uv - sp.p_uv;
  Vector2D scaled_dy = Vector2D(dy.x * width, dy.y * height);
  float l = std::max(scaled_dx.norm(), scaled_dy.norm());
  return log2(l);
}

// Returns the nearest sample given a particular level and set of uv coords
Color Texture::sample_nearest(Vector2D uv, int level) {
  // Optional helper function for Parts 5 and 6
  // Feel free to ignore or create your own
  MipLevel ml = mipmap[level];
  int round_x = round(uv[0]*width/pow(2, level));
  int round_y = round(uv[1]*height/pow(2, level));
  return ml.get_texel(round_x, round_y);
}

// Returns the bilinear sample given a particular level and set of uv coords
Color Texture::sample_bilinear(Vector2D uv, int level) {
  // Optional helper function for Parts 5 and 6
  // Feel free to ignore or create your own
  MipLevel ml = mipmap[level];

  Vector2D scaled_uv = Vector2D(uv.x*width/pow(2, level), uv.y*height/pow(2, level));
  Vector2D round_ru = Vector2D(round(uv.x*width/pow(2, level)), round(uv.y*height/pow(2, level)));

  Vector2D lu = bounce(round_ru + Vector2D(-0.5, 0.5), level);
  Vector2D rl = bounce(round_ru + Vector2D(0.5, -0.5), level);
  Vector2D ll = bounce(round_ru + Vector2D(-0.5, -0.5), level);
  Vector2D ru = bounce(round_ru + Vector2D(0.5, 0.5), level);

  Color left_upper = ml.get_texel(floor(lu.x), floor(lu.y));
  Color right_upper = ml.get_texel(floor(ru.x), floor(ru.y));
  Color left_lower = ml.get_texel(floor(ll.x), floor(ll.y));
  Color right_lower = ml.get_texel(floor(rl.x), floor(rl.y));

  Color upper_lerp = linear_interpolate(left_upper, right_upper, lu, ru, Vector2D(scaled_uv.x, lu.y));
  Color lower_lerp = linear_interpolate(left_lower, right_lower, ll, rl, Vector2D(scaled_uv.x, rl.y));
  return linear_interpolate(lower_lerp, upper_lerp, Vector2D(scaled_uv.x, rl.y), Vector2D(scaled_uv.x, lu.y), scaled_uv);
}

Vector2D Texture::bounce(Vector2D vec, int level) {
  float bounced_x = std::min({std::max({vec.x, 0.5}), width/pow(2, level) - 0.5});
  float bounced_y = std::min({std::max({vec.y, 0.5}), height/pow(2, level) - 0.5});
  return Vector2D(bounced_x, bounced_y);
}

Color Texture::linear_interpolate(Color c1, Color c2, Vector2D p1, Vector2D p2, Vector2D x) {
  if (p1 == p2) {
    return c1;
  }
  return c1 + (c2 - c1) * (x - p1).norm() / (p2 - p1).norm();
}

// Color Texture::lerp_color(Color a, Color b,

/****************************************************************************/



inline void uint8_to_float(float dst[3], unsigned char *src) {
  uint8_t *src_uint8 = (uint8_t *)src;
  dst[0] = src_uint8[0] / 255.f;
  dst[1] = src_uint8[1] / 255.f;
  dst[2] = src_uint8[2] / 255.f;
}

inline void float_to_uint8(unsigned char *dst, float src[3]) {
  uint8_t *dst_uint8 = (uint8_t *)dst;
  dst_uint8[0] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[0])));
  dst_uint8[1] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[1])));
  dst_uint8[2] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[2])));
}

void Texture::generate_mips(int startLevel) {

  // make sure there's a valid texture
  if (startLevel >= mipmap.size()) {
    std::cerr << "Invalid start level";
  }

  // allocate sublevels
  int baseWidth = mipmap[startLevel].width;
  int baseHeight = mipmap[startLevel].height;
  int numSubLevels = (int)(log2f((float)max(baseWidth, baseHeight)));

  numSubLevels = min(numSubLevels, kMaxMipLevels - startLevel - 1);
  mipmap.resize(startLevel + numSubLevels + 1);

  int width = baseWidth;
  int height = baseHeight;
  for (int i = 1; i <= numSubLevels; i++) {

    MipLevel &level = mipmap[startLevel + i];

    // handle odd size texture by rounding down
    width = max(1, width / 2);
    //assert (width > 0);
    height = max(1, height / 2);
    //assert (height > 0);

    level.width = width;
    level.height = height;
    level.texels = vector<unsigned char>(3 * width * height);
  }

  // create mips
  int subLevels = numSubLevels - (startLevel + 1);
  for (int mipLevel = startLevel + 1; mipLevel < startLevel + subLevels + 1;
       mipLevel++) {

    MipLevel &prevLevel = mipmap[mipLevel - 1];
    MipLevel &currLevel = mipmap[mipLevel];

    int prevLevelPitch = prevLevel.width * 3; // 32 bit RGB
    int currLevelPitch = currLevel.width * 3; // 32 bit RGB

    unsigned char *prevLevelMem;
    unsigned char *currLevelMem;

    currLevelMem = (unsigned char *)&currLevel.texels[0];
    prevLevelMem = (unsigned char *)&prevLevel.texels[0];

    float wDecimal, wNorm, wWeight[3];
    int wSupport;
    float hDecimal, hNorm, hWeight[3];
    int hSupport;

    float result[3];
    float input[3];

    // conditional differentiates no rounding case from round down case
    if (prevLevel.width & 1) {
      wSupport = 3;
      wDecimal = 1.0f / (float)currLevel.width;
    } else {
      wSupport = 2;
      wDecimal = 0.0f;
    }

    // conditional differentiates no rounding case from round down case
    if (prevLevel.height & 1) {
      hSupport = 3;
      hDecimal = 1.0f / (float)currLevel.height;
    } else {
      hSupport = 2;
      hDecimal = 0.0f;
    }

    wNorm = 1.0f / (2.0f + wDecimal);
    hNorm = 1.0f / (2.0f + hDecimal);

    // case 1: reduction only in horizontal size (vertical size is 1)
    if (currLevel.height == prevLevel.height) {
      //assert (currLevel.height == 1);

      for (int i = 0; i < currLevel.width; i++) {
        wWeight[0] = wNorm * (1.0f - wDecimal * i);
        wWeight[1] = wNorm * 1.0f;
        wWeight[2] = wNorm * wDecimal * (i + 1);

        result[0] = result[1] = result[2] = 0.0f;

        for (int ii = 0; ii < wSupport; ii++) {
          uint8_to_float(input, prevLevelMem + 3 * (2 * i + ii));
          result[0] += wWeight[ii] * input[0];
          result[1] += wWeight[ii] * input[1];
          result[2] += wWeight[ii] * input[2];
        }

        // convert back to format of the texture
        float_to_uint8(currLevelMem + (3 * i), result);
      }

      // case 2: reduction only in vertical size (horizontal size is 1)
    } else if (currLevel.width == prevLevel.width) {
      //assert (currLevel.width == 1);

      for (int j = 0; j < currLevel.height; j++) {
        hWeight[0] = hNorm * (1.0f - hDecimal * j);
        hWeight[1] = hNorm;
        hWeight[2] = hNorm * hDecimal * (j + 1);

        result[0] = result[1] = result[2] = 0.0f;
        for (int jj = 0; jj < hSupport; jj++) {
          uint8_to_float(input, prevLevelMem + prevLevelPitch * (2 * j + jj));
          result[0] += hWeight[jj] * input[0];
          result[1] += hWeight[jj] * input[1];
          result[2] += hWeight[jj] * input[2];
        }

        // convert back to format of the texture
        float_to_uint8(currLevelMem + (currLevelPitch * j), result);
      }

      // case 3: reduction in both horizontal and vertical size
    } else {

      for (int j = 0; j < currLevel.height; j++) {
        hWeight[0] = hNorm * (1.0f - hDecimal * j);
        hWeight[1] = hNorm;
        hWeight[2] = hNorm * hDecimal * (j + 1);

        for (int i = 0; i < currLevel.width; i++) {
          wWeight[0] = wNorm * (1.0f - wDecimal * i);
          wWeight[1] = wNorm * 1.0f;
          wWeight[2] = wNorm * wDecimal * (i + 1);

          result[0] = result[1] = result[2] = 0.0f;

          // convolve source image with a trapezoidal filter.
          // in the case of no rounding this is just a box filter of width 2.
          // in the general case, the support region is 3x3.
          for (int jj = 0; jj < hSupport; jj++)
            for (int ii = 0; ii < wSupport; ii++) {
              float weight = hWeight[jj] * wWeight[ii];
              uint8_to_float(input, prevLevelMem +
                                        prevLevelPitch * (2 * j + jj) +
                                        3 * (2 * i + ii));
              result[0] += weight * input[0];
              result[1] += weight * input[1];
              result[2] += weight * input[2];
            }

          // convert back to format of the texture
          float_to_uint8(currLevelMem + currLevelPitch * j + 3 * i, result);
        }
      }
    }
  }
}

}
