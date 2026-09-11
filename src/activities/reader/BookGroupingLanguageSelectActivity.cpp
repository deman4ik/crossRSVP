#include "BookGroupingLanguageSelectActivity.h"

#include <I18n.h>

#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

BookGroupingLanguageSelectActivity::BookGroupingLanguageSelectActivity(
    GfxRenderer& renderer, MappedInputManager& mappedInput, std::string cachePath,
    const BookGroupingLanguagePreference::Choice currentChoice)
    : UiListActivity("BookGroupingLanguage", renderer, mappedInput),
      cachePath(std::move(cachePath)),
      currentChoice(currentChoice) {
  const char* const labels[] = {tr(STR_RSVP_GROUPING_LANGUAGE_AUTO), tr(STR_RSVP_GROUPING_LANGUAGE_RUSSIAN),
                                tr(STR_RSVP_GROUPING_LANGUAGE_ENGLISH)};
  for (int i = 0; i < 3; ++i) {
    rows[i].label = labels[i];
    rows[i].actionValue = static_cast<int16_t>(i);
    if (static_cast<uint8_t>(currentChoice) == static_cast<uint8_t>(i)) rows[i].value = tr(STR_SELECTED);
  }
  nav.selected = static_cast<int>(currentChoice);
}

const char* BookGroupingLanguageSelectActivity::headerTitle() const { return tr(STR_RSVP_GROUPING_LANGUAGE); }

void BookGroupingLanguageSelectActivity::activateIndex(const int index) {
  if (index < 0 || index >= 3) return;
  app.clearTapFlash();
  const auto choice = static_cast<BookGroupingLanguagePreference::Choice>(index);
  if (!BookGroupingLanguagePreference(cachePath).save(choice)) return;
  currentChoice = choice;
  setResult(BookGroupingLanguageResult{static_cast<uint8_t>(choice)});
  finish();
}

void BookGroupingLanguageSelectActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows;
  props.count = 3;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
