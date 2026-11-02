// SPDX-License-Identifier: Apache-2.0
#include "linux_ffs_binding.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>

// usbdevice owns gadget setup/teardown. adbd binds only that explicitly supplied
// gadget, after opening FunctionFS endpoints, on startup and every reconnect.
// Return 1 when bound, 0 when already bound/unmanaged, or -1 with errno on error.
int bind_linux_functionfs(const char* udc_path, const char* controller) {
    if (!udc_path && !controller) return 0;
    if (!udc_path || !*udc_path || !controller || !*controller ||
        std::strpbrk(controller, "/\r\n")) {
        errno = EINVAL;
        return -1;
    }

    int fd = open(udc_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    char buffer[512];
    ssize_t count;
    do {
        count = read(fd, buffer, sizeof(buffer));
    } while (count < 0 && errno == EINTR);
    int saved_errno = errno;
    close(fd);
    if (count < 0) {
        errno = saved_errno;
        return -1;
    }
    if (count == sizeof(buffer)) {
        errno = EOVERFLOW;
        return -1;
    }
    std::string current(buffer, count);
    while (!current.empty() && (current.back() == '\n' || current.back() == '\r')) {
        current.pop_back();
    }
    if (current == controller) return 0;
    if (!current.empty()) {
        // Never unbind or take over a gadget attached to a different controller.
        errno = EBUSY;
        return -1;
    }

    // Do not create a missing configfs node or truncate it. A sysfs attribute
    // must receive the complete value in one write.
    fd = open(udc_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    const std::string value = std::string(controller) + "\n";
    do {
        count = write(fd, value.data(), value.size());
    } while (count < 0 && errno == EINTR);
    saved_errno = errno;
    close(fd);
    if (count < 0) {
        errno = saved_errno;
        return -1;
    }
    if (static_cast<size_t>(count) != value.size()) {
        errno = EIO;
        return -1;
    }
    return 1;
}
