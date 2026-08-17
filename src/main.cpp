#include <cstdlib>
#include <cstring>
#include <utility>

#include "Game.hpp"
#include "ScriptEngine.hpp"

// Entry point.
//   jeu                       -> local (solo + local co-op via gamepads)
//   jeu --host [port]         -> host an authoritative LAN game
//   jeu --join <ip> [port]    -> join a host as a client
int main(int argc, char* argv[]) {
    game::NetConfig net;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--host") == 0) {
            net.mode = game::NetMode::Host;
            if (i + 1 < argc) net.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--join") == 0) {
            net.mode = game::NetMode::Client;
            if (i + 1 < argc) net.host = argv[++i];
            if (i + 1 < argc) net.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        }
    }

    game::GameContent content = game::ScriptEngine::loadContent("data");

    game::Game game(std::move(content), net);
    if (!game.init()) {
        return 1;
    }
    game.run();
    return 0;
}
