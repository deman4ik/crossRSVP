#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "RsvpSettings.h"

namespace {

TEST(RsvpShortWordGroupingSettings, MissingValueKeepsGroupingOff) {
  JsonDocument document;
  EXPECT_EQ(rsvp::SHORT_WORD_GROUPING_DEFAULT, 0);
  EXPECT_EQ(rsvp::loadShortWordGroupingSetting(document.as<JsonVariantConst>()), 0);
}

TEST(RsvpShortWordGroupingSettings, RoundTripsEnabledAndDisabledValues) {
  for (const uint8_t value : {uint8_t{0}, uint8_t{1}}) {
    JsonDocument document;
    rsvp::saveShortWordGroupingSetting(document, value);
    EXPECT_EQ(rsvp::loadShortWordGroupingSetting(document.as<JsonVariantConst>()), value);
    EXPECT_TRUE(document[rsvp::SHORT_WORD_GROUPING_SETTING_KEY].is<uint8_t>());
    EXPECT_TRUE(document["rsvpContextLine"].isNull());
  }
}

TEST(RsvpShortWordGroupingSettings, AcceptsBooleanAndRejectsInvalidValues) {
  JsonDocument document;
  document[rsvp::SHORT_WORD_GROUPING_SETTING_KEY] = true;
  EXPECT_EQ(rsvp::loadShortWordGroupingSetting(document.as<JsonVariantConst>()), 1);
  document[rsvp::SHORT_WORD_GROUPING_SETTING_KEY] = 2;
  EXPECT_EQ(rsvp::loadShortWordGroupingSetting(document.as<JsonVariantConst>()), 0);
  document[rsvp::SHORT_WORD_GROUPING_SETTING_KEY] = "true";
  EXPECT_EQ(rsvp::loadShortWordGroupingSetting(document.as<JsonVariantConst>()), 0);
}

TEST(RsvpShortWordGroupingSettings, LegacyContextLineDoesNotMigrate) {
  JsonDocument document;
  document["rsvpContextLine"] = true;
  EXPECT_EQ(rsvp::loadShortWordGroupingSetting(document.as<JsonVariantConst>()), 0);
}

TEST(RsvpShortWordGroupingSettings, RegistersPersistedReaderToggle) {
  std::ifstream source(RSVP_SETTINGS_LIST_SOURCE);
  ASSERT_TRUE(source.is_open());
  const std::string contents((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
  EXPECT_NE(contents.find("SettingInfo::Toggle(StrId::STR_RSVP_SHORT_WORD_GROUPING"), std::string::npos);
  EXPECT_NE(contents.find("&CrossPointSettings::rsvpShortWordGroupingEnabled"), std::string::npos);
  EXPECT_NE(contents.find("rsvp::SHORT_WORD_GROUPING_SETTING_KEY"), std::string::npos);
}

TEST(RsvpShortWordGroupingSettings, FocusReadingRemainsFlatAndNested) {
  const auto sourceRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
  const auto read = [](const std::filesystem::path& path) {
    std::ifstream source(path);
    return std::string((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
  };
  const std::string settings = read(sourceRoot / "src" / "SettingsList.h");
  const std::string settingsActivity = read(sourceRoot / "src" / "activities" / "settings" / "SettingsActivity.cpp");
  const std::string textSettings = read(sourceRoot / "src" / "activities" / "settings" / "TextSettingsActivity.cpp");
  const auto focus = settings.find("SettingInfo::Toggle(StrId::STR_FOCUS_READING");
  ASSERT_NE(focus, std::string::npos);
  const auto nextSetting = settings.find("SettingInfo::Toggle(StrId::STR_HYPHENATION", focus);
  ASSERT_NE(nextSetting, std::string::npos);
  EXPECT_EQ(settings.substr(focus, nextSetting - focus).find("withTextSettings"), std::string::npos);
  EXPECT_NE(settingsActivity.find("if (setting.inTextSettings) continue;"), std::string::npos);
  EXPECT_NE(textSettings.find("StrId::STR_FOCUS_READING"), std::string::npos);
}

TEST(RsvpShortWordGroupingSettings, LabelIsTranslatedInEnglishAndRussian) {
  const auto translation = [](const std::filesystem::path& path) {
    std::ifstream source(path);
    return std::string((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
  };
  const auto translations =
      std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "lib" / "I18n" / "translations";
  const std::string english = translation(translations / "english.yaml");
  const std::string russian = translation(translations / "russian.yaml");
  EXPECT_NE(english.find("STR_RSVP_SHORT_WORD_GROUPING: \"Short-word grouping\""), std::string::npos);
  EXPECT_NE(russian.find("STR_RSVP_SHORT_WORD_GROUPING: \"Группировка коротких слов\""), std::string::npos);
}

}  // namespace
