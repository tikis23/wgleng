#pragma once

#include <memory>

#include "../io/RenderTarget.h"
#include "../rendering/Renderer.h"
#include "Scene.h"

struct Context {
    RenderTarget rt;
    Renderer renderer;
    std::shared_ptr<Scene> scene;
};