#include "hooks.h"
#include "command_helpers.h"
#include "string_helpers.h"

#include "tinyxml2.h"

namespace shadey {

  static const tinyxml2::XMLDocument& GetVulkanXML() {
    static tinyxml2::XMLDocument* doc = nullptr;

    if (doc == nullptr) {
      doc = new tinyxml2::XMLDocument();
      doc->LoadFile("vk.xml");
    }

    return *doc;
  }

  const tinyxml2::XMLElement* GetFirstVulkanTypeElement() {
    const tinyxml2::XMLDocument& doc = GetVulkanXML();

    auto registry = doc.FirstChildElement("registry");
    if (!registry)
      return nullptr;

    auto types = registry->FirstChildElement("types");
    if (!types)
      return nullptr;

    return types->FirstChildElement("type");
  }

  bool StringContains(const char* s1, const char* s2) {
    if (s1 == s2)
      return true;

    if (s1 == nullptr || s2 == nullptr)
      return false;

    return strstr(s1, s2);
  }

  const char* PreviousText(const tinyxml2::XMLNode* node) {
    if (node == nullptr)
      return nullptr;

    node = node->PreviousSibling();

    if (node == nullptr)
      return nullptr;

    auto text = node->ToText();

    return text == nullptr
      ? nullptr
      : text->Value();
  }

  void WriteVulkanStruct(const tinyxml2::XMLElement* type, std::stringstream& stream) {
    const char* name = type->Attribute("name");

    stream << "```cpp\n";
    stream << "struct " << name << " {" << "\n";

    // Loop through members to find max lengths
    size_t maxTypeLength = 0;
    size_t maxNameLength = 0;
    {
      auto member = type->FirstChildElement("member");
      while (member != nullptr) {
        auto memberType = member->FirstChildElement("type");
        auto memberName = member->FirstChildElement("name");

        size_t extraSize = 0;
        if (StringContains(PreviousText(memberType), "const")) extraSize += 6;
        if (StringContains(PreviousText(memberName), "*"))     extraSize += 1;

        if (memberType != nullptr && memberName != nullptr) {
          maxTypeLength = std::max(maxTypeLength, strlen(memberType->GetText()) + extraSize);
          maxNameLength = std::max(maxNameLength, strlen(memberName->GetText()));
        }

        member = member->NextSiblingElement("member");
      }
    }

    // Loop through again to output.
    {
      auto member = type->FirstChildElement("member");
      while (member != nullptr) {
        auto memberType   = member->FirstChildElement("type");
        auto memberName   = member->FirstChildElement("name");
        auto memberValues = member->Attribute("values");

        if (memberType != nullptr && memberName != nullptr) {
          if (memberValues != nullptr)
            stream << "  // Values: " << memberValues << "\n";

          stream << "  ";
          if (StringContains(PreviousText(memberType), "const")) stream << "const ";
          stream << memberType->GetText();
          if (StringContains(PreviousText(memberName), "*"))     stream << "*";
          stream << " ";

          size_t extraSize = 0;
          if (StringContains(PreviousText(memberType), "const")) extraSize += 6;
          if (StringContains(PreviousText(memberName), "*"))     extraSize += 1;

          size_t spaces = maxTypeLength - (strlen(memberType->GetText()) + extraSize);
          for (size_t i = 0; i < spaces; i++)
            stream << " ";

          stream << "  " << memberName->GetText() << ";";

          auto comment = member->FirstChildElement("comment");
          if (comment != nullptr) {
            spaces = maxNameLength - strlen(memberName->GetText());
            for (size_t i = 0; i < spaces; i++)
              stream << " ";

            stream << " // " << comment->GetText();
          }

          stream << "\n";
        }

        member = member->NextSiblingElement("member");
      }
    }

    stream << "};\n";
    stream << "```";
  }

  bool LookupVulkanType(std::string lookupStruct, std::stringstream& stream) {
    auto type = GetFirstVulkanTypeElement();
    while (type != nullptr) {
      const char* category = type->Attribute("category");
      const char* name     = type->Attribute("name");

      if (category != nullptr && name != nullptr) {
        if (!strcmp(name, lookupStruct.c_str())) {
          WriteVulkanStruct(type, stream);

          stream << "https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/" << lookupStruct << ".html";

          return true;
        }
      }

      type = type->NextSiblingElement("type");
    }

    return false;
  }

  class VulkanTypeCommand : public ShadeyCommand {
  public:
    using ShadeyCommand::ShadeyCommand;

    void onCommand(const ShadeyCommandContext& ctx) override {
      auto name = std::string(ctx.argsString());
      trim(name);

      std::stringstream stream;
      if (!LookupVulkanType(name, stream)) {
        ctx.client().sendMessage(ctx.message().channelID, "Couldn't find " + name);
        return;
      }

      std::string str = stream.str();
      if (str.length() > 1500)
        throw std::runtime_error("Too big, sorry!");

      reply(ctx, str);
    }
  };

  SHADEY_REGISTER_HOOK(VulkanTypeCommand, "vktype");

}