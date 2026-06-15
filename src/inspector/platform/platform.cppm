export module platform;

import types;

export namespace vr::platform {
    /*
        Internal state contains data about the window, depends on OS so it's a
        void*.
    */
    struct platform_state {
        void* internal_state;
    };

    /*
        Platform functions implementations are dispatched according to the OS
        specified during compilation.

        For now we just need windowing and events.
    */

    // All startup code including window creation
    // false return indicates fatal failure
    bool platform_init(
        platform_state* plat_state,
        i16 x, i16 y, i16 w, i16 h
    );

    bool platform_update(platform_state* plat_state);

    void platform_shutdown(platform_state* plat_state);
}
