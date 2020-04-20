#include "hooks.h"
#include "command_helpers.h"

#include "renderer.h"
#include "string_helpers.h"

namespace shadey {

  class ShaderCodeHook : public ShadeyHook {
  public:
    using ShadeyHook::ShadeyHook;

    void onMessage(ShadeyClient& client, SleepyDiscord::Message message) final {
      // Extract the code from the message.
      std::string code;
      {
        std::string content = message.content;

        size_t codeStart = content.find("```glsl");
        if (codeStart == std::string::npos)
          return;

        content = content.substr(codeStart + strlen("```glsl"));

        size_t codeEnd = content.find("```", codeStart);

        if (codeEnd == std::string::npos)
          return;

        code = content.substr(0, codeEnd);

        if (code.empty())
          return;

        replace(code, "\\n", "\n");
      }

      Renderer renderer;
      std::string filename = renderer.init(code);

      client.uploadFile(message.channelID, filename, "");
    }
  };

  SHADEY_REGISTER_HOOK(ShaderCodeHook);

}