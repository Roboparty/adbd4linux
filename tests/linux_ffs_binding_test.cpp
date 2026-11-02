// SPDX-License-Identifier: Apache-2.0
#include "../cmake/linux_ffs_binding.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>

int main() {
    char directory[] = "/tmp/adbd-binding-test.XXXXXX";
    assert(mkdtemp(directory));
    const std::string path = std::string(directory) + "/UDC";
    auto put = [&](const char* value) { std::ofstream(path) << value; };
    auto get = [&]() {
        std::ifstream input(path);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    assert(bind_linux_functionfs(nullptr, nullptr) == 0);
    assert(bind_linux_functionfs(path.c_str(), nullptr) == -1 && errno == EINVAL);
    assert(bind_linux_functionfs(nullptr, "controller") == -1 && errno == EINVAL);
    assert(bind_linux_functionfs(path.c_str(), "") == -1 && errno == EINVAL);
    assert(bind_linux_functionfs(path.c_str(), "bad\nname") == -1 && errno == EINVAL);
    assert(bind_linux_functionfs(path.c_str(), "controller") == -1 && errno == ENOENT);
    assert(access(path.c_str(), F_OK) == -1);
    for (int cycle = 0; cycle < 10; ++cycle) {
        // FunctionFS closes its final endpoint and configfs drops the UDC name.
        put("\n");
        assert(bind_linux_functionfs(path.c_str(), "controller") == 1);
        assert(get() == "controller\n");
        assert(bind_linux_functionfs(path.c_str(), "controller") == 0);
    }
    put("other-controller\n");
    assert(bind_linux_functionfs(path.c_str(), "controller") == -1 && errno == EBUSY);
    assert(get() == "other-controller\n");
    unlink(path.c_str());
    assert(bind_linux_functionfs(path.c_str(), "controller") == -1 && errno == ENOENT);
    rmdir(directory);
    std::cout << "FunctionFS binding tests passed (10 reconnects and error paths)\n";
}
