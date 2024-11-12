#include "Renderer.h"
#include "Renderer.h"

#include <unordered_map>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <emscripten/fetch.h>
#include <glm/gtc/type_ptr.hpp>

#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_impl_sdl2.h"
#include "../vendor/imgui/imgui_impl_opengl3.h"
#include "../core/Components.h"
#include "../io/Input.h"
#include "Debug.h"
#include "Highlights.h"

Renderer::Renderer()
	: m_viewportWidth{1}, m_viewportHeight{1}, m_gbuffer(m_viewportWidth, m_viewportHeight),
	  m_csmWidth{1024}, m_csmHeight{1024}, m_csmbuffer(m_csmWidth, m_csmHeight, {100, 500, 1000}) {
	ReloadShaders();

	// GL settings
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glClearColor(0.5f, 0.4f, 0.3f, 1.0f);
}
Renderer::~Renderer() {}

void Renderer::LoadShaderFromFile(std::string file) {
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
	m_shadersLoading = true;
	printf("Loading shaders ...\n");

	// mesh
	if (static_cast<uint64_t>(shaders) & static_cast<uint64_t>(ShaderType::MESH)) {
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
	if (static_cast<uint64_t>(shaders) & static_cast<uint64_t>(ShaderType::LIGHTING)) {
		m_lightingProgram = std::make_unique<ShaderProgram>("lighting");
		m_lightingProgram->SetConstant("MATERIALS_PER_UBO", std::to_string(m_materialsPerUniformBuffer));
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
	if (static_cast<uint64_t>(shaders) & static_cast<uint64_t>(ShaderType::CSM)) {
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
	// debug
	if (static_cast<uint64_t>(shaders) & static_cast<uint64_t>(ShaderType::DEBUG)) {
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
        std::string shaderFileToLoad = m_shaderLoadingQueue.front(); m_shaderLoadingQueue.pop_front();
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

	m_meshProgram->AddUniformBufferBinding("CameraUniform", m_cameraUniform.GetBindingIndex());
	m_meshProgram->AddUniformBufferBinding("ModelMatricesUniform", m_modelUniform.GetBindingIndex());

	m_lightingProgram->AddUniformBufferBinding("LightingInfoUniform", m_lightingInfoUniform.GetBindingIndex());
	m_lightingProgram->AddUniformBufferBinding("CSMUniform", m_csmUniform.GetBindingIndex());
	m_lightingProgram->AddUniformBufferBinding("MaterialUniform", m_materialUniform.GetBindingIndex());

	for (auto& csmProgram : m_csmPrograms) {
		csmProgram->AddUniformBufferBinding("CSMUniform", m_csmUniform.GetBindingIndex());
		csmProgram->AddUniformBufferBinding("ModelMatricesUniform", m_modelUniform.GetBindingIndex());
	}

	m_debugProgram->AddUniformBufferBinding("CameraUniform", m_cameraUniform.GetBindingIndex());

	m_shadersLoading = false;
	printf("Shaders loaded.\n");
}

void Renderer::Render(bool isHidden, const std::shared_ptr<Scene>& scene) {
	ImGui::Render();

	// check for shader reloads
	if (Highlights::HasChanged(true)) {
		ReloadShaders(ShaderType::LIGHTING);
	}

	if (isHidden || m_shadersLoading || !scene) {
		DebugDraw::Clear();
		return;
	}

	// triangle debug
	static bool showWireframe = false;
	if (Input::IsHeld(SDL_SCANCODE_LCTRL) && Input::JustPressed(SDL_SCANCODE_I)) showWireframe = !showWireframe;

	// update uniforms
	auto& camera = scene->GetCamera();
	camera->Update(m_viewportWidth, m_viewportHeight);
	const glm::mat4 camProjView = camera->GetProjectionMatrix() * camera->GetViewMatrix();
	m_cameraUniform.Update({
		.projxview = camProjView,
		.nearFarPlane = {camera->GetNearPlane(), camera->GetFarPlane()}
		});

	m_lightingInfoUniform.Update({
		.sunlightDir = glm::normalize(scene->sunlightDir),
		.sunlightColor = {1.0, 0.7, 0.8, 3.0},
		.cameraPos = camera->position,
		.viewportSize_nearFarPlane = {m_viewportWidth, m_viewportHeight, camera->GetNearPlane(), camera->GetFarPlane()},
		.invProjView = glm::inverse(camProjView)
		});

	static std::size_t lastMaterialCount = 0;
	std::size_t materialCount = MeshRegistry::GetMaterials().size();
	if (lastMaterialCount != materialCount) { // could be made more efficient but meh
		MaterialUniform materialData;
		std::copy(MeshRegistry::GetMaterials().begin(), MeshRegistry::GetMaterials().end(), materialData.materials);
		m_materialUniform.Update(materialData);
		lastMaterialCount = materialCount;
	}

	const auto lightSpaceMatrices = m_csmbuffer.GetLightSpaceMatrices(camera, scene->sunlightDir);
	CSMUniform csmData;
	std::copy(lightSpaceMatrices.begin(), lightSpaceMatrices.end(), csmData.lightSpaceMatrices);
	m_csmUniform.Update(csmData);

	// get renderable meshes and matrices
	constexpr auto computeModelMatrix = [](
		const glm::vec3& globalPos, const glm::vec3& localPos,
		const glm::vec3& globalRot, const glm::vec3& localRot,
		const glm::vec3& scale) -> glm::mat4 {
			glm::mat4 model{ 1 };
			model = glm::translate(model, globalPos);
			model = glm::rotate(model, globalRot.z, glm::vec3(0, 0, 1));
			model = glm::rotate(model, globalRot.y, glm::vec3(0, 1, 0));
			model = glm::rotate(model, globalRot.x, glm::vec3(1, 0, 0));
			model = glm::translate(model, localPos);
			model = glm::rotate(model, localRot.x, glm::vec3(1, 0, 0));
			model = glm::rotate(model, localRot.y, glm::vec3(0, 1, 0));
			model = glm::rotate(model, localRot.z, glm::vec3(0, 0, 1));
			model = glm::scale(model, scale);
			return model;
		};

	uint32_t matrixCount = 0;
	std::unordered_map<Mesh, std::vector<glm::mat4>> meshMatrices;
	std::unordered_map<Mesh, std::vector<std::pair<glm::mat4, glm::vec3>>> meshHighlights;
	for (auto&& [entity, meshComp, transformComp] :
		scene->registry.view<MeshComponent, TransformComponent>(entt::exclude<RigidBodyComponent>).each()) {
		// check if hidden
		if (meshComp.hidden || meshComp.hiddenPersistent) {
			meshComp.hidden = false;
			continue;
		}

		// get model matrix
		glm::mat4 model = computeModelMatrix(
			transformComp.position, meshComp.position,
			glm::radians(transformComp.rotation), glm::radians(meshComp.rotation),
			transformComp.scale * meshComp.scale);

		// !!!! EXTRA DATA PACKED INTO MATRIX, REQUIRES RESETING IN SHADER !!!!
		model[3][3] = meshComp.highlightId;
		meshComp.highlightId = 0;

		// add to map
		meshMatrices[meshComp.mesh].push_back(model);
		matrixCount++;
	}
	for (auto&& [entity, meshComp, rbComp] : scene->registry.view<MeshComponent, RigidBodyComponent>().each()) {
		// check if hidden
		if (meshComp.hidden || meshComp.hiddenPersistent) {
			meshComp.hidden = false;
			continue;
		}

		auto body = rbComp.body;
		if (!body) continue;


		// get model matrix
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

		// add to map
		meshMatrices[meshComp.mesh].push_back(model);
		matrixCount++;
	}

	// batch matrices and get render data
	std::vector<glm::mat4> batchedMatrices; // TODO: possible to avoid copying matrices into this single vector, but more complex
	batchedMatrices.reserve(matrixCount);
	std::vector<MeshBatch> batches;
	uint32_t currentUboOffset = 0;
	uint32_t currentBatchSize = 0;
	for (auto& [mesh, models] : meshMatrices) {
		uint32_t matricesLeft = models.size();
		while (matricesLeft > 0) {
			const uint32_t matricesInThisBatch = std::min(matricesLeft, m_matricesPerUniformBuffer - currentBatchSize);
			auto matricesOffset = models.begin() + (models.size() - matricesLeft);
			if (currentUboOffset >= batchedMatrices.size()) {
				batchedMatrices.resize(currentUboOffset + m_matricesPerUniformBuffer);
			}
			batchedMatrices.insert(batchedMatrices.begin() + currentUboOffset, matricesOffset, matricesOffset + matricesInThisBatch);
			matricesLeft -= matricesInThisBatch;
			currentBatchSize = (currentBatchSize + matricesInThisBatch) % m_matricesPerUniformBuffer;

			MeshBatch batchData;
			batchData.mesh = mesh;
			batchData.instanceOffset = currentUboOffset;
			batchData.instanceCount = matricesInThisBatch;
			batches.push_back(batchData);

			currentUboOffset += matricesInThisBatch;
			// fix for alignment
			int padding = (currentUboOffset * sizeof(glm::mat4)) % m_modelUniform.GetOffsetAlignment();
			padding = m_modelUniform.GetOffsetAlignment() - padding;
			currentUboOffset += padding / sizeof(glm::mat4);
			currentBatchSize += padding / sizeof(glm::mat4);
		}
	}

	// resize ubo if needed
	const uint32_t requiredSize = matrixCount * sizeof(glm::mat4) + sizeof(glm::mat4) * m_matricesPerUniformBuffer;
	if (m_modelUniform.GetSize() < requiredSize) {
		m_modelUniform.Resize(requiredSize + sizeof(glm::mat4) * m_matricesPerUniformBuffer);
	}
	// upload matrices
	m_modelUniform.Update(0, batchedMatrices.size() * sizeof(glm::mat4), batchedMatrices.data());

	// csm
	glViewport(0, 0, m_csmWidth, m_csmHeight);
	glCullFace(GL_FRONT);
	for (int i = 0; i < m_csmbuffer.GetFBOs().size(); i++) {
		const auto& csmProgram = m_csmPrograms[i];
		csmProgram->Use();
		const GLuint fbo = m_csmbuffer.GetFBOs()[i];
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glClear(GL_DEPTH_BUFFER_BIT);
		uint32_t currentVao = 0;
		for (auto& batch : batches) {
			auto vao = batch.mesh->GetVAO();
			if (currentVao != vao) {
				glBindVertexArray(vao);
				currentVao = vao;
			}
			m_modelUniform.Bind(batch.instanceOffset * sizeof(glm::mat4), m_matricesPerUniformBuffer * sizeof(glm::mat4));
			glDrawArraysInstanced(GL_TRIANGLES, 0, batch.mesh->GetDrawCount(), batch.instanceCount);
		}
	}

	glViewport(0, 0, m_viewportWidth, m_viewportHeight);
	glCullFace(GL_BACK);

	// world
	glBindFramebuffer(GL_FRAMEBUFFER, m_gbuffer.GetFBO());
	glClear(GL_DEPTH_BUFFER_BIT);
	glm::uvec4 uclearColor{ 0 };
	glm::vec4 fclearColor{ 0 };
	glClearBufferuiv(GL_COLOR, 0, glm::value_ptr(uclearColor));
	glClearBufferfv(GL_COLOR, 1, glm::value_ptr(fclearColor));
	m_meshProgram->Use();
	uint32_t currentVao = 0;
	for (auto& batch : batches) {
		auto vao = batch.mesh->GetVAO();
		if (currentVao != vao) {
			glBindVertexArray(vao);
			currentVao = vao;
		}
		m_modelUniform.Bind(batch.instanceOffset * sizeof(glm::mat4), m_matricesPerUniformBuffer * sizeof(glm::mat4));
		if (showWireframe) glDrawArraysInstanced(GL_LINE_STRIP, 0, batch.mesh->GetDrawCount(), batch.instanceCount);
		else glDrawArraysInstanced(GL_TRIANGLES, 0, batch.mesh->GetDrawCount(), batch.instanceCount);
	}

	// debug draw
	if (DebugDraw::IsEnabled()) {
		m_debugProgram->Use();
		DebugDraw::GetDrawer()->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawContactPoints);
		scene->GetPhysicsWorld().dynamicsWorld->setDebugDrawer(DebugDraw::GetDrawer());
		scene->GetPhysicsWorld().dynamicsWorld->debugDrawWorld();
		DebugDraw::Draw();
	}

	// lighting
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	m_lightingProgram->Use();
	m_lightingProgram->SetTexture("tDepth", GL_TEXTURE_2D, 0, m_gbuffer.GetDepthTexture());
	m_lightingProgram->SetTexture("tMaterial", GL_TEXTURE_2D, 1, m_gbuffer.GetMaterialTexture());
	m_lightingProgram->SetTexture("tNormal", GL_TEXTURE_2D, 2, m_gbuffer.GetNormalTexture());
	m_lightingProgram->SetTexture("tShadow", GL_TEXTURE_2D_ARRAY, 3, m_csmbuffer.GetDepthTextureArray());
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// imgui
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Renderer::SetViewportSize(int32_t width, int32_t height) {
	if (m_viewportWidth == width && m_viewportHeight == height) return;
	m_viewportWidth = width;
	m_viewportHeight = height;
	m_gbuffer.Resize(width, height);
}
