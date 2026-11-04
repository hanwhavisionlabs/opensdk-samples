// Copyright (c) 2026 Hanwha Vision Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "component.h"
#include "typedef_base.h"
#include "i_webservice_component.h"
#include "i_log_manager.h"
#include "i_netsvc_httpd_service.h"
#include "i_app_dispatcher.h"
#include "open_platform_define.h"
#include "i_p_open_platform_manager.h"
#include <filesystem>

class WebServiceComponent : public Component, public IWebServiceComponent {
 public:
  WebServiceComponent();
  WebServiceComponent(ClassID id, const char* name);
  virtual ~WebServiceComponent();

  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;

  virtual void RegisterOpenAPIURI();

 private:
  bool ParseHttpEvent(Event* event);
  bool ParseManifest(const std::string& manifest_path, ManifestInfo& info);
  std::string requestHttpService();
  std::string MakeBase64TestString();
  std::string base64_encode(const char* in, size_t n);
  void DebugLog(const char* format, ...) {
    char buffer[1024] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    auto* arg = new ("") Platform_Std_Refine::SerializableString(buffer);
    SendTargetEvents(ILogManager::remote_debug_message_group, static_cast<int32_t>(ILogManager::EEvent::eRemoteDebugMessage), 0, arg);
    std::cout << buffer << std::endl;
  }

 private:
  ManifestInfo manifest_;
};
