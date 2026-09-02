#pragma once

#include <array>
#include <cstddef>

#include "RsvpTypes.h"

namespace rsvp {

// Fixed-size control buffer used by the main task while the panel is refreshing.
class RsvpPendingActions final {
 public:
  static constexpr size_t CAPACITY = 16;

  bool push(const Action action) {
    if (!isUserAction(action) || size == CAPACITY) return false;
    actions[size++] = action;
    return true;
  }

  Action pop() {
    for (const Action candidate : priority) {
      for (size_t index = 0; index < size; ++index) {
        if (actions[index] != candidate) continue;
        for (size_t move = index + 1; move < size; ++move) actions[move - 1] = actions[move];
        --size;
        return candidate;
      }
    }
    return Action::None;
  }

  bool empty() const { return size == 0; }
  size_t count() const { return size; }

 private:
  static constexpr std::array<Action, 8> priority = {
      Action::Exit,       Action::ModeSwitch,    Action::TogglePlayback, Action::WordDoesNotFit,
      Action::PaceDown,   Action::PaceUp,        Action::RewindFive,      Action::StepForward,
  };

  static constexpr bool isUserAction(const Action action) {
    return action == Action::TogglePlayback || action == Action::StepForward || action == Action::RewindFive ||
           action == Action::PaceDown || action == Action::PaceUp || action == Action::ModeSwitch ||
           action == Action::Exit || action == Action::WordDoesNotFit;
  }

  std::array<Action, CAPACITY> actions{};
  size_t size = 0;
};

}  // namespace rsvp
