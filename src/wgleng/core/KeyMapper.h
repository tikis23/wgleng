#pragma once

#include <functional>
#include <vector>

#include "../io/Input.h"

class KeyMapper {
private:
	friend class KeyMapBuilder;
	struct KeyMapping {
		using InputFunc = bool(*)(int);
		std::vector<std::pair<InputFunc, int>> keys;
		std::function<void()> action;
	};
	std::vector<KeyMapping> m_mappings;

public:
	class KeyMapBuilder {
	public:
		KeyMapBuilder() = delete;
		~KeyMapBuilder() = default;
		KeyMapBuilder(KeyMapBuilder&&) = delete;
		KeyMapBuilder(const KeyMapBuilder&) = delete;
		KeyMapBuilder& operator=(KeyMapBuilder&&) = delete;
		KeyMapBuilder& operator=(const KeyMapBuilder&) = delete;

		bool Then(auto&& callback) {
			if (!m_mapper) return false;
			m_mapping.action = callback;
			m_mapper->m_mappings.push_back(m_mapping);
			m_mapper = nullptr;
			return true;
		}

		// SDL_SCANCODE_XX
		KeyMapBuilder& PressKey(SDL_Scancode scancode) {
			ExpandMapping(&Input::JustPressed, scancode);
			return *this;
		}
		// SDL_SCANCODE_XX
		KeyMapBuilder& HoldKey(SDL_Scancode scancode) {
			ExpandMapping(&Input::IsHeld, scancode);
			return *this;
		}
		// SDL_SCANCODE_XX
		KeyMapBuilder& ReleaseKey(SDL_Scancode scancode) {
			ExpandMapping(&Input::JustReleased, scancode);
			return *this;
		}
		// SDL_BUTTON_XXXX
		KeyMapBuilder& PressMouse(int button) {
			ExpandMapping(&Input::JustPressedMouse, button);
			return *this;
		}
		// SDL_BUTTON_XXXX
		KeyMapBuilder& HoldMouse(int button) {
			ExpandMapping(&Input::IsHeldMouse, button);
			return *this;
		}
		// SDL_BUTTON_XXXX
		KeyMapBuilder& ReleaseMouse(int button) {
			ExpandMapping(&Input::JustReleasedMouse, button);
			return *this;
		}

	private:
		void ExpandMapping(auto func, int key) {
			m_mapping.keys.push_back({(KeyMapping::InputFunc)func, key});
		}

		friend class KeyMapper;
		KeyMapBuilder(KeyMapper* mapper) : m_mapper{mapper} {}
		KeyMapper* m_mapper;
		KeyMapping m_mapping;
	};

	void Update() const {
		for (auto& mapping : m_mappings) {
			bool allKeysPressed = true;
			for (auto& key : mapping.keys) {
				if (!key.first(key.second)) {
					allKeysPressed = false;
					break;
				}
			}
			if (allKeysPressed) {
				mapping.action();
			}
		}
	}
	KeyMapBuilder AddMapping() {
		return KeyMapBuilder(this);
	}
	void Clear() {
		m_mappings.clear();
	}
};
