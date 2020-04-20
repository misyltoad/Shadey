#include "hooks.h"

namespace shadey {

  void ShadeyGlobalHookList::install(std::unique_ptr<ShadeyHook>&& ptr) {
    m_hooks.push_back(std::move(ptr));
  }

  const std::vector<std::unique_ptr<ShadeyHook>>& ShadeyGlobalHookList::hooks() const {
    return m_hooks;
  }

  ShadeyGlobalHookList* ShadeyGlobalHookList::instance() {
    static std::unique_ptr<ShadeyGlobalHookList> s_instance =
      std::make_unique<ShadeyGlobalHookList>();

    return s_instance.get();
  }

}