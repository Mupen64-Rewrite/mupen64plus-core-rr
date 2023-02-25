#ifndef AVEC_BASE_HPP_INCLUDED
#define AVEC_BASE_HPP_INCLUDED
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <stdexcept>
#include <system_error>
#include <type_traits>
extern "C" {
#include <libavutil/avutil.h>
}

namespace av {
  // ERROR HANDLING
  // ==============
  
  class av_category_t final : public std::error_category {
  public:
    inline const char* name() const noexcept override {
      return "av_category";
    }
    
    inline std::string message(int code) const noexcept override {
      thread_local static char msg_buffer[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(code, msg_buffer, sizeof(msg_buffer));
      return std::string(msg_buffer);
    }
  };
  
  inline const std::error_category& av_category() {
    static av_category_t result;
    return result;
  }
  
  inline std::system_error av_error(int code) {
    return std::system_error(code, av_category());
  }
}  // namespace av

#endif