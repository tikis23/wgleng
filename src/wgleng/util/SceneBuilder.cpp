#include "SceneBuilder.h"

#include <emscripten/fetch.h>
#include <glm/gtx/euler_angles.hpp>
#include <sstream>

#include "../core/Components.h"
#include "../core/EntityCreator.h"
#include "../rendering/Debug.h"
#include "../rendering/Highlights.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_internal.h"
#include "../vendor/imgui/imgui_stdlib.h"

SceneBuilder::SceneBuilder(entt::registry& registry, PhysicsWorld& physicsWorld)
	: m_registry(registry), m_physicsWorld(physicsWorld) {
	m_colliders.emplace_back("Box");
	m_colliders.emplace_back("Sphere");
	m_colliders.emplace_back("Capsule");
}

void SceneBuilder::AddModel(std::string_view name) {
	m_models.emplace_back(name);
}

void SceneBuilder::Play() {
	m_playing = !m_playing;
	for (int32_t i = 0; i < m_savedStates.size(); i++) {
		RecreateEntity(i, m_playing);
	}
	m_selectedEntities.clear();
}

void SceneBuilder::RecreateEntity(int32_t entityId, bool useMass, bool invisible) {
	const float globalScale = invisible ? 0 : 1;
	if (entityId == -1) return;
	entt::entity& entity = m_savedStates[entityId].first;
	const State& state = m_savedStates[entityId].second;
	if (entity != entt::null) m_registry.destroy(entity);
	entity = CreateDefaultEntity(m_registry);

	m_registry.emplace_or_replace<FlagComponent>(entity, FlagComponent{state.flags});

	if (!state.tag.empty()) {
		m_registry.emplace<TagComponent>(entity, TagComponent{state.tag});
	}

	if (state.selectedModel != -1) {
		m_registry.emplace<MeshComponent>(entity, MeshComponent{
			.mesh = MeshRegistry::Get(m_models[state.selectedModel]),
			.position = state.modelOffset,
			.rotation = state.modelRotation,
			.scale = globalScale * state.modelScale * (state.selectedCollider != -1 ? state.scale : glm::vec3{1})
		});
		if (state.selectedCollider == -1) {
			m_registry.emplace<TransformComponent>(entity, TransformComponent{
				.position = state.position,
				.rotation = state.rotation,
				.scale = globalScale * state.scale
			});
		}
	}
	if (state.selectedCollider != -1) {
		const float mass = useMass ? state.mass : 0;

		std::shared_ptr<btCollisionShape> col;
		if (m_colliders[state.selectedCollider] == "Box") {
			col = m_physicsWorld.GetBoxCollider(globalScale * state.boxColliderSize * state.scale);
		}
		else if (m_colliders[state.selectedCollider] == "Sphere") {
			col = m_physicsWorld.GetSphereCollider(globalScale * state.sphereColliderRadius * glm::length(state.scale));
		}
		else if (m_colliders[state.selectedCollider] == "Capsule") {
			col = m_physicsWorld.GetCapsuleCollider(globalScale * state.capsuleColliderRadius * glm::length(state.scale),
				state.capsuleColliderHeight * glm::length(state.scale));
		}
		const auto rb = m_physicsWorld.CreateRigidBody(entity, col, mass, state.position, state.rotation);
		m_registry.emplace<RigidBodyComponent>(entity, RigidBodyComponent{rb});
		rb->setFriction(state.friction);
	}
}

void SceneBuilder::Blink(int32_t entityId, bool clearOthers) {
	if (clearOthers) {
		for (auto&& [index, tp] : m_blinks) {
			tp -= 1h;
		}
	}
	if (entityId < 0) { // blink animation
		const TimePoint now;
		for (int i = 0; i < m_blinks.size(); i++) {
			auto [index, tp] = m_blinks[i];
			auto elapsed = now - tp;
			if (index >= m_savedStates.size()) {
				m_blinks.erase(m_blinks.begin() + i);
				i--;
				continue;
			}
			bool show = true;
			for (TimeDuration t = 0ms; t < elapsed; t += 150ms) {
				if (t > elapsed) break;
				show = !show;
			}

			if (elapsed > 2000ms) show = true;
			RecreateEntity(index, false, !show);
			if (elapsed > 2000ms) {
				m_blinks.erase(m_blinks.begin() + i);
				i--;
			}
		}
	}
	else { // add to blinking
		m_blinks.emplace_back(entityId, TimePoint());
	}
}

void SceneBuilder::FlagSelector() {
	static const std::vector<std::pair<const char*, EntityFlags::EntityFlagsEnum>> flagValues = {
		{"PICKABLE", EntityFlags::PICKABLE},
		{"INTERACTABLE", EntityFlags::INTERACTABLE},
		{"DISABLE_COLLISIONS", EntityFlags::DISABLE_COLLISIONS},
		{"DISABLE_GRAVITY", EntityFlags::DISABLE_GRAVITY},
	};
	if (ImGui::BeginChild("Entity flags", ImVec2(0, 0), ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysAutoResize |
		ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY)) {
		for (const auto& [flagName, flagValue] : flagValues) {
			int selectCount = 0;
			for (const auto i : m_selectedEntities) {
				if (m_savedStates[i].second.flags & flagValue) selectCount++;
			}
			bool selected = selectCount > 0;
			const bool partial = selected && selectCount < m_selectedEntities.size();
			if (partial) ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
			if (ImGui::Checkbox(flagName, &selected)) {
				for (const auto i : m_selectedEntities) {
					if (selected || partial) m_savedStates[i].second.flags |= flagValue;
					else m_savedStates[i].second.flags &= ~flagValue;
				}
			}
			if (partial) ImGui::PopItemFlag();
		}
	}
	ImGui::EndChild();
}

void SceneBuilder::Update() {
	// draw axis
	int32_t maxInt = 1000000;
	DebugDraw::DrawLine({-maxInt, 0, 0}, {maxInt, 0, 0}, {1, 0, 0});
	DebugDraw::DrawLine({0, -maxInt, 0}, {0, maxInt, 0}, {0, 1, 0});
	DebugDraw::DrawLine({0, 0, -maxInt}, {0, 0, maxInt}, {0, 0, 1});
	// draw floor
	for (int i = 0; i < 3000; i++) {
		int scale = i * 20;
		DebugDraw::DrawLine({-maxInt, 0, scale}, {maxInt, 0, scale}, {0.5, 0.5, 0.5});
		DebugDraw::DrawLine({-maxInt, 0, -scale}, {maxInt, 0, -scale}, {0.5, 0.5, 0.5});
		DebugDraw::DrawLine({scale, 0, -maxInt}, {scale, 0, maxInt}, {0.5, 0.5, 0.5});
		DebugDraw::DrawLine({-scale, 0, -maxInt}, {-scale, 0, maxInt}, {0.5, 0.5, 0.5});
	}

	if (m_playing) {
		m_blinks.clear();
		return;
	}

	// tool window
	if (ImGui::Begin("Scene Builder")) {
		if (ImGui::BeginListBox("Entities")) {
			bool selectMultiple = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
			int somethingSelected = -1;
			for (int i = 0; i < m_savedStates.size(); i++) {
				ImGui::PushID(i);
				bool isSelected = m_selectedEntities.contains(i);
				if (ImGui::Selectable(std::format("({}) Entity {}", m_savedStates[i].second.tag, i + 1).c_str(), isSelected)) {
					if (selectMultiple) {
						if (isSelected) m_selectedEntities.erase(i);
						else {
							m_selectedEntities.insert(i);
							somethingSelected = i;
						}
					}
					else {
						if (isSelected) {
							if (m_selectedEntities.size() > 1) {
								m_selectedEntities.clear();
								m_selectedEntities.insert(i);
								somethingSelected = i;
							}
							else {
								m_selectedEntities.clear();
							}
						}
						else {
							m_selectedEntities.clear();
							m_selectedEntities.insert(i);
							somethingSelected = i;
						}
					}
				}
				ImGui::PopID();
			}
			ImGui::EndListBox();
			if (somethingSelected >= 0) Blink(somethingSelected, true);
		}

		if (ImGui::Button("Create")) {
			m_savedStates.emplace_back(entt::null, State{});
			m_selectedEntities.clear();
			m_selectedEntities.insert(m_savedStates.size() - 1);
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete")) {
			// remove blinks
			for (auto&& [index, tp] : m_blinks) {
				if (m_selectedEntities.contains(index)) tp -= 1h;
			}

			// erase entities
			std::vector<int32_t> deletedIds;
			for (auto i : m_selectedEntities) {
				for (auto deletedId : deletedIds) {
					if (i > deletedId) i--;
				}
				m_registry.destroy(m_savedStates[i].first);
				m_savedStates.erase(m_savedStates.begin() + i);
				deletedIds.push_back(i);
			}
			m_selectedEntities.clear();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clone")) {
			std::set<int32_t> newSelections;
			for (auto i : m_selectedEntities) {
				auto stateCopy = m_savedStates[i].second;
				m_savedStates.emplace_back(entt::null, stateCopy);
				int32_t newId = m_savedStates.size() - 1;
				newSelections.insert(newId);
				RecreateEntity(newId);
			}
			m_selectedEntities = newSelections;
		}

		ImGui::Separator();

		if (m_selectedEntities.size() > 1) {
			glm::vec3 offset{0};
			glm::vec3 rotate{0};
			std::string multiTag;
			bool tagsEqual = true;
			for (auto i : m_selectedEntities) {
				if (multiTag.empty()) multiTag = m_savedStates[i].second.tag;
				else if (m_savedStates[i].second.tag != multiTag) {
					tagsEqual = false;
					break;
				}
			}
			if (!tagsEqual) multiTag = "";

			if (ImGui::InputText("Tag", &multiTag)) {
				for (auto i : m_selectedEntities) {
					m_savedStates[i].second.tag = multiTag;
				}
			}
			FlagSelector();
			ImGui::Separator();

			ImGui::DragFloat3("Offset", &offset.x, m_dragSpeed);
			ImGui::DragFloat3("Rotate (not correct)", &rotate.x, m_dragSpeed);
			// rotation is not correct probably because some objects models are not centered on all axis

			// calculate center
			glm::vec3 center{0};
			for (auto i : m_selectedEntities) {
				center += m_savedStates[i].second.position + offset;
			}
			center /= m_selectedEntities.size();

			// get transform matrix
			glm::mat4 transform(1);
			transform = glm::translate(transform, center);
			transform = glm::rotate(transform, glm::radians(rotate.x), {1, 0, 0});
			transform = glm::rotate(transform, glm::radians(rotate.y), {0, 1, 0});
			transform = glm::rotate(transform, glm::radians(rotate.z), {0, 0, 1});
			transform = glm::translate(transform, -center);

			for (auto i : m_selectedEntities) {
				glm::mat4 model(1);
				model = glm::translate(model, m_savedStates[i].second.position + offset);
				model = glm::rotate(model, glm::radians(m_savedStates[i].second.rotation.x), {1, 0, 0});
				model = glm::rotate(model, glm::radians(m_savedStates[i].second.rotation.y), {0, 1, 0});
				model = glm::rotate(model, glm::radians(m_savedStates[i].second.rotation.z), {0, 0, 1});

				glm::mat4 rotated = transform * model;
				glm::vec3 newRot;
				glm::extractEulerAngleXYZ(rotated, newRot.x, newRot.y, newRot.z);
				newRot = glm::degrees(newRot);
				glm::vec3 newPos = glm::vec3(rotated[3]);

				m_savedStates[i].second.position = newPos;
				m_savedStates[i].second.rotation = newRot;
				RecreateEntity(i);
			}
			ImGui::Separator();
		}
		else if (m_selectedEntities.size() == 1) {
			int32_t selectedEntity = *m_selectedEntities.begin();
			State& state = m_savedStates[selectedEntity].second;
			ImGui::InputText("Tag", &state.tag);

			FlagSelector();
			ImGui::Separator();

			ImGui::DragFloat3("Position", &state.position.x, m_dragSpeed);
			ImGui::DragFloat3("Rotation", &state.rotation.x, m_dragSpeed);
			ImGui::DragFloat3("Scale", &state.scale.x, m_dragSpeed);
			ImGui::Separator();

			if (ImGui::BeginListBox("Models")) {
				for (int i = 0; i < m_models.size(); i++) {
					ImGui::PushID(i);
					bool isSelected = i == state.selectedModel;
					if (ImGui::Selectable(m_models[i].c_str(), isSelected)) {
						if (isSelected) state.selectedModel = -1;
						else state.selectedModel = i;
					}
					ImGui::PopID();
				}
				ImGui::EndListBox();
			}
			if (state.selectedModel != -1) {
				ImGui::DragFloat3("Model offset", &state.modelOffset.x, m_dragSpeed);
				ImGui::DragFloat3("Model Rotation", &state.modelRotation.x, m_dragSpeed);
				ImGui::DragFloat3("Model Scale", &state.modelScale.x, m_dragSpeed);
			}

			ImGui::Separator();
			if (ImGui::BeginListBox("Colliders")) {
				for (int i = 0; i < m_colliders.size(); i++) {
					ImGui::PushID(i);
					bool isSelected = i == state.selectedCollider;
					if (ImGui::Selectable(m_colliders[i].c_str(), isSelected)) {
						if (isSelected) state.selectedCollider = -1;
						else state.selectedCollider = i;
					}
					ImGui::PopID();
				}
				ImGui::EndListBox();
			}
			if (state.selectedCollider != -1) {
				ImGui::DragFloat("Mass (0 for static)", &state.mass, 10.f * m_dragSpeed);
				ImGui::DragFloat("Friction", &state.friction, m_dragSpeed);
				if (m_colliders[state.selectedCollider] == "Box") {
					ImGui::DragFloat3("Box Collider Size", &state.boxColliderSize.x, m_dragSpeed);
				}
				else if (m_colliders[state.selectedCollider] == "Sphere") {
					ImGui::DragFloat("Sphere Collider Radius", &state.sphereColliderRadius, m_dragSpeed);
				}
				else if (m_colliders[state.selectedCollider] == "Capsule") {
					ImGui::DragFloat("Capsule Collider Radius", &state.capsuleColliderRadius, m_dragSpeed);
					ImGui::DragFloat("Capsule Collider Height", &state.capsuleColliderHeight, m_dragSpeed);
				}
			}

			state.rotation.x = fmod(state.rotation.x, 360);
			state.rotation.y = fmod(state.rotation.y, 360);
			state.rotation.z = fmod(state.rotation.z, 360);
			state.modelRotation.x = fmod(state.modelRotation.x, 360);
			state.modelRotation.y = fmod(state.modelRotation.y, 360);
			state.modelRotation.z = fmod(state.modelRotation.z, 360);
			RecreateEntity(selectedEntity);
			ImGui::Separator();
		}
		ImGui::DragFloat("Drag Speed", &m_dragSpeed, 0.01f, 0.01f, 2.f);
		ImGui::Separator();

		ImGui::InputText("##savename", &m_saveName);
		ImGui::SameLine();
		if (ImGui::Button("Save")) Save();
		ImGui::SameLine();
		if (ImGui::Button("Load")) Load();

		if (ImGui::Button("Reset")) Reset();
	}
	ImGui::End();
	Blink(-1);
	for (int i : m_selectedEntities) {
		auto meshComp = m_registry.try_get<MeshComponent>(m_savedStates[i].first);
		if (meshComp) {
			meshComp->highlightId = Highlights::GetHighlightId("yellow");
		}
	}
}

void SceneBuilder::Reset() {
	for (auto&& [entity, state] : m_savedStates) {
		m_registry.destroy(entity);
	}
	m_selectedEntities.clear();
	m_savedStates.clear();
}

void SceneBuilder::Load() {
#ifdef SHADER_HOT_RELOAD
	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	attr.requestMethod[0] = 'G';
	attr.requestMethod[1] = 'E';
	attr.requestMethod[2] = 'T';
	attr.requestMethod[3] = 0;
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attr.userData = this;

	attr.onsuccess = [](emscripten_fetch_t* fetch) {
		auto builder = static_cast<SceneBuilder*>(fetch->userData);
		if (fetch->status == 200) {
			// parse data
			std::string data(fetch->data, fetch->numBytes);
			// remove header/footer
			data = data.substr(data.find_first_of('{') + 1);
			data = data.substr(0, data.find_last_of('}'));
			std::istringstream stream(data);

			// helper func
			auto readvec3 = [](std::istringstream& vec3Stream) {
				glm::vec3 v;
				vec3Stream.ignore();
				vec3Stream >> v.x;
				vec3Stream.ignore();
				vec3Stream >> v.y;
				vec3Stream.ignore();
				vec3Stream >> v.z;
				vec3Stream.ignore();
				return v;
			};

			// parse lines
			std::vector<State> states;
			std::string linestr;
			while (std::getline(stream, linestr)) {
				if (linestr.length() < 15) continue;
				std::istringstream line(linestr);
				State state;

				// opening brace
				line.ignore(256, '{');

				// global stuff
				state.position = readvec3(line);
				line.ignore();
				state.rotation = readvec3(line);
				line.ignore();
				state.scale = readvec3(line);
				line.ignore();

				// model
				line >> state.selectedModel;
				line.ignore();
				state.modelOffset = readvec3(line);
				line.ignore();
				state.modelRotation = readvec3(line);
				line.ignore();
				state.modelScale = readvec3(line);
				line.ignore();

				// tag
				line.ignore();
				std::getline(line, state.tag, '"');
				line.ignore();

				// flag
				line >> state.flags;
				line.ignore();

				// collider
				line >> state.selectedCollider;
				line.ignore();
				line >> state.mass;
				line.ignore();
				line >> state.friction;
				line.ignore();

				// box collider
				state.boxColliderSize = readvec3(line);
				line.ignore();

				// sphere collider
				line >> state.sphereColliderRadius;
				line.ignore();

				// capsule collider
				line >> state.capsuleColliderRadius;
				line.ignore();
				line >> state.capsuleColliderHeight;

				states.push_back(state);
			}
			builder->Load(states.size(), states.data(), true);
		}
		else {
			printf("Failed to load scene: %s\n", fetch->url);
		}
		emscripten_fetch_close(fetch);
	};
	attr.onerror = [](emscripten_fetch_t* fetch) {
		printf("Failed to load scene: %s\n", fetch->url);
		emscripten_fetch_close(fetch);
	};
	emscripten_fetch(&attr, ("http://localhost:8000/scenes/" + std::string(m_saveName) + ".h").c_str());
#else
	printf("Loading from url only available in SHADER_HOT_RELOAD mode.\n");
#endif
}
void SceneBuilder::Load(uint32_t stateCount, const State* states, bool saveable) {
	Reset();
	for (uint32_t i = 0; i < stateCount; i++) {
		m_savedStates.emplace_back(entt::null, states[i]);
		RecreateEntity(i, !saveable);
	}
	if (!saveable) m_savedStates.clear();
}
void SceneBuilder::Save() {
#ifdef SHADER_HOT_RELOAD
	// create save data
	std::string* saveDataIpl = new std::string("");
	std::string& saveData = *saveDataIpl;
	{
		auto vec3Str = [](const glm::vec3& v) {
			return std::format("{{{},{},{}}}", v.x, v.y, v.z);
		};
		saveData += "#pragma once\n\n";
		saveData += "#include <stdint.h>\n";
		saveData += "#include <wgleng/util/SceneBuilder.h>\n\n";
		saveData += std::format("constexpr uint32_t {}_stateVersion = {};\n", m_saveName, m_stateVersion);
		saveData += std::format("constexpr uint32_t {}_stateCount = {};\n", m_saveName, m_savedStates.size());
		saveData += std::format("constexpr SceneBuilder::State {}_states[] = {{\n", m_saveName);
		for (const auto& pair : m_savedStates) {
			const State& state = pair.second;
			saveData += std::format("   {{{},{},{},", vec3Str(state.position), vec3Str(state.rotation), vec3Str(state.scale));
			saveData += std::format("{},{},{},{},", state.selectedModel, vec3Str(state.modelOffset), vec3Str(state.modelRotation),
				vec3Str(state.modelScale));
			saveData += std::format("\"{}\",{},", state.tag, state.flags);
			saveData += std::format("{},{},{},{},", state.selectedCollider, state.mass, state.friction, vec3Str(state.boxColliderSize));
			saveData += std::format("{},{},{}}},\n", state.sphereColliderRadius, state.capsuleColliderRadius, state.capsuleColliderHeight);
		}
		saveData += "};\n";
	}

	// save to file
	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	const char* headers[] = {
		"Content-Type", "text/plain", nullptr
	};
	attr.requestHeaders = headers;
	attr.requestMethod[0] = 'P';
	attr.requestMethod[1] = 'O';
	attr.requestMethod[2] = 'S';
	attr.requestMethod[3] = 'T';
	attr.requestMethod[4] = 0;
	attr.requestData = saveData.c_str();
	attr.requestDataSize = saveData.size();
	attr.userData = saveDataIpl;

	auto finishSequence = [](emscripten_fetch_t* fetch) {
		const auto usaveDataIpl = static_cast<std::string*>(fetch->userData);
		delete usaveDataIpl;
		emscripten_fetch_close(fetch);
	};

	attr.onsuccess = finishSequence;
	attr.onerror = finishSequence;

	emscripten_fetch(&attr, ("http://localhost:8000/SaveScene/" + m_saveName).c_str());
#else
	printf("Saving only available in SHADER_HOT_RELOAD mode.\n");
#endif
}
