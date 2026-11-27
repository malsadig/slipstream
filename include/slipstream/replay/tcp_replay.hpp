#pragma once

#include "slipstream/messages.hpp"

namespace slipstream::replay {
    int runTcpReplay(int argc, char* argv[], MessageType selection);
}