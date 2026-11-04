// Copyright (c) 2026 Hanwha Vision Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "sample_component.h"

#include <dispatcher_serialize.h>
#include <i_app_dispatcher.h>
#include <i_p_open_platform_manager.h>
#include <open_platform_define.h>

#include <map>

namespace {
auto eventToArgumentBuffer = [](Event* event) {
  auto blob = event->GetBlobArgument();
  std::pair<std::variant<BaseObject*, char*>, uint64_t> ret((char*)blob.GetRawData(),  // variant
                                                            blob.GetSize());           // size
  return ret;
};
}

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name), exclusive_mode_(), sd_card_app_path_() {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
    case static_cast<int32_t>(IAppDispatcher::EEventType::eHttpRequest): {
      if (event->IsReply()) {
        auto ret = eventToArgumentBuffer(event);
        SerializableString param;
        param.DeserializeBaseObject(&param, ret);
        if (event->GetReplyType() == (int)IPOpenPlatformManager::EAppEventType::eStartSDCard) {
          DebugLog("App Path on the SDCard : %s", param.GetString().c_str());
          sd_card_app_path_ = param.GetString().c_str();
        } else if (event->GetReplyType() == (int)IPOpenPlatformManager::EAppEventType::eStopSDCard) {
          DebugLog("SDCard stopped");
          stop_result_ = param.GetString();
        } else if (event->GetReplyType() == (int)IPOpenPlatformManager::EAppEventType::eFormatSDCard) {
          DebugLog("SDCard formatted");
          format_result_ = param.GetString();
        } else if (event->GetReplyType() == (int)IPOpenPlatformManager::EAppEventType::eGetSDCardStatus) {
          DebugLog("SD Card Status");
          ParseSDCardStatus(param.GetString());
        }
      } else {
        ParseHttpEvent(event);
      }
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eGetExclusiveModeStatusDone): {
      SetExclusiveModeStatus(event);
      break;
    }
    default:
      Component::ProcessAEvent(event);
      break;
  }
  return true;
}

void SampleComponent::RegisterOpenAPIURI() {
  auto get_puts = Vector<String>{};
  get_puts.push_back("GET");
  get_puts.push_back("POST");

  auto get_status_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/getsdstatus"), GetInstanceName(), get_puts);
  auto get_status_result_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/getsdstatusresult"), GetInstanceName(), get_puts);
  auto start_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/startsdcard"), GetInstanceName(), get_puts);
  auto sdcard_path_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/getsdcardpath"), GetInstanceName(), get_puts);
  auto stop_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/stopsdcard"), GetInstanceName(), get_puts);
  auto stop_result_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/stopresult"), GetInstanceName(), get_puts);
  auto format_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/formatsdcard"), GetInstanceName(), get_puts);
  auto format_result_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/formatresult"), GetInstanceName(), get_puts);
  auto set_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/setexclusivemode"), GetInstanceName(), get_puts);
  auto get_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/getexclusivemodestatus"), GetInstanceName(), get_puts);
  auto get_result_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/getexclusivemoderesult"), GetInstanceName(), get_puts);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, get_status_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, get_status_result_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, start_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, sdcard_path_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, stop_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, stop_result_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, format_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, format_result_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, set_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, get_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, get_result_request);
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  enum class SDCardAPI {
    GetSDCardStatus,
    GetSDCardStatusResult,
    StartSDCard,
    GetSDCardPath,
    StopSDCard,
    StopResult,
    FormatSDCard,
    FormatResult,
    SetExclusiveMode,
    GetExclusiveModeStatus,
    GetExclusiveModeResult
  };
  const std::map<std::string, SDCardAPI> SDCardAPIMap = {
      {"/getsdstatus", SDCardAPI::GetSDCardStatus},
      {"/getsdstatusresult", SDCardAPI::GetSDCardStatusResult},
      {"/startsdcard", SDCardAPI::StartSDCard},
      {"/getsdcardpath", SDCardAPI::GetSDCardPath},
      {"/stopsdcard", SDCardAPI::StopSDCard},
      {"/stopresult", SDCardAPI::StopResult},
      {"/formatsdcard", SDCardAPI::FormatSDCard},
      {"/formatresult", SDCardAPI::FormatResult},
      {"/setexclusivemode", SDCardAPI::SetExclusiveMode},
      {"/getexclusivemodestatus", SDCardAPI::GetExclusiveModeStatus},
      {"/getexclusivemoderesult", SDCardAPI::GetExclusiveModeResult},
  };

  auto request_api = SDCardAPIMap.at(path_info);
  auto body = oas->GetRequestBody();
  DebugLog("[APP][CHECK] Requested SDCard API: %d", (int)request_api);
  switch (request_api) {
  case SDCardAPI::GetSDCardStatus: {
    HandleGetSDCardStatus();
    break;
  }
  case SDCardAPI::GetSDCardStatusResult: {
    HandleGetSDCardStatusResult(event);
    break;
  }
    case SDCardAPI::StartSDCard: {
      HandleStartSDCard(body);
      break;
    }
    case SDCardAPI::GetSDCardPath: {
      HandleGetSDCardPath(event);
      break;
    }
    case SDCardAPI::StopSDCard: {
      HandleStopSDCard(body);
      break;
    }
    case SDCardAPI::StopResult: {
      HandleStopResult(event);
      break;
    }
    case SDCardAPI::FormatSDCard: {
      HandleFormatSDCard(body);
      break;
    }
    case SDCardAPI::FormatResult: {
      HandleFormatResult(event);
      break;
    }
    case SDCardAPI::SetExclusiveMode: {
      HandleSetExclusiveMode(body);
      break;
    }
    case SDCardAPI::GetExclusiveModeStatus: {
      HandleGetExclusiveModeStatus(body);
      break;
    }
    case SDCardAPI::GetExclusiveModeResult: {
      HandleGetExclusiveModeResult(event);
      break;
    }
    default:
      break;
  }

  return true;
}

bool SampleComponent::HandleGetSDCardStatus() {
  SendReplyEvent("OpenPlatform", (int)IPOpenPlatformManager::EAppEventType::eGetSDCardStatus);

  return true;
}

bool SampleComponent::HandleGetSDCardStatusResult(Event *event) {
  std::map<bool, std::string> BoolToString = {{true, "True"}, {false, "False"}};

  auto oas = static_cast<OpenAppSerializable *>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  JsonUtility::JsonDocument doc(JsonUtility::Type::kObjectType);
  auto &alloc = doc.GetAllocator();

  auto array = JsonUtility::ValueType(JsonUtility::Type::kArrayType);
  for (auto &s : slot_status_) {
    auto s_obj = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
    s_obj.AddMember("slot_index", s.slot_index, alloc);
    s_obj.AddMember("is_present", BoolToString[s.is_present], alloc);
    s_obj.AddMember("is_mounted", BoolToString[s.is_mounted], alloc);

    array.PushBack(s_obj, alloc);
  }
  doc.AddMember("slots", array, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  doc.Accept(writer);

  std::cout << strbuf.GetString() << std::endl;

  oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());

  return true;
}

bool SampleComponent::HandleStartSDCard(const std::string& request) {
  JsonUtility::JsonDocument doc;
  doc.Parse(request);

  if (doc.HasMember("app_id") && doc.HasMember("sdcard")) {
    JsonUtility::JsonDocument req(JsonUtility::Type::kObjectType);
    auto& alloc = req.GetAllocator();
    JsonUtility::set(req, "SDCard", std::atoi(doc["sdcard"].GetString()), alloc);

    rapidjson::StringBuffer strbuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    req.Accept(writer);

    auto* param = new ("AppMsgParam") openplatform::AppMsgParam(doc["app_id"].GetString(), strbuf.GetString());
    SendReplyEvent("OpenPlatform", (int)IPOpenPlatformManager::EAppEventType::eStartSDCard, 0, param);

    return true;
  }
  return false;
}

bool SampleComponent::HandleStopSDCard(const std::string& request) {
  JsonUtility::JsonDocument doc;
  doc.Parse(request);

  if (doc.HasMember("app_id") && doc.HasMember("sdcard")) {
    JsonUtility::JsonDocument req(JsonUtility::Type::kObjectType);
    auto& alloc = req.GetAllocator();
    JsonUtility::set(req, "SDCard", std::atoi(doc["sdcard"].GetString()), alloc);

    rapidjson::StringBuffer strbuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    req.Accept(writer);

    auto* param = new ("AppMsgParam") openplatform::AppMsgParam(doc["app_id"].GetString(), strbuf.GetString());
    SendReplyEvent("OpenPlatform", (int)IPOpenPlatformManager::EAppEventType::eStopSDCard, 0, param);

    return true;
  }
  return false;
}

bool SampleComponent::HandleFormatSDCard(const std::string& request) {
  JsonUtility::JsonDocument doc;
  doc.Parse(request);

  if (doc.HasMember("app_id") && doc.HasMember("sdcard")) {
    JsonUtility::JsonDocument req(JsonUtility::Type::kObjectType);
    auto& alloc = req.GetAllocator();
    JsonUtility::set(req, "SDCard", std::atoi(doc["sdcard"].GetString()), alloc);

    rapidjson::StringBuffer strbuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    req.Accept(writer);

    auto* param = new ("AppMsgParam") openplatform::AppMsgParam(doc["app_id"].GetString(), strbuf.GetString());
    SendReplyEvent("OpenPlatform", (int)IPOpenPlatformManager::EAppEventType::eFormatSDCard, 0, param);

    return true;
  }
  return false;
}

bool SampleComponent::HandleSetExclusiveMode(const std::string& request) {
  std::map<std::string, bool> StringToBool = {{"True", true}, {"False", false}};
  JsonUtility::JsonDocument doc;
  doc.Parse(request);

  if (doc.HasMember("app_id") && doc.HasMember("exclusive_mode")) {
    JsonUtility::JsonDocument req(JsonUtility::Type::kObjectType);
    auto& alloc = req.GetAllocator();
    JsonUtility::set(req, "use", StringToBool[doc["exclusive_mode"].GetString()], alloc);

    rapidjson::StringBuffer strbuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    req.Accept(writer);

    auto* param = new ("AppMsgParam") openplatform::AppMsgParam(doc["app_id"].GetString(), strbuf.GetString());
    SendNoReplyEvent("OpenPlatform", (int)IPOpenPlatformManager::EAppEventType::eExclusiveMode, 0, param);

    return true;
  }
  return false;
}

bool SampleComponent::HandleGetExclusiveModeStatus(const std::string& request) {
  JsonUtility::JsonDocument doc;
  doc.Parse(request);

  if (doc.HasMember("app_id")) {
    auto* param = new ("AppMsgParam") openplatform::AppMsgParam(doc["app_id"].GetString(), "");
    SendReplyEvent("OpenPlatform", (int)IPOpenPlatformManager::EAppEventType::eGetExclusiveModeStatus, 0, param,
                   (int)IPOpenPlatformManager::EAppEventType::eGetExclusiveModeStatusDone);

    return true;
  }
  return false;
}

bool SampleComponent::HandleGetExclusiveModeResult(Event* event) {
  std::map<bool, std::string> BoolToString = {{true, "True"}, {false, "False"}};
  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  JsonUtility::JsonDocument doc(JsonUtility::Type::kObjectType);
  auto& alloc = doc.GetAllocator();

  JsonUtility::set(doc, "Mode", BoolToString[this->exclusive_mode_], alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  doc.Accept(writer);

  oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());

  return true;
}

void SampleComponent::SetExclusiveModeStatus(Event* event) {
  if (event->IsReply()) {
    auto blob = event->GetBlobArgument();
    auto* data = blob.GetRawData();
    if (data != nullptr) {
      std::pair<std::variant<BaseObject*, char*>, uint64_t> ret((char*)blob.GetRawData(), blob.GetSize());
      openplatform::AppMsgParam param;
      param.DeserializeBaseObject(&param, ret);

      auto message = param.GetMessage();

      JsonUtility::JsonDocument doc;
      doc.Parse(message);

      if (doc.HasMember("status")) {
        this->exclusive_mode_ = doc["status"].GetInt();
        printf("Exclusive mode : %d\n", this->exclusive_mode_);
      }
    }
  }
}

bool SampleComponent::HandleGetSDCardPath(Event* event) {
  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());

  JsonUtility::JsonDocument doc(JsonUtility::Type::kObjectType);
  auto& alloc = doc.GetAllocator();

  doc.AddMember("path", sd_card_app_path_, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  doc.Accept(writer);

  oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());

  return true;
}

bool SampleComponent::HandleStopResult(Event* event) {
  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());

  JsonUtility::JsonDocument doc(JsonUtility::Type::kObjectType);
  auto& alloc = doc.GetAllocator();

  doc.AddMember("result", stop_result_, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  doc.Accept(writer);

  oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());

  return true;
}

bool SampleComponent::HandleFormatResult(Event* event) {
  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());

  JsonUtility::JsonDocument doc(JsonUtility::Type::kObjectType);
  auto& alloc = doc.GetAllocator();

  doc.AddMember("result", format_result_, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  doc.Accept(writer);

  oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());

  return true;
}

bool SampleComponent::ParseSDCardStatus(const std::string &sd_status) {
  std::cout << "status:" << sd_status.c_str() << std::endl;

  JsonUtility::JsonDocument doc;
  doc.Parse(sd_status);

  if (doc.HasMember("SDCardSlots") && doc["SDCardSlots"].IsArray()) {
    slot_status_.clear();
    for (auto &itr : doc["SDCardSlots"].GetArray()) {
      SlotStatus s;
      JsonUtility::get(itr, "SlotIndex", s.slot_index);
      JsonUtility::get(itr, "IsPresent", s.is_present);
      JsonUtility::get(itr, "IsMounted", s.is_mounted);

      printf("SlotIndex:%d IsPresent:%d, IsMounted:%d \n", s.slot_index, s.is_present, s.is_mounted);
      slot_status_.push_back(std::move(s));
    }
  }

  return true;
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}