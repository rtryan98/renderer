#include <cstdint>

#include "renderer/application.hpp"

int32_t main([[maybe_unused]] uint32_t argc, [[maybe_unused]] const char* argv[])
{
    auto application = ren::Application();
    application.run();
    return 0;
}
