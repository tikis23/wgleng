#include "Renderer.h"

#include <emscripten/fetch.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <vector>

#include "../core/Components.h"
#include "../util/Metrics.h"
#include "../util/ModelMatrix.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_impl_opengl3.h"
#include "Debug.h"
#include "FrustumCulling.h"
#include "Highlights.h"
#include "Text.h"

Renderer::Renderer()
	: m_viewportWidth{640}, m_viewportHeight{480} {
	CheckExtensionSupport();
	SetSettings(m_settings, true);

	// GL settings
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glClearColor(0.5f, 0.4f, 0.3f, 1.0f);
}
Renderer::~Renderer() = default;
void Renderer::SetViewportSize(int32_t width, int32_t height) {
	if (m_viewportWidth == width && m_viewportHeight == height) return;
	m_viewportWidth = width;
	m_viewportHeight = height;
	if (m_settings.resolution.automatic) {
		auto newSettings = m_settings;
		newSettings.resolution.width = width;
		newSettings.resolution.height = height;
		SetSettings(newSettings, false);
	}
}
void Renderer::LoadShaderFromFile(const std::string& file) {
	if (file.empty()) {
		for (int i = m_shaderLoadingPrograms.size(); i > 0; i--) {
			auto vert = m_shaderLoadingOutput.front();
			m_shaderLoadingOutput.pop_front();
			auto frag = m_shaderLoadingOutput.front();
			m_shaderLoadingOutput.pop_front();
			const auto prog = m_shaderLoadingPrograms.front();
			m_shaderLoadingPrograms.pop_front();
			(*prog)->Load(vert.c_str(), frag.c_str());
		}
		SetupUniforms();
		return;
	}

	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	attr.requestMethod[0] = 'G';
	attr.requestMethod[1] = 'E';
	attr.requestMethod[2] = 'T';
	attr.requestMethod[3] = 0;
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attr.userData = this;

	attr.onsuccess = [](emscripten_fetch_t* fetch) {
		auto renderer = static_cast<Renderer*>(fetch->userData);
		if (fetch->status == 200) {
			// save output
			renderer->m_shaderLoadingOutput.push_back(std::string(fetch->data + 3, fetch->numBytes - 5));
		}
		else {
			printf("Failed to load shader: %s\n", fetch->url);
		}
		emscripten_fetch_close(fetch);
		if (renderer->m_shaderLoadingQueue.empty()) { // compile shaders
			renderer->LoadShaderFromFile("");
		}
		else { // load next
			std::string next = renderer->m_shaderLoadingQueue.front();
			renderer->m_shaderLoadingQueue.pop_front();
			renderer->LoadShaderFromFile(next);
		}
	};
	attr.onerror = [](emscripten_fetch_t* fetch) {
		printf("Failed to load shader: %s\n", fetch->url);
		emscripten_fetch_close(fetch);
	};
	emscripten_fetch(&attr, ("http://localhost:8000/rendering/" + std::string(file)).c_str());
}
void Renderer::ReloadShaders(ShaderType shaders) {
	if (shaders == ShaderType::NONE) return;
	if (m_shadersLoading) return;
	m_shadersLoading = true;
	printf("Loading shaders ...\n");

	// mesh
	if (!!(shaders & ShaderType::MESH)) {
		m_meshProgram = std::make_unique<ShaderProgram>("mesh");
		m_meshProgram->SetConstant("MODELS_PER_UBO", std::to_string(m_matricesPerUniformBuffer));

        #ifdef SHADER_HOT_RELOAD
		m_shaderLoadingPrograms.push_back(&m_meshProgram);
		m_shaderLoadingQueue.push_back("shaders/mesh.vs");
		m_shaderLoadingQueue.push_back("shaders/mesh.fs");
        #else
		const GLchar vertexSource[] = {
                #include "shaders/mesh.vs"
		};
		const GLchar fragmentSource[] = {
                #include "shaders/mesh.fs"
		};
		m_meshProgram->Load(vertexSource, fragmentSource);
        #endif
	}
	// lighting
	if (!!(shaders & ShaderType::LIGHTING)) {
		m_lightingProgram = std::make_unique<ShaderProgram>("lighting");
		m_lightingProgram->SetConstant("MATERIALS_PER_UBO", std::to_string(m_materialsPerUniformBuffer));
		const auto& highlights = Highlights::GetHighlights();
		m_lightingProgram->SetConstant("HIGHLIGHT_COUNT", std::to_string(highlights.size()));
		m_lightingProgram->SetConstant("HIGHLIGHT_COLORS", [&] {
			std::string str = "";
			for (int i = 0; i < highlights.size(); i++) {
				str += std::format("vec3({},{},{})", highlights[i].color.r, highlights[i].color.g, highlights[i].color.b)
						+ (i == highlights.size() - 1 ? "" : ",");
			}
			return str;
		}());
		m_lightingProgram->SetConstant("OUTLINES",
			m_settings.outlines == RendererSettings::OutlinePreset::OFF ? "0" : "1"
		);
		m_lightingProgram->SetConstant("SHADOWS",
			m_settings.shadows == RendererSettings::ShadowPreset::OFF ? "0" : "1"
		);
		std::string pcf = "1";
		if (m_settings.shadows == RendererSettings::ShadowPreset::OFF) pcf = "0";
		if (m_settings.shadows == RendererSettings::ShadowPreset::LOW) pcf = "0";
		m_lightingProgram->SetConstant("SHADOW_PCF", pcf);
		m_lightingProgram->SetConstant("MAX_FRUSTUMS", std::to_string(m_maxCSMFrustums));
		m_lightingProgram->SetConstant("CASCADE_COUNT", std::to_string(m_csmbuffer.GetFrustumCount()));
		m_lightingProgram->SetConstant("CASCADE_SPLITS", [&] {
			std::string str = "";
			const auto& cascades = m_csmbuffer.GetCascades();
			for (int i = 0; i < cascades.size(); i++) {
				str += std::to_string(cascades[i]) + (i == cascades.size() - 1 ? "" : ",");
			}
			return str;
		}());

        #ifdef SHADER_HOT_RELOAD
		m_shaderLoadingPrograms.push_back(&m_lightingProgram);
		m_shaderLoadingQueue.push_back("shaders/light.vs");
		m_shaderLoadingQueue.push_back("shaders/light.fs");
        #else
		const GLchar vertexSource[] = {
                #include "shaders/light.vs"
		};
		const GLchar fragmentSource[] = {
                #include "shaders/light.fs"
		};
		m_lightingProgram->Load(vertexSource, fragmentSource);
        #endif
	}
	// csm
	if (!!(shaders & ShaderType::CSM)) {
		m_csmPrograms.resize(m_csmbuffer.GetFrustumCount());
		for (int i = 0; i < m_csmPrograms.size(); i++) {
			auto& csmProgram = m_csmPrograms[i];
			csmProgram = std::make_unique<ShaderProgram>("csm");
			csmProgram->SetConstant("FRUSTUM_INDEX", std::to_string(i));
			csmProgram->SetConstant("MODELS_PER_UBO", std::to_string(m_matricesPerUniformBuffer));
			csmProgram->SetConstant("MAX_FRUSTUMS", std::to_string(m_maxCSMFrustums));

            #ifdef SHADER_HOT_RELOAD
			m_shaderLoadingPrograms.push_back(&csmProgram);
			m_shaderLoadingQueue.push_back("shaders/csm.vs");
			m_shaderLoadingQueue.push_back("shaders/csm.fs");
            #else
			const GLchar vertexSource[] = {
                    #include "shaders/csm.vs"
			};
			const GLchar fragmentSource[] = {
                    #include "shaders/csm.fs"
			};
			csmProgram->Load(vertexSource, fragmentSource);
            #endif
		}
	}
	// text
	if (!!(shaders & ShaderType::TEXT)) {
		m_textProgram = std::make_unique<ShaderProgram>("text");

		#ifdef SHADER_HOT_RELOAD
		m_shaderLoadingPrograms.push_back(&m_textProgram);
		m_shaderLoadingQueue.push_back("shaders/text.vs");
		m_shaderLoadingQueue.push_back("shaders/text.fs");
		#else
		const GLchar vertexSource[] = {
					#include "shaders/text.vs"
		};
		const GLchar fragmentSource[] = {
					#include "shaders/text.fs"
		};
		m_textProgram->Load(vertexSource, fragmentSource);
		#endif
	}
	// text
	if (!!(shaders & ShaderType::FXAA)) {
		m_fxaaProgram = std::make_unique<ShaderProgram>("fxaa");
		std::string val = "0";
		if (m_settings.fxaa == RendererSettings::FXAAPreset::LOW) val = "1";
		else if (m_settings.fxaa == RendererSettings::FXAAPreset::HIGH) val = "2";
		m_fxaaProgram->SetConstant("FXAA_PRESET", val);

		#ifdef SHADER_HOT_RELOAD
		m_shaderLoadingPrograms.push_back(&m_fxaaProgram);
		m_shaderLoadingQueue.push_back("shaders/fxaa.vs");
		m_shaderLoadingQueue.push_back("shaders/fxaa.fs");
		#else
		const GLchar vertexSource[] = {
					#include "shaders/fxaa.vs"
		};
		const GLchar fragmentSource[] = {
					#include "shaders/fxaa.fs"
		};
		m_fxaaProgram->Load(vertexSource, fragmentSource);
		#endif
	}
	// debug
	if (!!(shaders & ShaderType::DEBUG)) {
		m_debugProgram = std::make_unique<ShaderProgram>("debug");

        #ifdef SHADER_HOT_RELOAD
		m_shaderLoadingPrograms.push_back(&m_debugProgram);
		m_shaderLoadingQueue.push_back("shaders/debug.vs");
		m_shaderLoadingQueue.push_back("shaders/debug.fs");
        #else
		const GLchar vertexSource[] = {
                #include "shaders/debug.vs"
		};
		const GLchar fragmentSource[] = {
                #include "shaders/debug.fs"
		};
		m_debugProgram->Load(vertexSource, fragmentSource);
        #endif
	}

    #ifdef SHADER_HOT_RELOAD
	std::string shaderFileToLoad = m_shaderLoadingQueue.front();
	m_shaderLoadingQueue.pop_front();
	LoadShaderFromFile(shaderFileToLoad);
    #else
	SetupUniforms();
    #endif
}
void Renderer::SetupUniforms() {
	// setup uniforms
	m_nextUniformBindingIndex = 0;

	m_cameraUniform.SetBindingIndex(GetNextUniformBindingIndex());
	m_cameraUniform.Bind();

	m_modelUniform.SetBindingIndex(GetNextUniformBindingIndex());

	m_lightingInfoUniform.SetBindingIndex(GetNextUniformBindingIndex());
	m_lightingInfoUniform.Bind();

	m_materialUniform.SetBindingIndex(GetNextUniformBindingIndex());
	m_materialUniform.Bind();

	m_csmUniform.SetBindingIndex(GetNextUniformBindingIndex());
	m_csmUniform.Bind();

	m_textUniform.SetBindingIndex(GetNextUniformBindingIndex());
	m_textUniform.Bind();

	m_meshProgram->AddUniformBufferBinding("CameraUniform", m_cameraUniform.GetBindingIndex());
	m_meshProgram->AddUniformBufferBinding("ModelMatricesUniform", m_modelUniform.GetBindingIndex());

	m_lightingProgram->AddUniformBufferBinding("LightingInfoUniform", m_lightingInfoUniform.GetBindingIndex());
	m_lightingProgram->AddUniformBufferBinding("CSMUniform", m_csmUniform.GetBindingIndex());
	m_lightingProgram->AddUniformBufferBinding("MaterialUniform", m_materialUniform.GetBindingIndex());

	m_textProgram->AddUniformBufferBinding("TextUniform", m_textUniform.GetBindingIndex());

	for (auto& csmProgram : m_csmPrograms) {
		csmProgram->AddUniformBufferBinding("CSMUniform", m_csmUniform.GetBindingIndex());
		csmProgram->AddUniformBufferBinding("ModelMatricesUniform", m_modelUniform.GetBindingIndex());
	}

	m_debugProgram->AddUniformBufferBinding("CameraUniform", m_cameraUniform.GetBindingIndex());

	m_shadersLoading = false;
	printf("Shaders loaded.\n");
}
void Renderer::CheckExtensionSupport() {
	//auto exts = emscripten_webgl_get_supported_extensions();
	//printf("%s\n", exts);
}
void Renderer::SetFramebuffer(uint32_t framebuffer) {
	if (m_currentFramebuffer == framebuffer) return;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	m_currentFramebuffer = framebuffer;
}
void Renderer::SetRenderSize(uint32_t x, uint32_t y) {
	if (m_currentRenderSizeX == x && m_currentRenderSizeY == y) return;
	glViewport(0, 0, static_cast<int32_t>(x), static_cast<int32_t>(y));
	m_currentRenderSizeX = x;
	m_currentRenderSizeY = y;
}
void Renderer::SetFaceCullingFront() {
	if (m_faceCullingFront) return;
	glCullFace(GL_FRONT);
	m_faceCullingFront = true;
}
void Renderer::SetFaceCullingBack() {
	if (!m_faceCullingFront) return;
	glCullFace(GL_BACK);
	m_faceCullingFront = false;
}
void Renderer::SetSettings(const RendererSettings& settings, bool force) {
	ShaderType shaders = ShaderType::NONE;
	if (force || m_settings.resolution.width != settings.resolution.width ||
		m_settings.resolution.height != settings.resolution.height) {
		m_settings.resolution = settings.resolution;
		m_gbuffer.Resize(m_settings.resolution.width, m_settings.resolution.height);
		m_fxaabuffer.Resize(m_settings.resolution.width, m_settings.resolution.height);
	}
	if (force || m_settings.fxaa != settings.fxaa) {
		m_settings.fxaa = settings.fxaa;
		shaders |= ShaderType::FXAA;
	}
	if (force || m_settings.shadows != settings.shadows) {
		m_settings.shadows = settings.shadows;
		shaders |= ShaderType::LIGHTING | ShaderType::CSM;
		if (m_settings.shadows == RendererSettings::ShadowPreset::LOW) {
			m_csmbuffer.ResizeAndSetCascades(1024, 1024, {200, 600});
		}
		else if (m_settings.shadows == RendererSettings::ShadowPreset::MEDIUM) {
			m_csmbuffer.ResizeAndSetCascades(2048, 2048, {150, 500, 1000});
		}
		else if (m_settings.shadows == RendererSettings::ShadowPreset::HIGH) {
			m_csmbuffer.ResizeAndSetCascades(4096, 4096, {100, 300, 800, 1500});
		}
		else {
			m_csmbuffer.ResizeAndSetCascades(1, 1, {100});
		}
	}
	if (force || m_settings.outlines != settings.outlines) {
		m_settings.outlines = settings.outlines;
		shaders |= ShaderType::LIGHTING;
	}

	if (force) shaders = ShaderType::ALL;
	ReloadShaders(shaders);
}
void Renderer::Render(bool isHidden, const std::shared_ptr<Scene>& scene) {
	// check for shader reloads
	if (Highlights::HasChanged(true)) {
		ReloadShaders(ShaderType::LIGHTING);
	}

	if (isHidden || m_shadersLoading || !scene || !scene->GetCamera()) {
		DebugDraw::Clear();
		ImGui::Render();
		return;
	}
	auto& camera = scene->GetCamera();

	const auto csmMatrices = m_csmbuffer.GetLightSpaceMatrices(camera, scene->sunlightDir);

	Metrics::MeasureDurationStart(Metric::UPDATE_MESHES);
	UpdateRenderableMeshes(scene, csmMatrices);
	Metrics::MeasureDurationStop(Metric::UPDATE_MESHES, true);

	Metrics::MeasureDurationStart(Metric::UPDATE_UNIFORMS);
	UpdateUniforms(scene, csmMatrices);
	Metrics::MeasureDurationStop(Metric::UPDATE_UNIFORMS, true);

	// setup for shadows
	if (m_settings.shadows != RendererSettings::ShadowPreset::OFF) {
		SetRenderSize(m_csmbuffer.GetWidth(), m_csmbuffer.GetHeight());
		SetFaceCullingFront();

		Metrics::MeasureDurationStart(Metric::RENDER_SHADOWS);
		RenderShadowMaps();
		Metrics::MeasureDurationStop(Metric::RENDER_SHADOWS, true);
	}

	// setup for meshes
	SetRenderSize(m_settings.resolution.width, m_settings.resolution.height);
	SetFaceCullingBack();

	// setup gbuffer
	SetFramebuffer(m_gbuffer.GetFBO());
	glClear(GL_DEPTH_BUFFER_BIT);
	glm::uvec4 uclearColor{0};
	glm::vec4 fclearColor{0};
	glClearBufferuiv(GL_COLOR, 0, glm::value_ptr(uclearColor));
	glClearBufferfv(GL_COLOR, 1, glm::value_ptr(fclearColor));

	Metrics::MeasureDurationStart(Metric::RENDER_MESHES);
	RenderMeshes();
	Metrics::MeasureDurationStop(Metric::RENDER_MESHES, true);

	Metrics::SetStaticMetric(Metric::TRIANGLES_TOTAL, m_totalDrawnTriangleCount);
	Metrics::SetStaticMetric(Metric::DRAWN_ENTITES, m_totalDrawnEntityCount);
	m_totalDrawnTriangleCount = 0;
	m_totalDrawnEntityCount = 0;

	RenderDebug(scene);

	Metrics::MeasureDurationStart(Metric::RENDER_TEXT);
	RenderText(scene);
	Metrics::MeasureDurationStop(Metric::RENDER_TEXT, true);

	// lighting
	if (m_settings.fxaa != RendererSettings::FXAAPreset::OFF) SetFramebuffer(m_fxaabuffer.GetFBO());
	else {
		SetFramebuffer(0);
		SetRenderSize(m_viewportWidth, m_viewportHeight);
	}

	Metrics::MeasureDurationStart(Metric::RENDER_LIGHTING);
	RenderLighting();
	Metrics::MeasureDurationStop(Metric::RENDER_LIGHTING, true);

	// FXAA
	if (m_settings.fxaa != RendererSettings::FXAAPreset::OFF) {
		SetFramebuffer(0);
		SetRenderSize(m_viewportWidth, m_viewportHeight);
		Metrics::MeasureDurationStart(Metric::RENDER_FXAA);
		RenderFXAA();
		Metrics::MeasureDurationStop(Metric::RENDER_FXAA, true);
	}
	// imgui
	Metrics::Show();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Renderer::UpdateRenderableMeshes(const std::shared_ptr<Scene>& scene, const std::vector<glm::mat4>& csmMatrices) {
	auto& reg = scene->registry;

	// get matrices
	std::unordered_map<Mesh, std::vector<glm::mat4>> meshMatrices;

	reg.group<RigidBodyComponent, MeshComponent>().each(
		[&](auto& rbComp, auto& meshComp) {
			const auto body = rbComp.body;
			if (!body || meshComp.hidden || meshComp.hiddenPersistent) {
				meshComp.hidden = false;
				meshComp.highlightId = 0;
				return;
			}

			btTransform transform;
			if (body->getMotionState()) body->getMotionState()->getWorldTransform(transform);
			else transform = body->getWorldTransform();

			transform.setOrigin(transform.getOrigin() + btVector3(meshComp.position.x, meshComp.position.y, meshComp.position.z));
			glm::vec3 euler{};
			transform.getRotation().getEulerZYX(euler.z, euler.y, euler.x);
			glm::vec3 objPos = glm::vec3(transform.getOrigin().x(), transform.getOrigin().y(), transform.getOrigin().z());

			glm::mat4 model = computeModelMatrix(
				objPos - meshComp.position, meshComp.position,
				euler, glm::radians(meshComp.rotation),
				meshComp.scale);

			// !!!! EXTRA DATA PACKED INTO MATRIX, REQUIRES RESETING IN SHADER !!!!
			model[3][3] = meshComp.highlightId;
			meshComp.highlightId = 0;

			meshMatrices[meshComp.mesh].push_back(model);
		});
	reg.group<TransformComponent>(entt::get<MeshComponent>, entt::exclude<RigidBodyComponent>).each(
		[&](auto& transformComp, auto& meshComp) {
			if (meshComp.hidden || meshComp.hiddenPersistent) {
				meshComp.hidden = false;
				return;
			}

			glm::mat4 model = computeModelMatrix(
				transformComp.position, meshComp.position,
				glm::radians(transformComp.rotation), glm::radians(meshComp.rotation),
				transformComp.scale * meshComp.scale);

			// !!!! EXTRA DATA PACKED INTO MATRIX, REQUIRES RESETING IN SHADER !!!!
			model[3][3] = meshComp.highlightId;
			meshComp.highlightId = 0;

			meshMatrices[meshComp.mesh].push_back(model);
		});

	// setup batching
	std::vector<glm::mat4>& matricesToUpload = m_renderableMeshesState.matricesToUpload;
	matricesToUpload.clear();
	std::vector<std::vector<MeshBatch>*> batchesList;
	batchesList.reserve(csmMatrices.size() + 1);
	m_renderableMeshesState.worldBatch.clear();
	batchesList.emplace_back(&m_renderableMeshesState.worldBatch);
	m_renderableMeshesState.csmBatches.resize(csmMatrices.size());
	for (auto& batch : m_renderableMeshesState.csmBatches) {
		batch.clear();
		batchesList.emplace_back(&batch);
	}

	// setup frustum culling
	std::vector<FrustumCulling> frustums;
	frustums.reserve(csmMatrices.size() + 1);
	glm::mat4 projxview = scene->GetCamera()->GetProjectionMatrix() * scene->GetCamera()->GetViewMatrix();
	frustums.emplace_back(projxview);
	for (auto i = 0; i < csmMatrices.size(); i++) {
		frustums.emplace_back(csmMatrices[i]);
	}

	// frustum cull and batch
	uint32_t currentUboOffset = 0;
	for (auto i = 0; i < frustums.size(); i++) {
		auto& frustum = frustums[i];
		auto& batches = *batchesList[i];
		for (auto& [mesh, mats] : meshMatrices) {
			MeshBatch batch;
			batch.mesh = mesh;
			batch.instanceOffset = currentUboOffset;
			batch.instanceCount = 0;
			const auto aabb = mesh->GetAabb();
			for (auto& mat : mats) {
				// frustum cull
				auto fixedMat = mat;
				fixedMat[3][3] = 0; // remove extra data
				if (!frustum.IsAabbVisible(aabb.first, aabb.second, fixedMat)) continue;
				matricesToUpload.push_back(mat);

				// batch
				batch.instanceCount++;
				currentUboOffset++;
				if (batch.instanceCount == m_matricesPerUniformBuffer) {
					// fix ubo offset alignment
					const auto alignment = m_modelUniform.GetOffsetAlignment();
					const auto padding = alignment - (currentUboOffset * sizeof(glm::mat4) % alignment);
					currentUboOffset += padding / sizeof(glm::mat4);
					matricesToUpload.resize(currentUboOffset);

					// save batch
					batches.push_back(batch);
					batch.instanceOffset = currentUboOffset;
					batch.instanceCount = 0;
				}
			}
			if (batch.instanceCount == 0) continue;
			// fix ubo offset alignment
			const auto alignment = m_modelUniform.GetOffsetAlignment();
			const auto padding = alignment - (currentUboOffset * sizeof(glm::mat4) % alignment);
			currentUboOffset += padding / sizeof(glm::mat4);
			matricesToUpload.resize(currentUboOffset);

			// save batch
			batches.push_back(batch);
		}
	}
}
void Renderer::UpdateUniforms(const std::shared_ptr<Scene>& scene, const std::vector<glm::mat4>& csmMatrices) {
	auto& camera = scene->GetCamera();
	camera->Update(m_settings.resolution.width, m_settings.resolution.height);
	const glm::mat4 camProjView = camera->GetProjectionMatrix() * camera->GetViewMatrix();

	m_cameraUniform.Update({
		.projxview = camProjView,
		.nearFarPlane = {camera->GetNearPlane(), camera->GetFarPlane()}
	});

	m_lightingInfoUniform.Update({
		.sunlightDir = glm::normalize(scene->sunlightDir),
		.sunlightColor = {1.0, 0.7, 0.8, 3.0},
		.cameraPos = camera->position,
		.viewportSize_nearFarPlane = {
			m_settings.resolution.width, m_settings.resolution.height,
			camera->GetNearPlane(), camera->GetFarPlane()
		},
		.invProjView = glm::inverse(camProjView)
	});

	const std::size_t materialCount = MeshRegistry::GetMaterials().size();
	if (m_materialCount != materialCount) { // could be made more efficient but meh
		MaterialUniform materialData;
		std::ranges::copy(MeshRegistry::GetMaterials(), materialData.materials);
		m_materialUniform.Update(materialData);
		m_materialCount = materialCount;
	}

	if (m_settings.shadows != RendererSettings::ShadowPreset::OFF) {
		CSMUniform csmData;
		std::ranges::copy(csmMatrices, csmData.lightSpaceMatrices);
		m_csmUniform.Update(csmData);
	}

	// resize ubo if needed
	const auto& mats = m_renderableMeshesState.matricesToUpload;
	const uint32_t requiredSize = (mats.size() + m_matricesPerUniformBuffer) * sizeof(glm::mat4);
	if (m_modelUniform.GetSize() < requiredSize) {
		m_modelUniform.Resize(requiredSize + sizeof(glm::mat4) * m_matricesPerUniformBuffer);
	}
	// upload matrices
	m_modelUniform.Update(0, mats.size() * sizeof(glm::mat4), mats.data());
}
void Renderer::RenderShadowMaps() {
	uint64_t vertexCount = 0;
	uint64_t entityCount = 0;
	for (int i = 0; i < m_csmbuffer.GetFBOs().size(); i++) {
		const auto& csmProgram = m_csmPrograms[i];
		csmProgram->Use();
		const GLuint fbo = m_csmbuffer.GetFBOs()[i];
		SetFramebuffer(fbo);
		glClear(GL_DEPTH_BUFFER_BIT);
		uint32_t currentVao = 0;
		for (const auto& batch : m_renderableMeshesState.csmBatches[i]) {
			const auto vao = batch.mesh->GetVAO();
			if (currentVao != vao) {
				glBindVertexArray(vao);
				currentVao = vao;
			}
			m_modelUniform.Bind(batch.instanceOffset * sizeof(glm::mat4), m_matricesPerUniformBuffer * sizeof(glm::mat4));
			glDrawElementsInstanced(GL_TRIANGLES, batch.mesh->GetDrawCount(), GL_UNSIGNED_INT, nullptr, batch.instanceCount);
			vertexCount += batch.instanceCount * batch.mesh->GetDrawCount();
			entityCount += batch.instanceCount;
		}
	}
	uint64_t triCount = vertexCount / 3;
	m_totalDrawnTriangleCount += triCount;
	Metrics::SetStaticMetric(Metric::TRIANGLES_SHADOW, triCount);
	m_totalDrawnEntityCount += entityCount;
	Metrics::SetStaticMetric(Metric::SHADOW_ENTITES, entityCount);
}
void Renderer::RenderMeshes() {
	uint64_t vertexCount = 0;
	uint64_t entityCount = 0;
	const int drawType = m_showWireframe ? GL_LINES : GL_TRIANGLES;
	m_meshProgram->Use();
	uint32_t currentVao = 0;
	for (const auto& batch : m_renderableMeshesState.worldBatch) {
		const auto vao = batch.mesh->GetVAO();
		if (currentVao != vao) {
			glBindVertexArray(vao);
			currentVao = vao;
		}
		m_modelUniform.Bind(batch.instanceOffset * sizeof(glm::mat4), m_matricesPerUniformBuffer * sizeof(glm::mat4));
		glDrawElementsInstanced(drawType, batch.mesh->GetDrawCount(), GL_UNSIGNED_INT, nullptr, batch.instanceCount);
		vertexCount += batch.instanceCount * batch.mesh->GetDrawCount();
		entityCount += batch.instanceCount;
	}
	uint64_t triCount = vertexCount / 3;
	m_totalDrawnTriangleCount += triCount;
	Metrics::SetStaticMetric(Metric::TRIANGLES_MESHES, triCount);
	m_totalDrawnEntityCount += entityCount;
	Metrics::SetStaticMetric(Metric::MESH_ENTITES, entityCount);
}
void Renderer::RenderDebug(const std::shared_ptr<Scene>& scene) const {
	if (DebugDraw::IsEnabled()) {
		m_debugProgram->Use();
		DebugDraw::GetDrawer()->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawContactPoints);
		scene->GetPhysicsWorld().dynamicsWorld->setDebugDrawer(DebugDraw::GetDrawer());
		scene->GetPhysicsWorld().dynamicsWorld->debugDrawWorld();
		DebugDraw::Draw();
	}
}
void Renderer::RenderText(const std::shared_ptr<Scene>& scene) {
	auto& camera = scene->GetCamera();
	const glm::mat4 camProjView = camera->GetProjectionMatrix() * camera->GetViewMatrix();

	// group scene texts
	std::unordered_map<std::string, std::vector<std::shared_ptr<DrawableText>>> textMap;
	auto& texts = scene->GetTexts();
	for (int i = 0; i < texts.size(); i++) {
		// if text is dead, remove it
		auto text = texts[i].lock();
		if (!text) {
			std::swap(texts[i], texts.back());
			texts.pop_back();
			i--;
			continue;
		}
		textMap[text->GetFontName()].push_back(text);
	}
	// group texts on rigid bodies
	for (auto&& [entity, textComp, rbComp] : scene->registry.view<TextComponent, RigidBodyComponent>().each()) {
		const auto body = rbComp.body;
		if (!body) continue;
		for (const auto& text : textComp.texts) {
			// get model matrix
			btTransform transform;
			if (body->getMotionState()) body->getMotionState()->getWorldTransform(transform);
			else transform = body->getWorldTransform();
			transform.setOrigin(transform.getOrigin() +
				btVector3(text->position.x, text->position.y, text->position.z));
			glm::vec3 euler{};
			transform.getRotation().getEulerZYX(euler.z, euler.y, euler.x);
			glm::vec3 objPos = glm::vec3(transform.getOrigin().x(), transform.getOrigin().y(), transform.getOrigin().z());

			text->useTransform = true;
			text->transform = computeModelMatrix(
				objPos - text->position, text->position,
				euler, glm::radians(text->rotation),
				text->scale);
			textMap[text->GetFontName()].push_back(text);
		}
	}
	// group texts on transforms
	for (auto&& [entity, textComp, tComp] : scene->registry.view<TextComponent, TransformComponent>().each()) {
		for (const auto& text : textComp.texts) {
			// get model matrix
			text->useTransform = true;
			text->transform = computeModelMatrix(
				tComp.position, text->position,
				glm::radians(tComp.rotation), glm::radians(text->rotation),
				tComp.scale * text->scale);
			textMap[text->GetFontName()].push_back(text);
		}
	}

	// draw 3d texts
	glDisable(GL_CULL_FACE);
	m_textProgram->Use();
	for (auto& [fontName, textArray] : textMap) {
		// set font texture
		const auto fontTexture = textArray[0]->GetFontTexture();
		m_textProgram->SetTexture("tGlyphs", GL_TEXTURE_2D_ARRAY, 0, fontTexture);

		// draw each text
		for (int i = 0; i < textArray.size(); i++) {
			const auto& text = textArray[i];
			if (text->useOrtho) continue;
			glm::mat4 model = glm::mat4(1);
			if (text->useTransform) {
				model = text->transform;
			}
			else {
				model = glm::translate(model, text->position);
				model = glm::rotate(model, glm::radians(text->rotation.x), glm::vec3(1, 0, 0));
				model = glm::rotate(model, glm::radians(text->rotation.y), glm::vec3(0, 1, 0));
				model = glm::rotate(model, glm::radians(text->rotation.z), glm::vec3(0, 0, 1));
				model = glm::scale(model, text->scale);
			}
			m_textUniform.Update({
				.projxview = camProjView,
				.model = model
			});
			glBindVertexArray(text->GetVAO());
			glDrawArrays(GL_TRIANGLES, 0, text->GetDrawCount());
			// remove 3d text
			std::swap(textArray[i], textArray.back());
			textArray.pop_back();
			i--;
		}
	}
	// 2d text
	glDisable(GL_DEPTH_TEST);
	const glm::mat4 ortho = glm::ortho(
		0.0f, static_cast<float>(m_settings.resolution.width),
		0.0f, static_cast<float>(m_settings.resolution.height));
	for (const auto& [fontName, textArray] : textMap) {
		if (textArray.empty()) continue;
		// set font texture
		const auto fontTexture = textArray[0]->GetFontTexture();
		m_textProgram->SetTexture("tGlyphs", GL_TEXTURE_2D_ARRAY, 0, fontTexture);

		// draw each text
		for (const auto& text : textArray) {
			glm::mat4 model = glm::mat4(1);
			if (text->useTransform) {
				model = text->transform;
			}
			else {
				auto textPos = text->position;
				auto textScale = text->scale;
				if (text->normalizedCoordinates) {
					textPos *= glm::vec3{ m_settings.resolution.width, m_settings.resolution.height, 1 };
					textScale *= glm::vec3{ m_settings.resolution.width, m_settings.resolution.height, 1 };
				}
				model = glm::translate(model, textPos + glm::vec3{0, 0, 1});
				model = glm::rotate(model, glm::radians(text->rotation.x), glm::vec3(1, 0, 0));
				model = glm::rotate(model, glm::radians(text->rotation.y), glm::vec3(0, 1, 0));
				model = glm::rotate(model, glm::radians(text->rotation.z), glm::vec3(0, 0, 1));
				model = glm::scale(model, textScale);
			}
			m_textUniform.Update({
				.projxview = ortho,
				.model = model
			});
			glBindVertexArray(text->GetVAO());
			glDrawArrays(GL_TRIANGLES, 0, text->GetDrawCount());
		}
	}
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}
void Renderer::RenderLighting() const {
	m_lightingProgram->Use();
	m_lightingProgram->SetTexture("tDepth", GL_TEXTURE_2D, 0, m_gbuffer.GetDepthTexture());
	m_lightingProgram->SetTexture("tMaterial", GL_TEXTURE_2D, 1, m_gbuffer.GetMaterialTexture());
	m_lightingProgram->SetTexture("tNormal", GL_TEXTURE_2D, 2, m_gbuffer.GetNormalTexture());
	m_lightingProgram->SetTexture("tShadow", GL_TEXTURE_2D_ARRAY, 3, m_csmbuffer.GetDepthTextureArray());
	glDrawArrays(GL_TRIANGLES, 0, 3);
}
void Renderer::RenderFXAA() const {
	m_fxaaProgram->Use();
	m_fxaaProgram->SetTexture("tColor", GL_TEXTURE_2D, 0, m_fxaabuffer.GetColorTexture());
	glDrawArrays(GL_TRIANGLES, 0, 3);
}
