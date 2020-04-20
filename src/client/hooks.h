#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "client.h"
#include "non_copyable.h"

namespace shadey {

  class ShadeyHook;

  class ShadeyGlobalHookList : public NonCopyable {
  public:
    void install(std::unique_ptr<ShadeyHook>&& ptr);

    static ShadeyGlobalHookList* instance();

    const std::vector<std::unique_ptr<ShadeyHook>>& hooks() const;

  private:
    std::vector<std::unique_ptr<ShadeyHook>> m_hooks;
  };

  class ShadeyHook : public NonCopyable {
  public:
    virtual void onMessage(ShadeyClient& client, SleepyDiscord::Message message) { }
  };

  template <typename T>
  class ShadeyHookInstaller : public NonCopyable {
  public:
    ShadeyHookInstaller(std::unique_ptr<T>&& ptr, std::string_view name) {
      std::cout << "Installing hook: " << name << std::endl;
      ShadeyGlobalHookList::instance()->install(std::move(ptr));
    }
  };

#define SHADEY_REGISTER_HOOK(x, ...) \
  static ::shadey::ShadeyHookInstaller<x> g_hookInstaller_##x( std::make_unique<x>( __VA_ARGS__ ), #x);

}