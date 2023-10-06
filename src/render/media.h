#pragma once
#include "common.h"
#include "taggedptr.h"
#include "render/phase.h"
#include "medium.h"
#include "raytracing.h"
#include "util/volume.h"

KRR_NAMESPACE_BEGIN

class HomogeneousMedium {
public:
	HomogeneousMedium() = default;

	HomogeneousMedium(Color sigma_a, Color sigma_s, Color L_e, float g) :
		sigma_a(sigma_a), sigma_s(sigma_s), L_e(L_e), phase(g) {}

	KRR_CALLABLE bool isEmissive() const { return !L_e.isZero(); }

	KRR_CALLABLE Color Le(Vector3f p) const { return L_e; }
	
	KRR_CALLABLE MediumProperties samplePoint(Vector3f p) const {
		return {sigma_a, sigma_s, &phase, L_e};
	}

	KRR_CALLABLE RayMajorant sampleRay(const Ray &ray, float tMax) const {
		return {sigma_a + sigma_s, 0, tMax};
	}

	Color sigma_a, sigma_s, L_e;
	HGPhaseFunction phase;
};

class NanoVDBMedium {
public:
	using VDBSampler = nanovdb::SampleFromVoxels<nanovdb::FloatGrid::TreeType, 1, false>;

	NanoVDBMedium(const Affine3f& transform, Color sigma_a, Color sigma_s, float g, NanoVDBGrid density) :
		transform(transform), phase(g), sigma_a(sigma_a), sigma_s(sigma_s), densityGrid(std::move(density)) {
		inverseTransform				   = transform.inverse();
	}

	KRR_CALLABLE bool isEmissive() const { return false; }

	KRR_CALLABLE Color Le(Vector3f p) const { return 0; }
	
	KRR_CALLABLE MediumProperties samplePoint(Vector3f p) const { 
		p = inverseTransform * p;
		float d = densityGrid.getDensity(p);
		return {sigma_a * d, sigma_s * d, &phase, Le(p)};
	}

	KRR_CALLABLE RayMajorant sampleRay(const Ray &ray, float raytMax) const {
		float tMin = 0, tMax = raytMax;
		AABB3f box	 = densityGrid.getBounds();
		Ray localRay = inverseTransform * ray;
		if(!box.intersect(localRay.origin, localRay.dir, raytMax, &tMin, &tMax)) return {};
		return {densityGrid.getMaxDensity() * (sigma_a + sigma_s), tMin, tMax};
	}

	NanoVDBGrid densityGrid;
	Affine3f transform, inverseTransform;
	HGPhaseFunction phase;
	Color sigma_a, sigma_s;
};

/* Put these definitions here since the optix kernel will need them... */
/* Definitions of inline functions should be put into header files. */
inline Color Medium::Le(Vector3f p) const {
	auto Le = [&](auto ptr) -> Color { return ptr->Le(p); };
	return dispatch(Le);
}

inline bool Medium::isEmissive() const {
	auto emissive = [&](auto ptr) -> bool { return ptr->isEmissive(); };
	return dispatch(emissive);
}

inline MediumProperties Medium::samplePoint(Vector3f p) const {
	auto sample = [&](auto ptr) -> MediumProperties { return ptr->samplePoint(p); };
	return dispatch(sample);
}

inline RayMajorant Medium::sampleRay(const Ray &ray, float tMax) const {
	auto sample = [&](auto ptr) -> RayMajorant { return ptr->sampleRay(ray, tMax); };
	return dispatch(sample);
}


KRR_NAMESPACE_END