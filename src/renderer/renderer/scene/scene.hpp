#pragma once

#include "renderer/scene/model.hpp"

#include <memory>
#include <vector>

namespace ren
{
class Scene
{
public:
    void add_model(Model* model);

private:
    std::vector<Model*> models;
};
}
