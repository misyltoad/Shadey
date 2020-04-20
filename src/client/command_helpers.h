#pragma once

#include <vector>
#include <algorithm>
#include <string_view>

#include "sleepy_discord/message.h"
#include "hooks.h"

namespace shadey {

  class ShadeyClient;

  class ShadeyCommandContext {

  public:

    ShadeyCommandContext(ShadeyClient& client, SleepyDiscord::Message& message);

    std::vector<std::string_view> args() const;

    std::string_view argsString() const;

    std::string_view command() const;

    SleepyDiscord::Message& message() const;

    ShadeyClient& client() const;

  private:

    SleepyDiscord::Message& m_message;

    ShadeyClient& m_client;
  };

  class ShadeyCommand : public ShadeyHook {
  public:
    ShadeyCommand(std::string_view commandName);

    virtual void onCommand(const ShadeyCommandContext& ctx) = 0;

    void onMessage(ShadeyClient& client, SleepyDiscord::Message message) final;

  private:

    std::string_view m_commandName;
  };

  inline bool contains(const std::string& str, std::string_view substr) {
    return str.find(substr) != std::string::npos;
  }

  inline bool validateMessage(const std::string& message) {
    if (message.empty())
      return false;

    if (contains(message, "@here") ||
        contains(message, "@everyone"))
      return false;

    return true;
  }

  inline void reply(const ShadeyCommandContext& ctx, const std::string& message) {
    if (!validateMessage(message)) {
      ctx.client().sendMessage(ctx.message().channelID, "Silly boy, you can't do that!");
      return;
    }

    ctx.client().sendMessage(ctx.message().channelID, message);
  }

}