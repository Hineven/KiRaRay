#pragma once

#include <cuda_runtime.h>
#include <optix.h>
#include <optix_stubs.h>
#include <sstream>
#include <stdexcept>

#include "raytracing.h"
#include "scene.h"

KRR_NAMESPACE_BEGIN

/*! SBT record for a raygen program */
struct __align__(OPTIX_SBT_RECORD_ALIGNMENT) RaygenRecord {
	__align__(OPTIX_SBT_RECORD_ALIGNMENT) char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

/*! SBT record for a miss program */
struct __align__(OPTIX_SBT_RECORD_ALIGNMENT) MissRecord {
	__align__(OPTIX_SBT_RECORD_ALIGNMENT) char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

/*! SBT record for a hitgroup program */
struct __align__(OPTIX_SBT_RECORD_ALIGNMENT) HitgroupRecord {
	__align__(OPTIX_SBT_RECORD_ALIGNMENT) char header[OPTIX_SBT_RECORD_HEADER_SIZE];
	HitgroupSBTData data;
};

class OptixScene {
public:
	using SharedPtr = std::shared_ptr<OptixScene>;
	OptixScene(std::shared_ptr<Scene> _scene);

	static OptixTraversableHandle buildASFromInputs(OptixDeviceContext optixContext, 
		CUstream cudaStream, const std::vector<OptixBuildInput> &buildInputs, 
		CUDABuffer& accelBuffer, bool compact = false, bool update = false); 
	static OptixTraversableHandle buildTriangleMeshGAS(OptixDeviceContext optixContext,
													   CUstream cudaStream,
													   const rt::MeshData &mesh,
													   CUDABuffer &accelBuffer);

	std::shared_ptr<Scene> getScene() const;
	OptixTraversableHandle getRootTraversable() const { return traversableIAS; }
	rt::SceneData getSceneData() const;
	
	void update();

protected:
	void buildAccelStructure();
	void updateAccelStructure();

	std::weak_ptr<Scene> scene;
	gpu::vector<OptixInstance> instancesIAS;

	std::vector<CUDABuffer> accelBuffersGAS;
	CUDABuffer accelBufferIAS{};

	std::vector<OptixTraversableHandle> traversablesGAS;
	OptixTraversableHandle traversableIAS;
};

struct OptixInitializeParameters {
	char* ptx;
	std::vector<string> raygenEntries;
	std::vector<string> rayTypes;
	std::vector<std::tuple<bool, bool, bool>> rayClosestShaders;

	OptixInitializeParameters& setPTX(char* ptx) {
		this->ptx = ptx;
		return *this;
	}
	OptixInitializeParameters &addRaygenEntry(string entryName) {
		raygenEntries.push_back(entryName);
		return *this;
	}
	OptixInitializeParameters &addRayType(string typeName, bool closestHit,
										  bool anyHit, bool intersect) {
		rayTypes.push_back(typeName);
		rayClosestShaders.push_back({closestHit, anyHit, intersect});
		return *this;
	}
};

class OptixBackend {
public:
	using SharedPtr = std::shared_ptr<OptixBackend>;
	OptixBackend()	= default; 
	~OptixBackend() = default;

	void initialize(const OptixInitializeParameters& params);
	void setScene(std::shared_ptr<Scene> _scene);
	template <typename T>
	void launch(const T& parameters, string entryPoint, int width,
		int height, int depth = 1) {
		if (height * width * depth == 0) return;
		static T *launchParams{nullptr};
		if (!launchParams) cudaMalloc(&launchParams, sizeof(T));
		if (!entryPoints.count(entryPoint))
			Log(Fatal, "The entrypoint %s is not initialized!", entryPoint.c_str());
		cudaMemcpyAsync(launchParams, &parameters, sizeof(T), cudaMemcpyHostToDevice, cudaStream);
		OPTIX_CHECK(optixLaunch(optixPipeline, cudaStream, CUdeviceptr(launchParams), 
			sizeof(T), &SBT[entryPoints[entryPoint]], width, height, depth));
	}

	std::shared_ptr<Scene> getScene() const { return scene; }
	rt::SceneData getSceneData() const;
	std::vector<string> getRayTypes() const { return optixParameters.rayTypes; }
	std::vector<string> getRaygenEntries() const { return optixParameters.raygenEntries; }
	OptixTraversableHandle getRootTraversable() const;

protected:
	void buildShaderBindingTable();

	OptixProgramGroup createRaygenPG(const char *entrypoint) const;
	OptixProgramGroup createMissPG(const char *entrypoint) const;
	OptixProgramGroup createIntersectionPG(const char *closest, const char *any,
										   const char *intersect) const;

	OptixModule optixModule;
	OptixPipeline optixPipeline;
	OptixDeviceContext optixContext;
	CUstream cudaStream;

	std::vector<OptixProgramGroup> raygenPGs;
	std::vector<OptixProgramGroup> missPGs;
	std::vector<OptixProgramGroup> hitgroupPGs; 
	gpu::vector<RaygenRecord> raygenRecords;
	gpu::vector<HitgroupRecord> hitgroupRecords;
	gpu::vector<MissRecord> missRecords;

	std::map<string, int> entryPoints;
	std::vector<OptixShaderBindingTable> SBT;
	
	std::shared_ptr<Scene> scene;
	OptixInitializeParameters optixParameters;

public:
	static const size_t OPTIX_MAX_RAY_TYPES = 3;	// Radiance, Shadow, ShadowTransmission
	static OptixModule createOptixModule(OptixDeviceContext optixContext, const char* ptx);
	static OptixPipelineCompileOptions getPipelineCompileOptions();

	static OptixProgramGroup createRaygenPG(OptixDeviceContext optixContext, OptixModule optixModule, const char* entrypoint);
	static OptixProgramGroup createMissPG(OptixDeviceContext optixContext, OptixModule optixModule, const char* entrypoint);
	static OptixProgramGroup createIntersectionPG(OptixDeviceContext optixContext, OptixModule optixModule,
		const char* closest, const char* any, const char* intersect);
};

KRR_NAMESPACE_END