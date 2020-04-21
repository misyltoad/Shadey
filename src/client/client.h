#pragma once

#include "sleepy_discord/websocketpp_websocket.h"

namespace shadey {

  class ShadeyClient : public SleepyDiscord::DiscordClient {
  public:
    using SleepyDiscord::DiscordClient::DiscordClient;

    void onReady(std::string* jsonMessage) override;
    void onMessage(SleepyDiscord::Message message) override;
  };

}