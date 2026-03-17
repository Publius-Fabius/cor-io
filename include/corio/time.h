
#include <chrono>

namespace corio
{
    template<typename D> D now_ms() {
        using namespace std::chrono;
        auto time = sys_time<milliseconds>::clock().now();
        return duration_cast<milliseconds>(time.time_since_epoch()).count();
    }
}