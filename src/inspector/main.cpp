import inspector;

using namespace vr::inspector;

int main() {
    inspector_config config = {0, 0, 200, 300};
    inspector_create(&config);

    inspector_run();

    return inspector_shutdown();
}
