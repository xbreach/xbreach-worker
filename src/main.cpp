#include <spdlog/spdlog.h>

#include "version.hpp"

int main() {
    spdlog::info("xbreach-worker {} starting", xbreach::worker::version());
    spdlog::info("scaffold build: work loop not implemented yet");
    return 0;
}
