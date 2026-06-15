export module inspector;

import types;
import platform;

import <cassert>;
import <cstdlib>;
import <cstdio>;

namespace vr::inspector {
    struct inspector_state {
        bool is_running;
        platform::platform_state platform;
        i16 x, y, w, h;
    };

    static bool initialized = false;
    static inspector_state state;
}

export namespace vr::inspector {
    // Just window data for now
    struct inspector_config {
        i16 initial_x, initial_y, initial_w, initial_h;
    };

    bool inspector_create(inspector_config* config) {
        // If this assert fails something went very wrong
        assert(!initialized);

        state.is_running = true;

        if (!platform::platform_init(
            &state.platform,
            config->initial_x,
            config->initial_y,
            config->initial_w,
            config->initial_h)
        ) return EXIT_FAILURE;

        initialized = true;
        return true;
    }

    bool inspector_run() {
        while (state.is_running) {
            if (!platform::platform_update(&state.platform)) state.is_running = false;
            // Update
            // Render
            // TODO: why does this print 4 times?
            printf("Hello!\n");
        }

        // No failing for now
        return true;
    }

    bool inspector_shutdown() {
        platform::platform_shutdown(&state.platform);
        return EXIT_SUCCESS;
    }
}
