#pragma once

#include "sleepy_discord/websocketpp_websocket.h"

namespace shadey {

  class ShadeyClient : public SleepyDiscord::DiscordClient {
  public:
    using SleepyDiscord::DiscordClient::DiscordClient;

    void onReady(SleepyDiscord::Ready ready) override;
    void onMessage(SleepyDiscord::Message message) override;

  private:
    SleepyDiscord::User m_self;
  };

}