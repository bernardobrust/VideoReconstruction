module;
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

export module inspector;

import types;
import platform;

export {
    // Just window data for now
    struct inspector_config {
        i16 initial_x, initial_y, initial_w, initial_h;
    };

    bool inspector_create(inspector_config* config);
    bool inspector_run();
    bool inspector_shutdown();
}

struct inspector_state {
    bool is_running;
    platform_state platform;
    i16 x, y, w, h;
};

static bool initialized = false;
static inspector_state state;

bool inspector_create(inspector_config* config) {
    // If this assert fails something went very wrong
    assert(!initialized);

    state.is_running = true;

    if (!platform_init(
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
        if (!platform_update(&state.platform)) state.is_running = false;
        // Update
        // Render
        // TODO: why does this print 4 times?
        printf("Hello!\n");
    }

    // No failing for now
    return true;
}

bool inspector_shutdown() {
    platform_shutdown(&state.platform);
    return EXIT_SUCCESS;
}
