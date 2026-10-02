# Xenia overlay design

## Context

A player holds a Steam Deck indoors and pauses play to choose a figure. Use
the existing Xenia overlay theme so the controls remain familiar over a game.

## Theme and color

Use ImGuiDrawer's inherited green accent and neutral surfaces. Do not add a
second palette, new font assets, gradients, or decorative motion. Use text
labels for Ready, Invalid, Read only, Changed, and Recovery required.

## Type and controls

Use the existing ImGui font and navigation system. Buttons and selectable
rows are at least 44 logical pixels high. Wrap filenames and error text in
details; list labels use the visible file name, with an independent item ID.

## Layout

Center the portal overlay within the current viewport. Keep the slot list and
library list scrollable, with fixed space for status and actions. Use two
columns when width permits and stacked sections on narrow views. Search and
filters precede library rows. Back and Close remain visible.

## Confirmation and feedback

Use an inline confirmation step with Cancel first for export replacement and
recovery. Keep the overlay open after failures. Exclude unsupported creation
and reset actions, and state why they are unavailable.

Sources: approved portal design, src/xenia/ui/imgui_gamepad_dialog.*, and
src/xenia/ui/imgui_drawer.cc.
