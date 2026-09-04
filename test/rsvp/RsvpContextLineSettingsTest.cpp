#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>

#include "I18n.h"
#include "RsvpSettings.h"

namespace {

constexpr char kEnglishLabel[] = "RSVP Context Line";
constexpr char kRussianLabel[] = "Строка контекста RSVP";

TEST(RsvpContextLineSettings, MissingValueKeepsContextLineOff) {
  JsonDocument document;
  EXPECT_EQ(rsvp::CONTEXT_LINE_DEFAULT, 0);
  EXPECT_EQ(rsvp::loadContextLineSetting(document.as<JsonVariantConst>()), 0);
}

TEST(RsvpContextLineSettings, RoundTripsEnabledAndDisabledValues) {
  for (const uint8_t value : {uint8_t{0}, uint8_t{1}}) {
    JsonDocument document;
    rsvp::saveContextLineSetting(document, value);
    EXPECT_EQ(rsvp::loadContextLineSetting(document.as<JsonVariantConst>()), value);
  }
}

TEST(RsvpContextLineSettings, AcceptsBooleanAndRejectsInvalidValues) {
  JsonDocument document;
  document[rsvp::CONTEXT_LINE_SETTING_KEY] = true;
  EXPECT_EQ(rsvp::loadContextLineSetting(document.as<JsonVariantConst>()), 1);
  document[rsvp::CONTEXT_LINE_SETTING_KEY] = 2;
  EXPECT_EQ(rsvp::loadContextLineSetting(document.as<JsonVariantConst>()), 0);
  document[rsvp::CONTEXT_LINE_SETTING_KEY] = "true";
  EXPECT_EQ(rsvp::loadContextLineSetting(document.as<JsonVariantConst>()), 0);
}

TEST(RsvpContextLineSettings, RegistersPersistedReaderToggle) {
  std::ifstream source(RSVP_SETTINGS_LIST_SOURCE);
  ASSERT_TRUE(source.is_open());
  const std::string contents((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
  EXPECT_NE(contents.find("SettingInfo::Toggle(StrId::STR_RSVP_CONTEXT_LINE"), std::string::npos);
  EXPECT_NE(contents.find("&CrossPointSettings::rsvpContextLine"), std::string::npos);
  EXPECT_NE(contents.find("rsvp::CONTEXT_LINE_SETTING_KEY"), std::string::npos);
}

TEST(RsvpContextLineSettings, LabelIsTranslatedInEnglishAndRussian) {
  I18N.setLanguage(Language::EN);
  EXPECT_STREQ(I18N.get(StrId::STR_RSVP_CONTEXT_LINE), kEnglishLabel);

  I18N.setLanguage(Language::RU);
  EXPECT_STREQ(I18N.get(StrId::STR_RSVP_CONTEXT_LINE), kRussianLabel);
}

}  // namespace
