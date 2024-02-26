#pragma once

#include "float.hpp"

#include "shared/hitRecord.hpp"
#include "ray.hpp"

class HittableObject {
	public:
		virtual bool hit(const ray& r, Float tMin, Float tMax, HitRecord& rec) const = 0;
};
