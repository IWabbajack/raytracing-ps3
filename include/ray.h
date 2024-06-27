#ifndef RAY_H
#define RAY_H

#include <vectormath/cpp/vectormath_aos.h>

class Ray {
public:
  Ray() {}

  Ray(const Vectormath::Aos::Vector3& origin, const Vectormath::Aos::Vector3& direction) : mOrigin(origin), mDir(direction) {}

  const Vectormath::Aos::Vector3& origin() const { return mOrigin; }
  const Vectormath::Aos::Vector3& direction() const { return mDir; }

  Vectormath::Aos::Vector3 at(f32 t) const {
    return mOrigin + (t * mDir);
  }

private:
  Vectormath::Aos::Vector3 mOrigin;
  Vectormath::Aos::Vector3 mDir;
};

#endif // RAY_H
