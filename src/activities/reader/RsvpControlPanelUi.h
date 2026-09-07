#pragma once

#include <cstdint>

#include "components/UiAppHost.h"

class GfxRenderer;
class MappedInputManager;

class RsvpControlPanelUi final : public UiAppHost {
 public:
  enum class Event : uint8_t { None, Play, Step, Rewind, PaceDown, PaceUp, Paged, Settings };

  explicit RsvpControlPanelUi(GfxRenderer& renderer);

  void begin();
  void render(bool canPlay, bool canStep);
  Event route(const MappedInputManager& input);
  int reservedHeight() const;

 private:
  static void screenFn(UiScreen& screen, void* user);
  static void onAction(const freeink::ui::ActionEvent& event, void* user);
  void buildScreen(UiScreen& screen);

  Event pending_ = Event::None;
  bool canPlay_ = false;
  bool canStep_ = false;
  bool diagnosticsLogged_ = false;
  freeink::ui::ButtonProps buttonProps_;
};
