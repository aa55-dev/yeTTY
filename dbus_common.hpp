#ifndef DBUS_COMMON_HPP
#define DBUS_COMMON_HPP

inline static constexpr const char* DBUS_SERVICE_NAME = "dev.aa55.yetty";
inline static constexpr const char* DBUS_SERVICE_REGEX = R"(^dev\.aa55\.yetty.id-[0-9]+$)";

inline static constexpr const char* DBUS_INTERFACE_NAME = "dev.aa55.yetty.control";

inline static constexpr const char* DBUS_START = "start";
inline static constexpr const char* DBUS_STOP = "stop";

inline static constexpr const char* DBUS_RESULT_SUCCESS = "success";
#endif // DBUS_COMMON_HPP
