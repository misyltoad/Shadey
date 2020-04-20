#include "hooks.h"
#include "command_helpers.h"

namespace shadey {

  class PingCommand : public ShadeyCommand {
  public:
    using ShadeyCommand::ShadeyCommand;

    void onCommand(const ShadeyCommandContext& ctx) override {
      reply(ctx, "Hello " + ctx.message().author.username);
    }
  };

  SHADEY_REGISTER_HOOK(PingCommand, "ping");

}