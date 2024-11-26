#pragma once

#include <array>
#include <functional>
#include <memory>
#include <stdint.h>
#include <vector>

template <typename ActionType, size_t ActionCount>
class Actions {
public:
	Actions() = default;
	~Actions() = default;

	using Callback = std::function<void()>;
	class Listener {
	public:
		Listener() = default;
		~Listener() {
			Close();
		}
		Listener(const Listener&) = delete;
		Listener& operator=(const Listener&) = delete;
		Listener(Listener&& other) noexcept
			: m_id{ other.m_id }, m_listeners{ other.m_listeners } {
			if (this == &other) return;
			other.m_listeners = nullptr;
		}
		Listener& operator=(Listener&& other) noexcept {
			if (this == &other) return *this;
			Close();
			m_id = other.m_id;
			m_listeners = other.m_listeners;
			other.m_listeners = nullptr;
			return *this;
		}

		void Close() {
			if (!m_listeners) return;
			for (size_t i = 0; i < m_listeners->size(); i++) {
				if ((*m_listeners)[i].first == m_id) {
					std::swap((*m_listeners)[i], m_listeners->back());
					m_listeners->pop_back();
					break;
				}
			}
			m_listeners = nullptr;
		}

	private:
		friend class Actions;
		uint64_t m_id;
		std::vector<std::pair<uint64_t, Callback>>* m_listeners{ nullptr };
	};

	bool IsEnabled(ActionType action) const {
		return m_actions[static_cast<size_t>(action)].enabled;
	}
	void Enable(ActionType action) {
		m_actions[static_cast<size_t>(action)].enabled = true;
	}
	void Disable(ActionType action) {
		m_actions[static_cast<size_t>(action)].enabled = false;
	}

	void Trigger(ActionType action) {
		if (!m_actions[static_cast<size_t>(action)].enabled) return;
		for (const auto& listener : m_actions[static_cast<size_t>(action)].listeners) {
			listener.second();
		}
	}
	Listener Listen(ActionType action, const Callback& callback) {
		Listener listener;
		listener.m_id = m_actions[static_cast<size_t>(action)].nextId++;
		listener.m_listeners = &m_actions[static_cast<size_t>(action)].listeners;

		m_actions[static_cast<size_t>(action)].listeners.push_back({listener.m_id, callback});

		return listener;
	}

private:
	struct ActionData {
		bool enabled{true};
		uint64_t nextId{0};
		std::vector<std::pair<uint64_t, Callback>> listeners;
	};
	std::array<ActionData, ActionCount> m_actions;
};
