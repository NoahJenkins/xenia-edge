/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/hid/portal/portal_flags.h"

#ifdef XE_PLATFORM_WIN32
#define XE_DEFAULT_PORTAL_BACKEND "physical"
#else
#define XE_DEFAULT_PORTAL_BACKEND "disabled"
#endif

DEFINE_string(portal_backend, XE_DEFAULT_PORTAL_BACKEND,
              "Portal backend: disabled, physical (Windows), or virtual. "
              "Virtual guest reports remain disabled pending Xbox evidence.",
              "HID");
DEFINE_path(skylanders_figure_library, std::filesystem::path{},
            "Managed figure library. Empty uses storage/skylanders/figures.",
            "Storage");
