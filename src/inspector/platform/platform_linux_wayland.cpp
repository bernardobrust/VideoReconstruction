module platform;

import types;
import <cstdio>;

#ifdef PLATFORM_LINUX_WAYLAND

bool platform_init(
    platform_state* plat_state,
    i16 x, i16 y, i16 w, i16 h
) {
    (void)plat_state; (void)x; (void)y; (void)w; (void)h;
    printf("Wayland not implemented yet\n");
    return false;
}

bool platform_update(platform_state* plat_state) {
    (void)plat_state;
    return false;
}

void platform_shutdown(platform_state* plat_state) {
    (void)plat_state;
}

#endif
