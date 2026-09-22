#pragma once

#include "components/UiAppHost.h"

class GfxRenderer;
class MappedInputManager;

// A single large touch target for the hardware diagnostic. It stays outside
// the measured RSVP line so touch support does not change the word geometry.
class RsvpWindowDiagnosticUi final : public UiAppHost {
 public:
  explicit RsvpWindowDiagnosticUi(GfxRenderer& renderer);

  void begin();
  void render(const char* label);
  bool route(const MappedInputManager& input);
  int reservedHeight() const;

 private:
  static void screenFn(UiScreen& screen, void* user);
  static void onAction(const freeink::ui::ActionEvent& event, void* user);
  void buildScreen(UiScreen& screen);

  const char* label_ = "";
  bool activated_ = false;
  freeink::ui::ButtonProps buttonProps_;
};
