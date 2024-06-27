#ifndef HITTABLE_H
#define HITTABLE_H

#include <cmath>
#include <iterator>
#include <vectormath/cpp/vectormath_aos.h>
#include <math.h>
#include <vector>
#include <array>
#include <random>

#include "ray.h"

// TODO: This is a quick and dirty hack. Clean it up!
std::random_device rd;
auto gen = std::mt19937{rd()};
auto realDist = std::uniform_real_distribution<>{-1.0, 1.0};

// Returns true if the vector is close to zero in all dimensions
inline bool isVec3NearZero(const Vectormath::Aos::Vector3 &vec3) {
	auto s = 1e-8;
	return (std::fabs(vec3.getX()) < s) && (std::fabs(vec3.getY()) < s) && (std::fabs(vec3.getZ()) < s);
}

inline Vectormath::Aos::Vector3 randomUnitVec3() {
	for (;;) {
		const auto& p = Vectormath::Aos::Vector3{realDist(gen), realDist(gen), realDist(gen)};
		if (Vectormath::Aos::lengthSqr(p) < 1.0f) {
				return Vectormath::Aos::normalize(p);
		}
	}
}

// TODO: This is a quick and dirty hack. Clean it up!
struct RandomUnitVec3Gen {
	static const u32 numSamples = 10000;
	std::array<Vectormath::Aos::Vector3, numSamples> vec3Samples;
	u64 currSampleIndex;

	RandomUnitVec3Gen() {
		currSampleIndex = 0;
		for (u32 sample = 0; sample < numSamples; ++sample) {
			vec3Samples[sample] = randomUnitVec3();
		}
	}

	const Vectormath::Aos::Vector3& getRandomUnitVec3() {
		return vec3Samples[(currSampleIndex++) % numSamples];
	}
};

RandomUnitVec3Gen __randomUnitVec3Gen;

inline Vectormath::Aos::Vector3 randomOnHemisphere(const Vectormath::Aos::Vector3& normal) {
	// const auto &randUnitVec = randomUnitVec3();
	const auto &randUnitVec = __randomUnitVec3Gen.getRandomUnitVec3();
	if (Vectormath::Aos::dot(randUnitVec, normal) > 0.0)
		return randUnitVec;
	else
		return -randUnitVec;
}

struct HitRecord {
	Vectormath::Aos::Vector3 hitPoint;
	Vectormath::Aos::Vector3 normal;
	Vectormath::Aos::Vector3 color;
	f32 t;
	bool frontFace;
	bool (*scatterCallback)(const Ray &r_in, const HitRecord &hitRecord, const Vectormath::Aos::Vector3 &albedo,
												 Vectormath::Aos::Vector3 &attenuation, Ray &scattered);

	// Memorize whether the normal is pointing agains or in the same direction of the normal.
	// We do this at geometry time because it is the least amount of work in this case.
	// NOTE: parameter outwardNormal is assumed to be a unit vector
	void setFaceNormal(const Ray &r, const Vectormath::Aos::Vector3 &outwardNormal) {
		frontFace = Vectormath::Aos::dot(r.direction(), outwardNormal) < 0;
		normal = frontFace ? outwardNormal : -outwardNormal;
	}
};

enum MaterialType { LAMBERTIAN, METAL };

bool lambertianScatter(const Ray &r_in, const HitRecord &hitRecord, const Vectormath::Aos::Vector3 &albedo,
												 Vectormath::Aos::Vector3 &attenuation, Ray &scattered) {
    auto scatterDirection = randomOnHemisphere(hitRecord.normal) + hitRecord.normal;

		if (isVec3NearZero(scatterDirection))
			scatterDirection = hitRecord.normal;

		scattered = Ray{hitRecord.hitPoint, scatterDirection};
		attenuation = albedo;
		return true;
}

bool metalScatter(const Ray &r_in, const HitRecord &hitRecord, const Vectormath::Aos::Vector3 &albedo,
												 Vectormath::Aos::Vector3 &attenuation, Ray &scattered) {
	auto reflectVec3 = [] (const auto &vec, const auto &normal) {
		return vec - 2 * Vectormath::Aos::dot(vec, normal) * normal;
	};

	const auto &reflectedHit = reflectVec3(r_in.direction(), hitRecord.normal);
	scattered = Ray{hitRecord.hitPoint, reflectedHit};
	attenuation = albedo;
	return true;
}

class Sphere {
public:
	Sphere(const Vectormath::Aos::Vector3 &center, f32 radius, const Vectormath::Aos::Vector3 &color, const MaterialType materialType) :
		mCenter{center}, mRadius{fmaxf(0.0f, radius)}, mColor{color}, mMaterialType{materialType} {}

	bool hit(const Ray &r, f32 rayTmin, f32 rayTmax, HitRecord &rec) const {
		const auto& oc = mCenter - r.origin();
		const auto &a = Vectormath::Aos::lengthSqr(r.direction());
		const auto &h = Vectormath::Aos::dot(r.direction(), oc);
		const auto &c = Vectormath::Aos::lengthSqr(oc) - (mRadius * mRadius);

		const auto discriminant = (h * h) - (a * c);
		if (discriminant < 0.0f)
			return false;

		const auto sqrtDiscriminant = sqrt(discriminant);

		// Find nearest root that lies in the acceptable range
		auto root = (h - sqrtDiscriminant) / a;
		if (root <= rayTmin || rayTmax <= root) {
			root = (h + sqrtDiscriminant) / a;
			if (root <= rayTmin || rayTmax <= root)
				return false;
		}

		rec.t = root;
		rec.hitPoint = r.at(rec.t);
		// Determine surface side
		const auto &outwardNormal = (rec.hitPoint - mCenter) / mRadius;
		rec.setFaceNormal(r, outwardNormal);
		rec.normal = (rec.hitPoint - mCenter) / mRadius;
		rec.color = mColor;
		switch (mMaterialType) {
			case LAMBERTIAN:
				rec.scatterCallback = lambertianScatter;
				break;
			case METAL:
				rec.scatterCallback = metalScatter;
				break;
		}

		return true;
	}

private:
	Vectormath::Aos::Vector3 mCenter;
	f32 mRadius;
	Vectormath::Aos::Vector3 mColor;
	MaterialType mMaterialType;
};

struct HittableList {
	std::vector<Sphere> objects;

	bool hit(const Ray &r, f32 rayTmin, f32 rayTmax, HitRecord &rec) const {
		HitRecord tmpRecord;
		bool hitAnything = false;
		auto closestSoFar = rayTmax;

		for (const auto &object : objects) {
			if (object.hit(r, rayTmin, closestSoFar, tmpRecord)) {
				hitAnything = true;
				closestSoFar = tmpRecord.t;
				rec = tmpRecord;
			}
		}

		return hitAnything;
	}
};

#endif // HITTABLE_H
