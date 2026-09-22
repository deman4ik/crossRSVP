#include "RsvpWindowDiagnosticUi.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_ACTIVATE = 1;
}

RsvpWindowDiagnosticUi::RsvpWindowDiagnosticUi(GfxRenderer& renderer) : UiAppHost(renderer) {}

void RsvpWindowDiagnosticUi::begin() {
  resetUi();
  activated_ = false;
  app.on(ACTION_ACTIVATE, &RsvpWindowDiagnosticUi::onAction, this);
  app.setScreen(&RsvpWindowDiagnosticUi::screenFn, this);
}

void RsvpWindowDiagnosticUi::render(const char* label) {
  label_ = label ? label : "";
  renderUi();
}

bool RsvpWindowDiagnosticUi::route(const MappedInputManager& input) {
  activated_ = false;
  const auto touch = routeTouch(input);
  return touch.routed && touch.snap.touchReleased && activated_;
}

int RsvpWindowDiagnosticUi::reservedHeight() const {
  const auto& theme = app.theme();
  return theme.rowHeight + theme.spaceMd * 2;
}

void RsvpWindowDiagnosticUi::screenFn(UiScreen& screen, void* user) {
  static_cast<RsvpWindowDiagnosticUi*>(user)->buildScreen(screen);
}

void RsvpWindowDiagnosticUi::onAction(const fui::ActionEvent&, void* user) {
  static_cast<RsvpWindowDiagnosticUi*>(user)->activated_ = true;
}

void RsvpWindowDiagnosticUi::buildScreen(UiScreen& screen) {
  const auto& theme = screen.theme();
  const auto device = uiTarget.deviceContext();
  const int height = device.height - device.safeArea.bottom;
  const int top = height - reservedHeight();
  const int padding = theme.spaceMd;
  const int left = device.safeArea.left + padding;
  const int width = device.width - device.safeArea.left - device.safeArea.right - padding * 2;

  screen.target().fill(fui::Rect{0, static_cast<int16_t>(top), static_cast<int16_t>(device.width),
                                 static_cast<int16_t>(reservedHeight())},
                       theme.button.normal.background);
  buttonProps_ = {};
  buttonProps_.label = label_;
  buttonProps_.action = ACTION_ACTIVATE;
  buttonProps_.inputMask = fui::InputTouch;
  buttonProps_.text = theme.bodyText;
  buttonProps_.text.align = fui::TextAlign::Center;
  buttonProps_.text.maxLines = 1;
  screen.button(buttonProps_, fui::Rect{static_cast<int16_t>(left), static_cast<int16_t>(top + padding),
                                        static_cast<int16_t>(width), static_cast<int16_t>(theme.rowHeight)});
}
