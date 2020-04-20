#include "command_helpers.h"

namespace shadey {

  namespace {
    static constexpr std::string_view g_commandPrefix = ">";

    static std::vector<std::string_view> splitStringView(std::string_view strv, std::string_view delims = " ") {
      std::vector<std::string_view> output;
      size_t first = 0;

      while (first < strv.size()) {
        const auto second = strv.find_first_of(delims, first);

        if (first != second && first != second + 1)
            output.emplace_back(strv.substr(first, second - first));

        if (second == std::string_view::npos)
            break;

        first = second + 1;
      }

      return output;
    }
  }

  ShadeyCommandContext::ShadeyCommandContext(ShadeyClient& client, SleepyDiscord::Message& message)
    : m_message(message)
    , m_client (client) { }


  std::vector<std::string_view> ShadeyCommandContext::args() const {
    return splitStringView(m_message.content, " ");
  }


  std::string_view ShadeyCommandContext::argsString() const {
    std::string_view view = m_message.content;

    const auto space = view.find_first_of(' ');

    return space == std::string_view::npos
      ? ""
      : view.substr(space + 1);
  }


  std::string_view ShadeyCommandContext::command() const {
    std::string_view view = m_message.content;

    if (!view.starts_with(g_commandPrefix))
      return "";

    view = view.substr(g_commandPrefix.length());

    const auto space = view.find_first_of(' ');

    return space == std::string_view::npos
      ? view.substr(0)
      : view.substr(0, space);
  }


  SleepyDiscord::Message& ShadeyCommandContext::message() const {
    return m_message;
  }


  ShadeyClient& ShadeyCommandContext::client() const {
    return m_client;
  }


  ShadeyCommand::ShadeyCommand(std::string_view commandName)
    : m_commandName(commandName) { }


  void ShadeyCommand::onMessage(ShadeyClient& client, SleepyDiscord::Message message) {
    const ShadeyCommandContext ctx(client, message);

    if (ctx.command() == m_commandName)
      this->onCommand(ctx);
  }

}