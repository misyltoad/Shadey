#pragma once

namespace shadey {

  inline std::string& replace(std::string& subject, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
      subject.replace(pos, search.length(), replace);
      pos += replace.length();
    }

    return subject;
  }

  inline const char* ws = " \t\n\r\f\v";

  inline std::string& rtrim(std::string& s, const char* t = ws) {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
  }

  inline std::string& ltrim(std::string& s, const char* t = ws) {
    s.erase(0, s.find_first_not_of(t));
    return s;
  }

  // trim from both ends of string (right then left)
  inline std::string& trim(std::string& s, const char* t = ws) {
    return ltrim(rtrim(s, t), t);
  }

}