#pragma once

#include <entt/entt.hpp>

#include "Components.h"

inline entt::entity CreateDefaultEntity(entt::registry& registry) {
	const auto entity = registry.create();
	registry.emplace<FlagComponent>(entity, FlagComponent{EntityFlags::NONE});
	return entity;
}
