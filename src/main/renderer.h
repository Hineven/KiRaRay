#pragma once

#include "window.h"
#include "scene.h"
#include "camera.h"
#include "file.h"

#include "vulkan/uirender.h"
#include "device/buffer.h"
#include "device/context.h"
#include "scene/importer.h"
#include "render/profiler/ui.h"
#include "render/profiler/fps.h"

KRR_NAMESPACE_BEGIN

class RenderApp : public DeviceManager{
public:
	RenderApp();
	~RenderApp() = default;

	void BackBufferResizing() override {}
	void BackBufferResized() override;
	void Render() override;

	void initialize();
	void finalize();

	// Process signals passed down from direct imgui callback (imgui do not capture it)
	virtual bool onMouseEvent(io::MouseEvent &mouseEvent) override;
	virtual bool onKeyEvent(io::KeyboardEvent &keyEvent) override;

	void setScene(Scene::SharedPtr scene);
	
	void run();
	void renderUI();

	void captureFrame(bool hdr = false, fs::path filename = "");
	
	void saveConfig(string path);
	void loadConfigFrom(fs::path path);
	void loadConfig(const json config);

private:
	bool mShowUI{ true };
	bool mSaveFrames{ false };
	size_t mSaveFrameInterval{ 2 };
	bool mSaveHDR{ true };

	int mFrameCount{ 0 };
	int mSpp{ 0 };			// Samples needed tobe rendered, 0 means unlimited.
	FrameRate mFrameRate;
	Scene::SharedPtr mpScene;
	ProfilerUI::UniquePtr mpProfilerUI;
	json mConfig{};
	string mConfigPath{};
};

KRR_NAMESPACE_END