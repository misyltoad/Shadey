#pragma once

namespace shadey {

  class NonCopyable {
  protected:
    NonCopyable() {}

  private:
    NonCopyable           (const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
  };

}