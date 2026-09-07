#include "RsvpControlPanelUi.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_PLAY = 1;
constexpr fui::ActionId ACTION_STEP = 2;
constexpr fui::ActionId ACTION_REWIND = 3;
constexpr fui::ActionId ACTION_PACE_DOWN = 4;
constexpr fui::ActionId ACTION_PACE_UP = 5;
constexpr fui::ActionId ACTION_PAGED = 6;
constexpr fui::ActionId ACTION_SETTINGS = 7;
constexpr int kColumns = 2;
constexpr int kRows = 4;
}  // namespace

RsvpControlPanelUi::RsvpControlPanelUi(GfxRenderer& renderer) : UiAppHost(renderer) {}

void RsvpControlPanelUi::begin() {
  resetUi();
  pending_ = Event::None;
  app.on(ACTION_PLAY, &RsvpControlPanelUi::onAction, this);
  app.setScreen(&RsvpControlPanelUi::screenFn, this);
}

int RsvpControlPanelUi::reservedHeight() const {
  const auto& theme = app.theme();
  return theme.spaceMd * 2 + kRows * theme.rowHeight + (kRows - 1) * theme.spaceSm;
}

void RsvpControlPanelUi::render(const bool canPlay, const bool canStep) {
  canPlay_ = canPlay;
  canStep_ = canStep;
  renderUi();
}

RsvpControlPanelUi::Event RsvpControlPanelUi::route(const MappedInputManager& input) {
  pending_ = Event::None;
  const auto touch = routeTouch(input);
  if (!touch.routed || !touch.snap.touchReleased) return Event::None;
  return pending_;
}

void RsvpControlPanelUi::onAction(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<RsvpControlPanelUi*>(user);
  switch (event.value) {
    case ACTION_PLAY:
      self->pending_ = Event::Play;
      break;
    case ACTION_STEP:
      self->pending_ = Event::Step;
      break;
    case ACTION_REWIND:
      self->pending_ = Event::Rewind;
      break;
    case ACTION_PACE_DOWN:
      self->pending_ = Event::PaceDown;
      break;
    case ACTION_PACE_UP:
      self->pending_ = Event::PaceUp;
      break;
    case ACTION_PAGED:
      self->pending_ = Event::Paged;
      break;
    case ACTION_SETTINGS:
      self->pending_ = Event::Settings;
      break;
    default:
      break;
  }
}

void RsvpControlPanelUi::screenFn(UiScreen& screen, void* user) {
  static_cast<RsvpControlPanelUi*>(user)->buildScreen(screen);
}

void RsvpControlPanelUi::buildScreen(UiScreen& screen) {
  const auto& theme = screen.theme();
  const int rowHeight = theme.rowHeight;
  const int gap = theme.spaceSm;
  const int padding = theme.spaceMd;
  const auto device = uiTarget.deviceContext();
  const int width = device.width;
  const int height = device.height - device.safeArea.bottom;
  const int panelTop = height - reservedHeight();
  screen.target().fill(
      fui::Rect{0, static_cast<int16_t>(panelTop), static_cast<int16_t>(width), static_cast<int16_t>(reservedHeight())},
      theme.button.normal.background);

  const char* labels[kRows][kColumns] = {
      {tr(STR_RSVP_TOUCH_PLAY), tr(STR_RSVP_TOUCH_STEP)},
      {tr(STR_RSVP_TOUCH_REWIND), tr(STR_SETTINGS_TITLE)},
      {tr(STR_RSVP_TOUCH_PACE_DOWN), tr(STR_RSVP_TOUCH_PACE_UP)},
      {tr(STR_RSVP_TOUCH_PAGED), nullptr},
  };
  const fui::ActionId actions[kRows][kColumns] = {
      {ACTION_PLAY, ACTION_STEP},
      {ACTION_REWIND, ACTION_SETTINGS},
      {ACTION_PACE_DOWN, ACTION_PACE_UP},
      {ACTION_PAGED, fui::NO_ACTION},
  };
  const int safeLeft = device.safeArea.left;
  const int safeRight = device.safeArea.right;
  const int contentWidth = width - safeLeft - safeRight - 2 * padding - (kColumns - 1) * gap;
  const int cellWidth = contentWidth / kColumns;
  for (int row = 0; row < kRows; ++row) {
    for (int column = 0; column < kColumns; ++column) {
      if ((actions[row][column] == ACTION_PLAY && !canPlay_) || (actions[row][column] == ACTION_STEP && !canStep_) ||
          labels[row][column] == nullptr)
        continue;
      buttonProps_ = {};
      buttonProps_.label = labels[row][column];
      buttonProps_.action = ACTION_PLAY;
      buttonProps_.value = static_cast<int16_t>(actions[row][column]);
      buttonProps_.inputMask = fui::InputTouch;
      buttonProps_.text = theme.bodyText;
      buttonProps_.text.align = fui::TextAlign::Center;
      buttonProps_.text.maxLines = 2;
      const int x = safeLeft + padding + column * (cellWidth + gap);
      const int y = panelTop + padding + row * (rowHeight + gap);
      screen.button(buttonProps_, fui::Rect{static_cast<int16_t>(x), static_cast<int16_t>(y),
                                            static_cast<int16_t>(cellWidth), static_cast<int16_t>(rowHeight)});
#if defined(SIMULATOR)
      if (!diagnosticsLogged_) {
        LOG_INF("RSVP", "touch_panel_button row=%d col=%d x=%d y=%d w=%d h=%d action=%u", row, column, x, y, cellWidth,
                rowHeight, static_cast<unsigned>(actions[row][column]));
      }
#endif
    }
  }
#if defined(SIMULATOR)
  diagnosticsLogged_ = true;
#endif
}
