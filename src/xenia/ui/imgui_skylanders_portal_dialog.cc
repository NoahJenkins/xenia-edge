/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/ui/imgui_skylanders_portal_dialog.h"

#include <algorithm>
#include <cstring>

#include "third_party/imgui/imgui.h"
#include "xenia/config.h"
#include "xenia/hid/input_system.h"
#include "xenia/hid/portal/portal_flags.h"
#include "xenia/ui/file_picker.h"

namespace xe::ui {
namespace {
float ControlHeight() { return std::max(44.0f, ImGui::GetFrameHeight()); }
bool Button(const char* label, float width = 150.0f) {
  return ImGui::Button(label, ImVec2(width, ControlHeight()));
}
std::filesystem::path FromUtf8(const char* text) {
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(text)));
}
bool Row(const std::string& title, const std::string& status, bool selected) {
  const float height = std::max(44.0f, ImGui::GetFontSize() * 2.0f + 10.0f);
  const bool clicked =
      ImGui::Selectable("##row", selected, 0, ImVec2(0, height));
  const auto min = ImGui::GetItemRectMin();
  const auto max = ImGui::GetItemRectMax();
  auto* draw = ImGui::GetWindowDrawList();
  draw->PushClipRect(min, max, true);
  const std::string name = std::string(selected ? "> " : "  ") + title;
  draw->AddText(ImVec2(min.x + 6, min.y + 3), ImGui::GetColorU32(ImGuiCol_Text),
                name.c_str());
  draw->AddText(ImVec2(min.x + 6, min.y + ImGui::GetFontSize() + 5),
                ImGui::GetColorU32(ImGuiCol_TextDisabled), status.c_str());
  draw->PopClipRect();
  return clicked;
}
const char* BackendName(hid::PortalBackendKind kind) {
  switch (kind) {
    case hid::PortalBackendKind::kVirtual:
      return "Virtual";
    case hid::PortalBackendKind::kPhysical:
      return "Physical";
    default:
      return "Disabled";
  }
}
const char* ActionName(hid::PortalOperationKind kind) {
  switch (kind) {
    case hid::PortalOperationKind::kAdd:
      return "Add to slot";
    case hid::PortalOperationKind::kRemove:
      return "Remove";
    case hid::PortalOperationKind::kReplace:
      return "Replace";
    case hid::PortalOperationKind::kMove:
      return "Move...";
    case hid::PortalOperationKind::kImport:
      return "Import...";
    case hid::PortalOperationKind::kExport:
      return "Export...";
    case hid::PortalOperationKind::kRecover:
      return "Recover...";
  }
  return "Unknown";
}
}  // namespace

ImGuiSkylandersPortalDialog::ImGuiSkylandersPortalDialog(
    ImGuiDrawer* drawer, hid::InputSystem* input,
    std::function<bool()> title_active)
    : ImGuiGamepadDialog(drawer, input),
      manager_(input->GetPortal()),
      title_active_(std::move(title_active)) {
  Refresh();
}
void ImGuiSkylandersPortalDialog::OnClose() {
  lifetime_.reset();
  if (on_close_) {
    on_close_();
  }
}
void ImGuiSkylandersPortalDialog::Refresh() {
  library_ = manager_->ListLibrary();
}
void ImGuiSkylandersPortalDialog::SelectBackend(
    hid::PortalBackendKind backend) {
  auto result = manager_->SelectBackend(backend, title_active_());
  message_error_ = !result.success;
  message_ = hid::PortalDialogModel::Message(result);
  if (result.success) {
    const std::string value =
        backend == hid::PortalBackendKind::kVirtual    ? "virtual"
        : backend == hid::PortalBackendKind::kPhysical ? "physical"
                                                       : "disabled";
    hid::PersistPortalBackend(value);
    config::SaveConfig();
    Refresh();
  }
}
void ImGuiSkylandersPortalDialog::Run(hid::PortalOperation operation) {
  const auto result = manager_->Apply(operation);
  message_error_ = !result.success;
  message_ = hid::PortalDialogModel::Message(result);
  if (result.success) {
    message_ = std::string(ActionName(operation.kind)) + ": complete.";
    SetStep(Step::kMain);
  }
  Refresh();
}
void ImGuiSkylandersPortalDialog::PickFile(bool exporting) {
  std::weak_ptr<int> weak = lifetime_;
  const auto operation =
      model_.Operation(exporting ? hid::PortalOperationKind::kExport
                                 : hid::PortalOperationKind::kImport,
                       manager_->Snapshot());
  const auto root = library_.root;
  imgui_drawer()->PostDeferredCallback([this, weak, exporting, operation,
                                        root]() mutable {
    if (weak.expired()) {
      return;
    }
    auto picker = FilePicker::Create();
    picker->set_mode(exporting ? FilePicker::Mode::kSave
                               : FilePicker::Mode::kOpen);
    picker->set_title(exporting ? "Export Skylanders figure"
                                : "Import Skylanders figure");
    picker->set_extensions(
        {{"Raw figures", "*.sky;*.bin;*.dump;*.dmp"}, {"All files", "*.*"}});
    picker->set_initial_directory(root);
    picker->set_overwrite_prompt(false);
    if (exporting) {
      picker->set_file_name("figure.sky");
    }
    const bool selected = picker->Show(input_system_->window());
    if (weak.expired() || !selected || picker->selected_files().empty()) {
      return;
    }
    pending_ = operation;
    if (exporting) {
      pending_.destination = picker->selected_files().front();
      std::error_code error;
      const bool exists = std::filesystem::exists(pending_.destination, error);
      if (error) {
        message_error_ = true;
        message_ = "Cannot inspect the export destination.";
      } else if (exists) {
        SetStep(Step::kExportConfirm);
      } else {
        Run(pending_);
      }
    } else {
      pending_.path = picker->selected_files().front();
      const auto name = hid::PortalSessionStore::Utf8(pending_.path.filename());
      const auto initial =
          name.size() < import_name_.size() ? name : "figure.sky";
      import_name_.fill(0);
      std::memcpy(import_name_.data(), initial.data(), initial.size());
      SetStep(Step::kImportName);
    }
  });
}
void ImGuiSkylandersPortalDialog::DrawSlots(
    const hid::PortalManagerSnapshot& portal, float height) {
  ImGui::TextUnformatted("Portal slots");
  ImGui::BeginChild("slots", ImVec2(0, height),
                    ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
  for (const auto& slot : portal.slots) {
    ImGui::PushID(slot.slot);
    const bool selected = slot.slot == model_.selected_slot();
    const auto label = "Slot " + std::to_string(slot.slot + 1);
    const auto name = slot.figure
                          ? hid::PortalSessionStore::Utf8(
                                portal.figure_paths[slot.slot].filename())
                          : "Empty";
    if (focus_slot_ && selected) {
      ImGui::SetKeyboardFocusHere();
    }
    if (Row(label, name, selected)) {
      model_.SelectSlot(slot.slot);
    }
    if (first_frame_ && selected) {
      ImGui::SetItemDefaultFocus();
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
  focus_slot_ = false;
}
void ImGuiSkylandersPortalDialog::DrawLibrary(float height) {
  ImGui::TextUnformatted("Figure library");
  ImGui::BeginChild("library", ImVec2(0, height),
                    ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
  const auto visible =
      hid::PortalDialogModel::Filter(library_, search_.data(), filter_);
  const auto* selected = model_.SelectedFigure(library_);
  if (visible.empty()) {
    ImGui::TextWrapped(
        library_.entries.empty()
            ? "No figures yet. Import a raw figure file to start."
            : "No files match this search and filter.");
  }
  for (const auto index : visible) {
    const auto& entry = library_.entries[index];
    ImGui::PushID(static_cast<int>(index));
    if (Row(entry.name, hid::PortalDialogModel::Status(entry),
            selected && selected->path == entry.path)) {
      model_.SelectFigure(entry.path);
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
}
void ImGuiSkylandersPortalDialog::DrawActions(
    const hid::PortalManagerSnapshot& portal) {
  const auto actions = model_.Actions(portal, library_);
  const float width = 145.0f;
  size_t column = 0;
  const size_t columns = std::max<size_t>(
      1, static_cast<size_t>(ImGui::GetContentRegionAvail().x /
                             (width + ImGui::GetStyle().ItemSpacing.x)));
  for (const auto kind : actions) {
    if (column++ % columns) {
      ImGui::SameLine();
    }
    if (!Button(ActionName(kind), width)) {
      continue;
    }
    pending_ = model_.Operation(kind, portal);
    if (kind == hid::PortalOperationKind::kImport ||
        kind == hid::PortalOperationKind::kExport) {
      PickFile(kind == hid::PortalOperationKind::kExport);
    } else if (kind == hid::PortalOperationKind::kMove) {
      SetStep(Step::kMove);
    } else if (kind == hid::PortalOperationKind::kRecover) {
      SetStep(Step::kRecoverConfirm);
    } else {
      Run(pending_);
    }
    break;
  }
}
void ImGuiSkylandersPortalDialog::DrawStep(
    const hid::PortalManagerSnapshot& portal) {
  if (step_ == Step::kImportName) {
    ImGui::TextUnformatted("Import figure");
    ImGui::TextWrapped("Source: %s",
                       hid::PortalSessionStore::Utf8(pending_.path).c_str());
    ImGui::TextWrapped(
        "Choose a name inside your figure library. The source file stays "
        "unchanged.");
    ImGui::TextUnformatted("Library filename");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##importname", import_name_.data(), import_name_.size());
    if (focus_step_) {
      ImGui::SetKeyboardFocusHere();
    }
    if (Button("Cancel")) {
      SetStep(Step::kMain);
    }
    ImGui::SameLine();
    if (Button("Import")) {
      const auto name = FromUtf8(import_name_.data());
      if (!hid::PortalSessionStore::SafeRelativePath(name) ||
          name != name.filename()) {
        message_error_ = true;
        message_ = "Enter a file name without folders.";
      } else {
        pending_.destination = library_.root / name;
        Run(pending_);
      }
    }
  } else if (step_ == Step::kMove) {
    ImGui::TextWrapped("Move from slot %u. Choose an empty slot.",
                       pending_.source_slot + 1);
    ImGui::BeginChild("destinations", ImVec2(0, 250),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
    for (const auto& slot : portal.slots) {
      if (slot.figure) {
        continue;
      }
      ImGui::PushID(slot.slot);
      if (Button(("Slot " + std::to_string(slot.slot + 1)).c_str(), -1)) {
        pending_.destination_slot = slot.slot;
        Run(pending_);
      }
      ImGui::PopID();
    }
    ImGui::EndChild();
    if (focus_step_) {
      ImGui::SetKeyboardFocusHere();
    }
    if (Button("Cancel")) {
      SetStep(Step::kMain);
    }
  } else {
    const bool export_confirm = step_ == Step::kExportConfirm;
    ImGui::TextUnformatted(export_confirm ? "Replace the export file?"
                                          : "Recover this figure?");
    const auto path = export_confirm ? pending_.destination : pending_.path;
    ImGui::TextWrapped("%s", hid::PortalSessionStore::Utf8(path).c_str());
    ImGui::TextWrapped(
        export_confirm
            ? "The existing destination will be replaced with the selected "
              "slot's figure."
            : "Xenia will check the file currently on disk and save it again. "
              "Figure access resumes only if recovery succeeds.");
    if (focus_step_) {
      ImGui::SetKeyboardFocusHere();
    }
    if (Button("Cancel")) {
      SetStep(Step::kMain);
    }
    ImGui::SameLine();
    if (Button(export_confirm ? "Replace file" : "Recover")) {
      pending_.overwrite_confirmed = export_confirm;
      Run(pending_);
      SetStep(Step::kMain);
    }
  }
  focus_step_ = false;
}
void ImGuiSkylandersPortalDialog::OnDraw(ImGuiIO& io) {
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
      ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(std::min(960.0f, io.DisplaySize.x - 24),
                                  std::min(740.0f, io.DisplaySize.y - 24)),
                           ImGuiCond_Always);
  if (first_frame_) {
    ImGui::SetNextWindowFocus();
  }
  bool open = true;
  const auto portal = manager_->Snapshot();
  ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(10, std::max(6.0f, (44.0f - ImGui::GetFontSize()) * 0.5f)));
  if (ImGui::Begin("Skylanders Portal", &open,
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize)) {
    if ((ImGui::IsKeyPressed(ImGuiKey_Escape) || ShouldCloseFromGamepad()) &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
      if (step_ == Step::kMain) {
        Close();
      } else {
        SetStep(Step::kMain);
      }
    }
    const float status_height = message_.empty() ? 0.0f : 90.0f;
    const bool main_virtual =
        step_ == Step::kMain &&
        portal.backend == hid::PortalBackendKind::kVirtual;
    const size_t action_count =
        main_virtual ? model_.Actions(portal, library_).size() : 0;
    const size_t columns = std::max<size_t>(
        1, static_cast<size_t>(ImGui::GetContentRegionAvail().x /
                               (145.0f + ImGui::GetStyle().ItemSpacing.x)));
    const float actions_height =
        ((action_count + columns - 1) / columns) *
        (ControlHeight() + ImGui::GetStyle().ItemSpacing.y);
    ImGui::BeginChild(
        "body",
        ImVec2(0, -ControlHeight() - 12 - status_height - actions_height),
        ImGuiChildFlags_NavFlattened);
    if (portal.backend == hid::PortalBackendKind::kVirtual) {
      ImGui::TextWrapped(
          "Virtual game support is not available yet. You can manage figures "
          "and slots.");
    }
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("Portal mode", BackendName(portal.backend))) {
      for (const auto backend :
           {hid::PortalBackendKind::kDisabled, hid::PortalBackendKind::kVirtual
#ifdef XE_PLATFORM_WIN32
            ,
            hid::PortalBackendKind::kPhysical
#endif
           }) {
        if (ImGui::Selectable(BackendName(backend),
                              backend == portal.backend)) {
          SelectBackend(backend);
        }
      }
      ImGui::EndCombo();
    }
    if (portal.backend == hid::PortalBackendKind::kVirtual) {
      if (step_ != Step::kMain) {
        DrawStep(portal);
      } else {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##search", "Search file names",
                                 search_.data(), search_.size());
        ImGui::SetNextItemWidth(240);
        int filter = static_cast<int>(filter_);
        if (ImGui::Combo("##filter", &filter,
                         "All figures\0Available\0Needs attention\0")) {
          filter_ = static_cast<hid::PortalLibraryFilter>(filter);
        }
        ImGui::SameLine();
        if (Button("Refresh", 120)) {
          Refresh();
        }
        const bool wide = ImGui::GetContentRegionAvail().x >= 680;
        if (wide &&
            ImGui::BeginTable("lists", 2, ImGuiTableFlags_SizingStretchProp)) {
          ImGui::TableSetupColumn("Slots", 0, 0.34f);
          ImGui::TableSetupColumn("Library", 0, 0.66f);
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          DrawSlots(portal, 230);
          ImGui::TableNextColumn();
          DrawLibrary(230);
          ImGui::EndTable();
        } else {
          DrawSlots(portal, 120);
          DrawLibrary(160);
        }
        ImGui::Text("Selected slot: %u", model_.selected_slot() + 1);
        if (const auto* selected = model_.SelectedFigure(library_)) {
          ImGui::TextWrapped("Figure: %s", selected->name.c_str());
          ImGui::TextWrapped("%s",
                             hid::PortalDialogModel::Status(*selected).c_str());
          if (selected->identity) {
            ImGui::Text("Figure ID: %u   Variant: %u",
                        selected->identity->character_id,
                        selected->identity->variant_id);
          }
          if (!selected->validation.issues.empty() &&
              ImGui::TreeNode("Validation details")) {
            for (const auto& issue : selected->validation.issues) {
              ImGui::TextWrapped("%s", issue.message.c_str());
            }
            ImGui::TreePop();
          }
        } else {
          ImGui::TextWrapped(
              "Select a library file to add or replace a figure.");
        }

        ImGui::TextWrapped(
            "Creation and reset are unavailable until figure format rules are "
            "verified.");
      }
      if (!library_.errors.empty() &&
          ImGui::TreeNode("Library and session notices")) {
        for (const auto& error : library_.errors) {
          ImGui::TextWrapped("%s", error.c_str());
        }
        ImGui::TreePop();
      }
    } else {
      ImGui::TextWrapped(
          portal.backend == hid::PortalBackendKind::kPhysical
              ? "Physical mode uses the connected USB portal. Choose Virtual "
                "while the game is stopped to manage files."
              : "Choose Virtual to open your local figure library. Stop the "
                "game before changing portal mode.");
    }
    ImGui::EndChild();
    if (main_virtual) {
      DrawActions(portal);
    }
    if (!message_.empty()) {
      ImGui::BeginChild("operation_status", ImVec2(0, status_height));
      ImGui::TextUnformatted(message_error_ ? "Operation failed"
                                            : "Operation complete");
      ImGui::TextWrapped("%s", message_.c_str());
      ImGui::EndChild();
    }
    if (Button(step_ == Step::kMain ? "Close" : "Back", 130)) {
      if (step_ == Step::kMain) {
        Close();
      } else {
        SetStep(Step::kMain);
      }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("A / Enter: select    B / Escape: back");
  }
  ImGui::End();
  ImGui::PopStyleVar();
  first_frame_ = false;
  if (!open) {
    Close();
  }
}
}  // namespace xe::ui
