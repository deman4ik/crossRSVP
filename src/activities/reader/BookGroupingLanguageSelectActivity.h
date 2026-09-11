#pragma once

#include <Epub/BookGroupingLanguagePreference.h>

#include "activities/UiListActivity.h"

class BookGroupingLanguageSelectActivity final : public UiListActivity {
 public:
  BookGroupingLanguageSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string cachePath,
                                     BookGroupingLanguagePreference::Choice currentChoice);

 private:
  int listCount() const override { return 3; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::string cachePath;
  BookGroupingLanguagePreference::Choice currentChoice;
  freeink::ui::ListItem rows[3]{};
};
