/* Now double buffered with animation.
 */

#include <ppu-lv2.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <limits>

#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <vectormath/cpp/vectormath_aos.h>
#include <io/pad.h>

#include "rsxutil.h"
#include "PS3Printer.h"
#include "utilities.h"
#include "ray.h"
#include "hittable.h"

// Constants
constexpr s32 MAX_BUFFERS = 2;
constexpr f32 F32_MAX = std::numeric_limits<f32>::max();
constexpr f32 PI = 3.1415926535897932385;
constexpr u32 MAX_DEPTH = 100;

using Color = Vectormath::Aos::Vector3;

void writePixel(rsxBuffer *buffer, s32 i, s32 j, Color& colorVec3) {
  colorVec3 = Vectormath::Aos::sqrtPerElem(colorVec3); // For gamma correction
  u32 color = 0;
  color |= (u32(colorVec3.getX() * 255.999f) << 16);
  color |= (u32(colorVec3.getY() * 255.999f) << 8);
  color |= u32(colorVec3.getZ() * 255.999f);
  buffer->ptr[i * buffer->width + j] = color;
}

// Test for intersection with a sphere
//
f32 hitSphere(const Vectormath::Aos::Vector3 &center, f32 radius, const Ray &r) {
  const auto& oc = center - r.origin();
  const auto &a = Vectormath::Aos::lengthSqr(r.direction());
  const auto &h = Vectormath::Aos::dot(r.direction(), oc);
  const auto &c = Vectormath::Aos::lengthSqr(oc) - (radius * radius);
  const auto discriminant = (h * h) - (a * c);

  // Visualize normals
  if (discriminant < 0) {
    return -1.0f;
  }
  else {
    return (h - sqrt(discriminant)) / a;
  }
}

Color rayColor(const Ray &r, u32 depth, const HittableList &world) {
  if (depth == 0)
    return Color{0.0f, 0.0f, 0.0f};
  HitRecord hitRecord;
  if (world.hit(r, 0.001f, F32_MAX, hitRecord)) {
    Ray scattered;
    Color attenuation;
    if (hitRecord.scatterCallback(r, hitRecord, hitRecord.color, attenuation, scattered))
      return attenuation * rayColor(scattered, depth - 1, world);
    return Color{0.0f, 0.0f, 0.0f};
    // auto direction = randomOnHemisphere(hitRecord.normal) + hitRecord.normal;
    // return 0.5f * rayColor(Ray(hitRecord.hitPoint, direction), depth - 1, world);
    // return 0.5f * (hitRecord.normal + Color{1.0f, 1.0f, 1.0f});
    // return 0.5f * (direction + Color{1.0f, 1.0f, 1.0f});
  }

  const auto &unitDirection = Vectormath::Aos::normalize(r.direction());
  f32 a = 0.5f * (unitDirection.getY() + 1.0);
  return (1.0f - a) * Color{1.0f, 1.0f, 1.0f} + a * Color{0.5f, 0.7f, 1.0f};
}

void printStats(rsxBuffer *buffer, u64 frame) {
  constexpr f32 STATS_POS_X = 0.9f;
  constexpr f32 STATS_POS_Y = 0.01f;

  PS3Printer::SetFontColor(FONT_COLOR_BLACK);// no transparency
  PS3Printer::SetFont(PS3Printer::ARIAL, 14);
  PS3Printer::Print(STATS_POS_X, STATS_POS_Y, "Frame: " + LongToString(frame), buffer->ptr);
}

void printDebugInfo(rsxBuffer *buffer, const std::string &log) {
  constexpr f32 LOG_POS_X = 0.01f;
  constexpr f32 LOG_POS_Y = 0.7f;

  PS3Printer::SetFontColor(FONT_COLOR_RED);// no transparency
  PS3Printer::SetFont(PS3Printer::ARIAL, 14);
  PS3Printer::Print(LOG_POS_X, LOG_POS_Y, log, buffer->ptr);
}

void drawGradient(rsxBuffer *buffer) {
  s32 i, j;
  s32 color = 0;
  for (i = 0; i < buffer->height; ++i) {
    for(j = 0; j < buffer->width; ++j) {
      f32 r = ((f32) i) / (buffer->height);
      f32 g = ((f32) j) / (buffer->width);
      f32 b = 0.0f;

      s32 ir = (s32) 255.999 * r;
      s32 ig = (s32) 255.999 * g;
      s32 ib = (s32) 255.999 * b;
      color = 0;
      color |= ((ir << 16) | (ig << 8) | ib);
      buffer->ptr[i * buffer->width + j] = color;
    }
  }
}

int main(s32 argc, const char* argv[])
{
  gcmContextData *context;
  void *host_addr = NULL;
  rsxBuffer buffers[MAX_BUFFERS];
  int currentBuffer = 0;
  padInfo padinfo;
  padData paddata;
  u16 width;
  u16 height;
  int i;

  /* Allocate a 1Mb buffer, alligned to a 1Mb boundary
   * to be our shared IO memory with the RSX. */
  host_addr = memalign (1024*1024, HOST_SIZE);
  context = initScreen (host_addr, HOST_SIZE);
  ioPadInit(7);

  getResolution(&width, &height);
  for (i = 0; i < MAX_BUFFERS; i++)
    makeBuffer( &buffers[i], width, height, i);

  flip(context, MAX_BUFFERS - 1);

  u64 frame = 0; // To keep track of how many frames we have rendered.

  PS3Printer::Init(buffers[currentBuffer].width, buffers[currentBuffer].height);

  // Initialization of the Viewport
  f32 focalLength = 1.0f;
  f32 viewportHeight = 2.0f;
  f32 viewportWidth = viewportHeight * (f32(buffers[currentBuffer].width) / buffers[currentBuffer].height);
  Vectormath::Aos::Vector3 cameraCenter {0.0f, 0.0f, 0.0f};
  // Vectors across and down the Viewport
  Vectormath::Aos::Vector3 viewportU {viewportWidth, 0.0f, 0.0f};
  Vectormath::Aos::Vector3 viewportV {0.0f, -viewportHeight, 0.0f};
  // Delta vectors from pixel-to-pixel
  auto pixelDeltaU = viewportU / buffers[currentBuffer].width;
  auto pixelDeltaV = viewportV / buffers[currentBuffer].height;
  // Location of the upper left pixel
  auto viewportUpperLeft = cameraCenter - Vectormath::Aos::Vector3{0.0f, 0.0f, focalLength} - (viewportU / 2) - (viewportV / 2);
  auto pixel00Loc = viewportUpperLeft + 0.5f * (pixelDeltaU + pixelDeltaV);

  // Initialize the world
  const auto &materialGround = Color{0.8f, 0.8f, 0.0f};
  const auto &materialCenter = Color{0.1f, 0.2f, 0.5f};
  const auto &materialRight = Color{0.8f, 0.6f, 0.2f};
  const auto &materialBack = Color{0.5f, 0.2f, 0.6f};
  const auto &materialLeft = Color{0.8f, 0.8f, 0.8f};
  HittableList world;
  world.objects.emplace_back(Sphere{Vectormath::Aos::Vector3{0.0f, -100.5f, -1.0f}, 100.0f, materialGround, LAMBERTIAN});
  world.objects.emplace_back(Sphere{Vectormath::Aos::Vector3{0.0f, 0.0f, -1.0f}, 0.5f, materialCenter, LAMBERTIAN});
  world.objects.emplace_back(Sphere{Vectormath::Aos::Vector3{1.0f, 0.0f, -1.0f}, 0.5f, materialRight, METAL});
  world.objects.emplace_back(Sphere{Vectormath::Aos::Vector3{-1.0f, 0.0f, 1.0f}, 0.5f, materialBack, METAL});
  world.objects.emplace_back(Sphere{Vectormath::Aos::Vector3{-1.0f, 0.0f, -1.5f}, 0.5f, materialLeft, METAL});

  // Ok, everything is setup. Now for the main loop.
  while(1){
    // Check the pads.
    ioPadGetInfo(&padinfo);
    for(i=0; i<MAX_PADS; i++){
      if(padinfo.status[i]){
        ioPadGetData(i, &paddata);

        if(paddata.BTN_START){
        goto end;
        }
      }

    }

    waitFlip(); // Wait for the last flip to finish, so we can draw to the old buffer
    ++frame;
    // drawGradient(&buffers[currentBuffer]); // Draw into the unused buffer
    for (s32 i = 0; i < buffers[currentBuffer].height; ++i) {
      for (s32 j = 0; j < buffers[currentBuffer].width; ++j) {
        auto pixelCenter = pixel00Loc + (i * pixelDeltaV) + (j * pixelDeltaU);
        auto rayDirection = pixelCenter - cameraCenter;
        Ray r {cameraCenter, rayDirection};

        auto pixelColor = Color{0.0f, 0.0f, 0.0f};
        pixelColor += rayColor(r, MAX_DEPTH, world);
        writePixel(&buffers[currentBuffer], i, j, pixelColor);
      }
    }

    printStats(&buffers[currentBuffer], frame);

    flip(context, buffers[currentBuffer].id); // Flip buffer onto screen

    currentBuffer++;
    if (currentBuffer >= MAX_BUFFERS)
      currentBuffer = 0;
  }

end:

  gcmSetWaitFlip(context);
  for (i = 0; i < MAX_BUFFERS; i++)
    rsxFree(buffers[i].ptr);

  rsxFinish(context, 1);
  free(host_addr);
  ioPadEnd();

  return 0;
}
