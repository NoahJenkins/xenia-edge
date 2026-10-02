/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#ifndef XENIA_HID_PORTAL_PORTAL_FLAGS_H_
#define XENIA_HID_PORTAL_PORTAL_FLAGS_H_

#include "xenia/base/cvar.h"

DECLARE_string(portal_backend);
DECLARE_path(skylanders_figure_library);

namespace xe::hid {
// Store an explicit user preference in the global configuration layer.
void PersistPortalBackend(const std::string& value);
}  // namespace xe::hid

#endif  // XENIA_HID_PORTAL_PORTAL_FLAGS_H_
