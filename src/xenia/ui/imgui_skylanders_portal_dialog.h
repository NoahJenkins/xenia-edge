/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#ifndef XENIA_UI_IMGUI_SKYLANDERS_PORTAL_DIALOG_H_
#define XENIA_UI_IMGUI_SKYLANDERS_PORTAL_DIALOG_H_

#include <array>
#include <functional>
#include <memory>

#include "xenia/hid/portal/portal_dialog_model.h"
#include "xenia/ui/imgui_gamepad_dialog.h"

namespace xe::ui {
class ImGuiSkylandersPortalDialog final : public ImGuiGamepadDialog {
 public:
  ImGuiSkylandersPortalDialog(ImGuiDrawer* drawer, hid::InputSystem* input,
                              std::function<bool()> title_active);
  void CloseDialog() {
    lifetime_.reset();
    Close();
  }
  void SetOnCloseCallback(std::function<void()> callback) {
    on_close_ = std::move(callback);
  }

 protected:
  void OnClose() override;
  void OnDraw(ImGuiIO& io) override;

 private:
  enum class Step {
    kMain,
    kImportName,
    kExportConfirm,
    kRecoverConfirm,
    kMove
  };
  void SetStep(Step step) {
    step_ = step;
    focus_slot_ = step == Step::kMain;
    focus_step_ = step != Step::kMain;
  }
  void Refresh();
  void Run(hid::PortalOperation operation);
  void PickFile(bool exporting);
  void DrawSlots(const hid::PortalManagerSnapshot& portal, float height);
  void DrawLibrary(float height);
  void DrawActions(const hid::PortalManagerSnapshot& portal);
  void DrawStep(const hid::PortalManagerSnapshot& portal);
  void SelectBackend(hid::PortalBackendKind backend);

  hid::PortalManager* manager_;
  hid::PortalDialogModel model_;
  hid::PortalLibrarySnapshot library_;
  std::function<bool()> title_active_;
  std::function<void()> on_close_;
  // Deferred native pickers may outlive a dialog during app/title teardown.
  std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
  std::array<char, 256> search_{};
  std::array<char, 512> import_name_{};
  hid::PortalLibraryFilter filter_ = hid::PortalLibraryFilter::kAll;
  hid::PortalOperation pending_{hid::PortalOperationKind::kImport};
  Step step_ = Step::kMain;
  std::string message_;
  bool message_error_ = false;
  bool first_frame_ = true;
  bool focus_slot_ = true;
  bool focus_step_ = false;
};
}  // namespace xe::ui
#endif  // XENIA_UI_IMGUI_SKYLANDERS_PORTAL_DIALOG_H_
