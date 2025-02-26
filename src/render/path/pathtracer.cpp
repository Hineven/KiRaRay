#include <optix_types.h>

#include "device/cuda.h"
#include "pathtracer.h"
#include "render/profiler/profiler.h"

KRR_NAMESPACE_BEGIN

extern "C" char PATHTRACER_PTX[];

void MegakernelPathTracer::initialize() {
	if (!optixBackend) {
		optixBackend = std::make_shared<OptixBackend>();
		auto params	 = OptixInitializeParameters()
						  .setPTX(PATHTRACER_PTX)
						  .addRayType("Radiance", true, true, false)
						  .addRayType("ShadowRay", true, true, false)
						  .addRaygenEntry("Pathtracer");
		optixBackend->initialize(params);
	}
}

void MegakernelPathTracer::setScene(Scene::SharedPtr scene) {
	initialize();
	mScene = scene;
	optixBackend->setScene(scene);
}

void MegakernelPathTracer::renderUI() {
	ui::Text("Path tracing parameters");
	ui::InputInt("Samples per pixel", &launchParams.spp, 1, 1);
	ui::SliderFloat("RR absorption probability", &launchParams.probRR, 0.f, 1.f, "%.3f");
	ui::InputInt("Max bounces", &launchParams.maxDepth);
	ui::DragFloat("Radiance clip", &launchParams.clampThreshold, 0.1, 1, 500);
	ui::Checkbox("Next event estimation", &launchParams.NEE);
	if (launchParams.NEE) {
		ui::InputInt("Light sample count", &launchParams.lightSamples);
	}
	ui::Text("Debugging");
	ui::Checkbox("Shader debug output", &launchParams.debugOutput);
	ui::InputInt2("Debug pixel", (int *) &launchParams.debugPixel);
}

void MegakernelPathTracer::render(RenderContext *context) {
	if (getFrameSize()[0] * getFrameSize()[1] == 0)
		return;
	PROFILE("Megakernel Path Tracer");
	{
		launchParams.fbSize		 = getFrameSize();
		launchParams.colorSpace	 = KRR_DEFAULT_COLORSPACE;
		launchParams.colorBuffer = context->getColorTexture()->getCudaRenderTarget();
		launchParams.camera		 = mScene->getCamera()->getCameraData();
		launchParams.sceneData	 = mScene->getSceneRT()->getSceneData();
		launchParams.traversable = optixBackend->getRootTraversable();
		launchParams.frameID	 = (uint)getFrameIndex();

		optixBackend->launch(launchParams, "Pathtracer", getFrameSize()[0], getFrameSize()[1]);
	}
}

KRR_REGISTER_PASS_DEF(MegakernelPathTracer);
KRR_NAMESPACE_END