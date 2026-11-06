// languagePicker: a file of its own, so an app without it doesn't link rx::ui.
#include <string>
#include <utility>
#include <vector>

#include "reactx/ui.hpp"
#include "rx/i18n.hpp"

namespace rx::i18n {

namespace {

Element LanguagePicker(const LanguagePickerProps &p) {
  const std::string current = useLanguage();
  std::vector<ui::Option> options;
  for (auto &l : languages())
    options.push_back({.value = l.code, .label = l.name});
  return ui::select(
      {.label = p.label,
       .value = current,
       .options = std::move(options),
       .onChange = [](const std::string &code) { setLanguage(code); },
       .labelAbove = p.labelAbove});
}

} // namespace

Element languagePicker(LanguagePickerProps props) { return h(LanguagePicker, std::move(props)); }

}  // namespace rx::i18n
