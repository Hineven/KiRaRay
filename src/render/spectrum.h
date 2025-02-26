#pragma once

#include "common.h"
#include "krrmath/math.h"
#include "device/gpustd.h"
#include "device/taggedptr.h"
#include "util/check.h"
#include "render/color.h"

KRR_NAMESPACE_BEGIN

class RGBColorSpace;
class RGBBoundedSpectrum;
class RGBUnboundedSpectrum;
class DenselySampledSpectrum;
class PiecewiseLinearSpectrum;

class SampledWavelengths {
public:
	SampledWavelengths() = default;

	KRR_CALLABLE bool operator==(const SampledWavelengths& swl) const {
		return (lambda == swl.lambda).all() && (pdfs == swl.pdfs).all();
	}

	KRR_CALLABLE bool operator!=(const SampledWavelengths& swl) const {
		return !(lambda == swl.lambda).all() || !(pdfs == swl.pdfs).all();
	}

	KRR_CALLABLE static SampledWavelengths sampleUniform(float u, float lambdaMin = cLambdaMin,
		float lambdaMax = cLambdaMax) {
		SampledWavelengths swl;
		swl.lambda[0] = lerp(lambdaMin, lambdaMax, u);
		float delta = (lambdaMax - lambdaMin) / nSpectrumSamples;
		for (int i = 1; i < nSpectrumSamples; i++) {
			swl.lambda[i] = swl.lambda[i - 1] + delta;
			if (swl.lambda[i] > lambdaMax) 
				swl.lambda[i] -= lambdaMax - lambdaMin;
		}
		swl.pdfs.fill(1.f / (lambdaMax - lambdaMin));
		return swl;
	}

	KRR_CALLABLE int mainIndex(float lambdaMin = cLambdaMin,
									float lambdaMax = cLambdaMax) const {
#if KRR_RENDER_SPECTRAL
		return 0;
#else
		return clamp(int((lambda[0] - lambdaMin) / (lambdaMax - lambdaMin) * nSpectrumSamples),
					 0, nSpectrumSamples - 1);
#endif
	}


	KRR_CALLABLE float operator[](int i) const { return lambda[i]; }
	KRR_CALLABLE float &operator[](int i) { return lambda[i]; }

	KRR_CALLABLE Array<float, nSpectrumSamples> pdf() const { return pdfs; }

private:
	Array<float, nSpectrumSamples> lambda, pdfs;
};

class SampledSpectrum : public Array<float, nSpectrumSamples> {
public:
	using Array<float, nSpectrumSamples>::Array;
	//SampledSpectrum() = default;
	KRR_CALLABLE explicit SampledSpectrum(float c) : Array(c) {}

	KRR_CALLABLE explicit operator bool() const { return this->any(); }
	/* These functions should not be called within a OptiX kernel. */
	KRR_HOST_DEVICE float y(const SampledWavelengths &swl) const;
	KRR_HOST_DEVICE XYZ toXYZ(const SampledWavelengths &swl) const;
	KRR_HOST_DEVICE RGB toRGB(const SampledWavelengths &swl, const RGBColorSpace &cs) const;
};

class Spectrum :
	public TaggedPointer<RGBBoundedSpectrum, RGBUnboundedSpectrum, DenselySampledSpectrum,
						 PiecewiseLinearSpectrum> {
public:
	using TaggedPointer::TaggedPointer;
	KRR_HOST_DEVICE float operator()(float lambda) const;
	KRR_HOST_DEVICE float maxValue() const;
	KRR_HOST_DEVICE SampledSpectrum sample(const SampledWavelengths &lambda) const;
};

class RGBBoundedSpectrum {
public:
	KRR_CALLABLE float operator()(float lambda) const { return rsp(lambda); }

	KRR_CALLABLE float maxValue() const { return rsp.maxValue(); }

	KRR_CALLABLE RGBBoundedSpectrum(RGB rgb, const RGBColorSpace &cs);

	KRR_CALLABLE SampledSpectrum sample(const SampledWavelengths& lambda) const {
		SampledSpectrum result;
		for (int i = 0; i < nSpectrumSamples; i++)
			result[i] = rsp(lambda[i]);
		return result;
	}

	RGBSigmoidPolynomial rsp;
};

class RGBUnboundedSpectrum {
public:
	KRR_CALLABLE float operator()(float lambda) const { return scale * rsp(lambda); }

	KRR_CALLABLE float maxValue() const { return scale * rsp.maxValue(); }

	KRR_CALLABLE RGBUnboundedSpectrum(): rsp(0, 0, 0), scale(0) {}
	KRR_CALLABLE RGBUnboundedSpectrum::RGBUnboundedSpectrum(RGB rgb, const RGBColorSpace &cs);

	KRR_CALLABLE SampledSpectrum sample(const SampledWavelengths &lambda) const {
		SampledSpectrum result;
		for (int i = 0; i < nSpectrumSamples; i++) result[i] = scale * rsp(lambda[i]);
		return result;
	}
private:
	RGBSigmoidPolynomial rsp;
	float scale;
};

class PiecewiseLinearSpectrum {
public:
	// PiecewiseLinearSpectrum Public Methods
	PiecewiseLinearSpectrum() = default;

	KRR_CALLABLE void scale(float s) {
		for (float &v : values) v *= s;
	}

	KRR_CALLABLE float maxValue() const {
		if (values.empty()) return 0;
		return *std::max_element(values.begin(), values.end());
	}

	KRR_CALLABLE SampledSpectrum sample(const SampledWavelengths &lambda) const {
		SampledSpectrum s;
		for (int i = 0; i < nSpectrumSamples; ++i) s[i] = (*this)(lambda[i]);
		return s;
	}

	KRR_HOST_DEVICE float operator()(float lambda) const;

	PiecewiseLinearSpectrum(gpu::span<const float> lambdas, gpu::span<const float> values,
							Allocator alloc = {});

	static PiecewiseLinearSpectrum *fromInterleaved(gpu::span<const float> samples, bool normalize,
													Allocator alloc);
private:
	// PiecewiseLinearSpectrum Private Members
	gpu::vector<float> lambdas, values;
};

class DenselySampledSpectrum {
public:
	// DenselySampledSpectrum Public Methods
	DenselySampledSpectrum(int lambdaMin = cLambdaMin, int lambdaMax = cLambdaMax,
						   Allocator alloc = {}) :
		lambdaMin(lambdaMin),
		lambdaMax(lambdaMax),
		values(lambdaMax - lambdaMin + 1, alloc) {}
	DenselySampledSpectrum(Spectrum s, Allocator alloc) :
		DenselySampledSpectrum(s, cLambdaMin, cLambdaMax, alloc) {}
	DenselySampledSpectrum(const DenselySampledSpectrum &s, Allocator alloc) :
		lambdaMin(s.lambdaMin),
		lambdaMax(s.lambdaMax),
		values(s.values.begin(), s.values.end(), alloc) {}

	KRR_CALLABLE SampledSpectrum sample(const SampledWavelengths &lambda) const {
		SampledSpectrum s;
		for (int i = 0; i < nSpectrumSamples; ++i) {
			int offset = std::lround(lambda[i]) - lambdaMin;
			if (offset < 0 || offset >= values.size()) s[i] = 0;
			else s[i] = values[offset];
		}
		return s;
	}

	KRR_CALLABLE void scale(float s) { for (float &v : values) v *= s; }

	KRR_CALLABLE
	float maxValue() const { return *std::max_element(values.begin(), values.end()); }

	DenselySampledSpectrum(Spectrum spec, int lambdaMin = cLambdaMin, int lambdaMax = cLambdaMax,
						   Allocator alloc = {}) :
		lambdaMin(lambdaMin), lambdaMax(lambdaMax), values(lambdaMax - lambdaMin + 1, alloc) {
		CHECK_GE(lambdaMax, lambdaMin);
		if (spec)
			for (int lambda = lambdaMin; lambda <= lambdaMax; ++lambda)
				values[lambda - lambdaMin] = spec(lambda);
	}

	template <typename F>
	static DenselySampledSpectrum sampleFunction(F func, int lambdaMin = cLambdaMin, 
			int lambdaMax	 = cLambdaMax, Allocator alloc = {}) {
		DenselySampledSpectrum s(lambdaMin, lambdaMax, alloc);
		for (int lambda = lambdaMin; lambda <= lambdaMax; ++lambda)
			s.values[lambda - lambdaMin] = func(lambda);
		return s;
	}

	KRR_CALLABLE float operator()(float lambda) const {
		DCHECK_GT(lambda, 0);
		int offset = std::lround(lambda) - lambdaMin;
		if (offset < 0 || offset >= values.size()) return 0;
		return values[offset];
	}

	KRR_CALLABLE bool operator==(const DenselySampledSpectrum &d) const {
		if (lambdaMin != d.lambdaMin || lambdaMax != d.lambdaMax ||
			values.size() != d.values.size())
			return false;
		for (size_t i = 0; i < values.size(); ++i)
			if (values[i] != d.values[i]) return false;
		return true;
	}

private:
	int lambdaMin, lambdaMax;
	gpu::vector<float> values;
};

inline float Spectrum::operator()(float lambda) const {
	auto op = [&](auto ptr) -> float { return (*ptr)(lambda); };
	return dispatch(op);
}
inline float Spectrum::maxValue() const {
	auto op = [&](auto ptr) -> float { return ptr->maxValue(); };
	return dispatch(op);
}
inline SampledSpectrum Spectrum::sample(const SampledWavelengths &lambda) const {
	auto sample = [&](auto ptr) -> SampledSpectrum { return ptr->sample(lambda); };
	return dispatch(sample);
}

class RGBColorSpace {
public:
	// RGBColorSpace Public Methods
	RGBColorSpace(Point2f r, Point2f g, Point2f b, Spectrum illuminant, 
		const RGBToSpectrumTable *rgbToSpectrumTable, 
		const DenselySampledSpectrum *x, 
		const DenselySampledSpectrum *y, 
		const DenselySampledSpectrum *z,
		Allocator alloc);

	KRR_CALLABLE RGBSigmoidPolynomial toRGBCoeffs(RGB rgb) const;

	static void init(Allocator alloc);

	// RGBColorSpace Public Members
	Point2f r, g, b, w;
	DenselySampledSpectrum illuminant;
	Matrix3f XYZFromRGB, RGBFromXYZ;
	const DenselySampledSpectrum *CIE_X, *CIE_Y, *CIE_Z;
	static const RGBColorSpace *sRGB, *DCI_P3, *Rec2020, *ACES2065_1;

	KRR_CALLABLE bool operator==(const RGBColorSpace &cs) const {
		return (r == cs.r && g == cs.g && b == cs.b && w == cs.w &&
				rgbToSpectrumTable == cs.rgbToSpectrumTable);
	}
	KRR_CALLABLE bool operator!=(const RGBColorSpace &cs) const {
		return (r != cs.r || g != cs.g || b != cs.b || w != cs.w ||
				rgbToSpectrumTable != cs.rgbToSpectrumTable);
	}

	KRR_CALLABLE RGB luminanceVector() const {
		return RGB(XYZFromRGB(1, 0), XYZFromRGB(1, 1), XYZFromRGB(1, 2));
	}

	static const RGBColorSpace *getNamed(std::string name);
	static const RGBColorSpace *lookup(Point2f r, Point2f g, Point2f b, Point2f w);

	KRR_CALLABLE XYZ toXYZ(RGB rgb) const { return XYZFromRGB * rgb.matrix(); }
	KRR_CALLABLE RGB toRGB(XYZ xyz) const { return RGBFromXYZ * xyz.matrix(); }
	KRR_CALLABLE XYZ toXYZ(SampledSpectrum s, const SampledWavelengths &lambda) const;
	KRR_CALLABLE RGB toRGB(SampledSpectrum s, const SampledWavelengths &lambda) const;

private:
	// RGBColorSpace Private Members
	const RGBToSpectrumTable *rgbToSpectrumTable;
};

 inline RGBBoundedSpectrum::RGBBoundedSpectrum(RGB rgb, const RGBColorSpace &cs) {
	DCHECK_LE(rgb.maxCoeff(), 1);
	DCHECK_GE(rgb.minCoeff(), 0);
	rsp = cs.toRGBCoeffs(rgb);
 }

 inline RGBUnboundedSpectrum::RGBUnboundedSpectrum(RGB rgb, const RGBColorSpace &cs) {
	scale	= 2 * rgb.maxCoeff();
	rsp		= cs.toRGBCoeffs(scale ? rgb / scale : RGB(0, 0, 0));
 }

namespace spec {

extern KRR_CONST RGBColorSpace *RGBColorSpace_sRGB;
extern KRR_CONST RGBColorSpace *RGBColorSpace_DCI_P3;
extern KRR_CONST RGBColorSpace *RGBColorSpace_Rec2020;
extern KRR_CONST RGBColorSpace *RGBColorSpace_ACES2065_1;

void init(Allocator alloc);

KRR_CALLABLE const DenselySampledSpectrum &X() {
#ifdef KRR_DEVICE_CODE
	extern KRR_DEVICE DenselySampledSpectrum *xGPU;
	return *xGPU;
#else
	extern DenselySampledSpectrum *x;
	return *x;
#endif
}

KRR_CALLABLE const DenselySampledSpectrum &Y() {
#ifdef KRR_DEVICE_CODE
	extern KRR_DEVICE DenselySampledSpectrum *yGPU;
	return *yGPU;
#else
	extern DenselySampledSpectrum *y;
	return *y;
#endif
}

KRR_CALLABLE const DenselySampledSpectrum &Z() {
#ifdef KRR_DEVICE_CODE
	extern KRR_DEVICE DenselySampledSpectrum *zGPU;
	return *zGPU;
#else
	extern DenselySampledSpectrum *z;
	return *z;
#endif
}

float innerProduct(Spectrum f, Spectrum g);
XYZ spectrumToXYZ(Spectrum s);
Spectrum getNamedSpectrum(std::string name);

} // namespace spec

inline RGBSigmoidPolynomial RGBColorSpace::toRGBCoeffs(RGB rgb) const {
	DCHECK(rgb.r() >= 0 && rgb.g() >= 0 && rgb.b() >= 0);
	return (*rgbToSpectrumTable)(rgb.cwiseMax(0));
}

/* OptiX shader program cannot access the variables on device (i.e. those declared with a 
	__device__ qualifier). For a workaround, we put these variables (e.g. CIE_X spectral)
	within the RGBColorSpace class, and pass its pointer to launch parameters. */
inline XYZ RGBColorSpace::toXYZ(SampledSpectrum s, const SampledWavelengths& lambda) const {
	SampledSpectrum X	= CIE_X->sample(lambda);
	SampledSpectrum Y	= CIE_Y->sample(lambda);
	SampledSpectrum Z	= CIE_Z->sample(lambda);
	SampledSpectrum pdf = lambda.pdf();
	return XYZ(SampledSpectrum(X * s).safeDiv(pdf).mean(),
			   SampledSpectrum(Y * s).safeDiv(pdf).mean(),
			   SampledSpectrum(Z * s).safeDiv(pdf).mean()) /
		   CIE_Y_integral;
}

inline RGB RGBColorSpace::toRGB(SampledSpectrum s, const SampledWavelengths &lambda) const {
	XYZ xyz = toXYZ(s, lambda);
	return toRGB(xyz);
}

KRR_CALLABLE float luminance(Color3f color) {
	return dot(Vector3f(color), Vector3f(0.299, 0.587, 0.114));
}

#if KRR_RENDER_SPECTRAL
KRR_CALLABLE float luminance(SampledSpectrum color) { return color.mean(); }
#endif

KRR_NAMESPACE_END