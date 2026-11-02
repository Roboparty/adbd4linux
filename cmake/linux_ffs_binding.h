// SPDX-License-Identifier: Apache-2.0
#pragma once

// 1: newly bound, 0: already bound/unmanaged, -1: error with errno set.
int bind_linux_functionfs(const char* udc_path, const char* controller);
