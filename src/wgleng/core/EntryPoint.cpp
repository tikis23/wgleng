#include "EntryPoint.h"

#include <stdio.h>

#include "../io/Input.h"
#include "../rendering/Debug.h"
#include "../rendering/Highlights.h"
#include "../rendering/Mesh.h"
#include "../rendering/Text.h"
#include "../util/Metrics.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_impl_opengl3.h"
#include "../vendor/imgui/imgui_impl_sdl2.h"

Context* ctx = nullptr;
bool isHidden = false;

void mainLoop() {
	// timing
	static auto lastTime = TimePoint();
	const auto now = TimePoint();
	const auto dt = now - lastTime;
	lastTime = now;

	// start imgui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();


	// logic
	Metrics::MeasureDurationStart(Metric::LOGIC_TOTAL);
	Input::Poll(false);
	onTick(ctx, dt);
	if (ctx->scene) ctx->scene->Update(dt);
	Metrics::MeasureDurationStop(Metric::LOGIC_TOTAL);

	// rendering
	Metrics::MeasureDurationStart(Metric::RENDER_TOTAL);
	ctx->renderer.SetViewportSize(ctx->rt.width, ctx->rt.height);
	ctx->renderer.Render(isHidden, ctx->scene);
	ctx->rt.SwapBuffers();
	Metrics::MeasureDurationStop(Metric::RENDER_TOTAL);

	// total frame time
	Metrics::MeasureDurationStop(Metric::FRAME_TOTAL);
	Metrics::MeasureDurationStart(Metric::FRAME_TOTAL);
}
void initialize() {
	// create context
	if (ctx != nullptr) {
		printf("Context already exists.\n");
		return;
	}
	Highlights::Init();
	ctx = new Context();
	if (!ctx->rt.IsValid()) {
		printf("Failed to create render target.\n");
		delete ctx;
		Highlights::Deinit();
		return;
	}
	Text::Init();
	DebugDraw::Init();

	onInit(ctx);
}
void deinitialize() {
	onDeinit(ctx);

	DebugDraw::Deinit();
	MeshRegistry::Clear();
	delete ctx;
	ctx = nullptr;
	Highlights::Deinit();
	Text::Deinit();
}

// embind stuff
bool I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_start() {
	initialize();
	emscripten_set_main_loop(mainLoop, 0, 1);
	return true;
}
void I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_stop() {
	deinitialize();
	emscripten_cancel_main_loop();
}
void I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_setFocused(bool focused) {
	if (ctx == nullptr) return;
	ctx->rt.SetFocused(focused);
}
void I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_setHidden(bool hidden) {
	if (ctx == nullptr) return;
	if (hidden == isHidden) return;

	isHidden = hidden;

	if (isHidden) {
		emscripten_cancel_main_loop();
		emscripten_set_main_loop(mainLoop, 5, 1);
	}
	else {
		emscripten_cancel_main_loop();
		emscripten_set_main_loop(mainLoop, 0, 1);
	}
}
