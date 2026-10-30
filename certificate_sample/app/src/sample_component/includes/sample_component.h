#pragma once

#include "component.h"
#include "i_log_manager.h"
#include "i_p_open_platform_manager.h"
#include "i_sample_component.h"
#include "open_platform_define.h"

using namespace openplatform;

class SampleComponent : public Component, public ISampleComponent {
 public:
  SampleComponent();
  SampleComponent(ClassID id, const char* name);
  virtual ~SampleComponent();

  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;
  virtual void RegisterOpenAPIURI();

 private:
  bool ParseHttpEvent(Event* event);

  bool HandleLoadDeviceCert(const std::string& to_path, std::string& result);
  bool HandleCreateDigitalSignature(const std::string& name, const std::string& data,
                                    const std::string& digest, std::string& result);

  void DebugLog(const char* format, ...) {
    char buffer[1024] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    auto* arg = new ("") Platform_Std_Refine::SerializableString(buffer);
    SendTargetEvents(ILogManager::remote_debug_message_group,
                     static_cast<int32_t>(ILogManager::EEvent::eRemoteDebugMessage), 0, arg);
    std::cout << buffer << std::endl;
  }

  inline std::string Base64Encode(const std::string& str) {
    if (!str.empty()) {
      auto e = Base64::encode(str.c_str(), str.size());
      std::string e_s(e.first.get(), e.second);
      return e_s;
    } else {
      return "";
    }
  }

 private:
  static constexpr unsigned kCertTimeoutMs { 10000 };
};
