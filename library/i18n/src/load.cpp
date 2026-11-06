// Messages from JSON (parseMessages, loadMessages): a file of its own, so an
// app that never calls them doesn't link rx::fetch and the JSON parser.
#include <string>
#include <string_view>
#include <utility>

#include "reactx/fetch.hpp"
#include "reactx/json.hpp"
#include "rx/i18n.hpp"

namespace rx::i18n {

std::expected<Messages, std::string> parseMessages(std::string_view text) {
  auto parsed = json::tryParse(text);
  if (!parsed)
    return std::unexpected("invalid JSON: " +
                           std::string(parsed.error().what()));
  if (!parsed->isObject())
    return std::unexpected(std::string("the messages must be a JSON object"));
  Messages out;
  std::string error;
  auto walk = [&](auto &self, const json::Object &object,
                  const std::string &prefix) -> void {
    for (const auto &[name, value] : object) {
      const std::string key = prefix.empty() ? name : prefix + "." + name;
      if (value.isObject())
        self(self, value.asObject(), key);
      else if (value.isString())
        out.insert_or_assign(key, value.asString());
      else if (error.empty())
        error = "\"" + key + "\" is not a string";
    }
  };
  walk(walk, parsed->asObject(), "");
  if (!error.empty())
    return std::unexpected(error);
  return out;
}

FetchHandle loadMessages(std::string lang, std::string url,
                         std::function<void(const std::string &)> done) {
  return fetch(
      {.url = std::move(url)},
      [lang = std::move(lang), done = std::move(done)](const FetchResponse &r) {
        std::string error;
        if (!r.ok()) {
          error = r.errorText();
        } else if (auto messages = parseMessages(r.body)) {
          addMessages(lang, std::move(*messages));
        } else {
          error = messages.error();
        }
        if (done)
          done(error);
      });
}

}  // namespace rx::i18n
