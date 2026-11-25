#include "test_suites.hpp"

int main() {
    runHeartbeatTests();
    runQuoteTests();
    runTradeTests();
    runSessionControlTests();
    runValidationTests();
    runStreamTests();
}