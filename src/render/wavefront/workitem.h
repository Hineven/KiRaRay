#pragma once
#include "common.h"
#include "raytracing.h"
#include "render/shared.h"
#include "render/spectrum.h"
#include "render/materials/bxdf.h"

KRR_NAMESPACE_BEGIN

using namespace shader;
using namespace rt;

/* Remember to copy these definitions to workitem.soa whenever changing them. */

struct PixelState {
	SampledSpectrum L;
	RGB pixel;
	PCGSampler sampler;
	SampledWavelengths lambda;
};

struct RayWorkItem {
	Ray ray;
	LightSampleContext ctx;
	SampledSpectrum thp;
	SampledSpectrum pu, pl;
	BSDFType bsdfType;
	uint depth;
	uint pixelId;
};

struct MissRayWorkItem {
	Ray ray;
	LightSampleContext ctx;
	SampledSpectrum thp;
	SampledSpectrum pu, pl;
	BSDFType bsdfType;
	uint depth;
	uint pixelId;
};

struct HitLightWorkItem {
	Light light;
	LightSampleContext ctx;
	Vector3f p;
	Vector3f wo;
	Vector3f n;
	Vector2f uv;
	SampledSpectrum thp;
	SampledSpectrum pu, pl;
	BSDFType bsdfType;
	uint depth;
	uint pixelId;
};

struct ShadowRayWorkItem {
	Ray ray;
	float tMax;
	SampledSpectrum Ld;
	SampledSpectrum pu, pl;
	uint pixelId;
};

struct ScatterRayWorkItem {
	SampledSpectrum thp;
	SampledSpectrum pu;
	SurfaceInteraction intr;
	uint depth;
	uint pixelId;
};

struct MediumSampleWorkItem {
	Ray ray;
	LightSampleContext ctx;
	SampledSpectrum thp;
	SampledSpectrum pu, pl;
	float tMax;
	SurfaceInteraction intr;	// has hit a surface as well...
	BSDFType bsdfType;
	uint depth;
	uint pixelId;
};

struct MediumScatterWorkItem {
	Vector3f p;
	SampledSpectrum thp;
	SampledSpectrum pu;
	Vector3f wo;
	float time;
	Medium medium;
	PhaseFunction phase;
	uint depth;
	uint pixelId;
};

#pragma warning (push, 0)
#pragma warning (disable: ALL_CODE_ANALYSIS_WARNINGS)
#include "render/wavefront/workitem_soa.h"
#pragma warning (pop)

KRR_NAMESPACE_END