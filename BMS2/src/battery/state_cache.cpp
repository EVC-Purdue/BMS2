#include "battery/state_cache.hpp"

#include "freertos/FreeRTOS.h"

namespace battery_state_cache
{

    namespace
    {

        Snapshot s_snapshot = {};
        portMUX_TYPE s_snapshot_mux = portMUX_INITIALIZER_UNLOCKED;

    } // namespace

    void publish(const Snapshot &snapshot)
    {
        taskENTER_CRITICAL(&s_snapshot_mux);
        s_snapshot = snapshot;
        taskEXIT_CRITICAL(&s_snapshot_mux);
    }

    void read(Snapshot &out_snapshot)
    {
        taskENTER_CRITICAL(&s_snapshot_mux);
        out_snapshot = s_snapshot;
        taskEXIT_CRITICAL(&s_snapshot_mux);
    }

} // namespace battery_state_cache
