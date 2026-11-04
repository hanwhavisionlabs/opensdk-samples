// Copyright (c) 2026 Hanwha Vision Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include <i_rule_manager.h>
#include <sample_component.h>

#include <unistd.h>

#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"
#include "lifecycle_manager_openapp.h"
// #include "analytics_detector.h"

enum class InternalEvent {
  eBegin = _SampleComponent_Id,
  eTriggeredProperty,
  eStopTriggerProperty,
  eEnd,
};

SampleComponent::SampleComponent() : ThreadSafeComponent(_SampleComponent_Id, "SampleComponent"), EventSchedulable(this) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  if (event == nullptr) {
    return false;
  }
  bool result = true;

  switch (event->GetType()) {
    case (int32_t)IAppDispatcher::EEventType::eHttpRequest: {
      result = ParseHttpEvent(event);
      break;
    }
    case static_cast<int32_t>(InternalEvent::eTriggeredProperty): {
      if (!stop_property_trigger_flag_) {
        TriggerProperty("Start");
      }
      break;
    }
    case static_cast<int32_t>(InternalEvent::eStopTriggerProperty): {
      stop_property_trigger_flag_ = true;
      CancelDelayedEvent(InternalEvent::eTriggeredProperty);
      TriggerProperty("End");
      break;
    }
    default:
      return ThreadSafeComponent::ProcessAEvent(event);
  }
  return result;
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto* oas = reinterpret_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  if (oas == nullptr) {
    return false;
  }
  std::string path_info = oas->GetFCGXParam("PATH_INFO").c_str();

  if (path_info == "/configuration") {
    if (oas->GetMethod() == "POST") {
      auto body = oas->GetRequestBody();
      JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
      document.Parse(body);
      if (document.HasParseError()) {
        oas->SetStatusCode(400);
        oas->SetResponseBody("request body parse error");
        return false;
      }
      std::string mode = document["mode"].GetString();
      if (mode == "register_oneshot_instance") {
        if (registered_oneshot_instance_) {
          oas->SetStatusCode(400);
          oas->SetResponseBody("OneShot instance is already registered");
          return false;
        }

        one_shot_rule_flag_ = document["enable"].GetBool();
        if (RegisterOneShotInstance()) {
          registered_oneshot_instance_ = true;
        }

      } else if (mode == "register_property_instance") {
        if (registered_property_instance_) {
          oas->SetStatusCode(400);
          oas->SetResponseBody("Property instance is already registered");
          return false;
        }

        property_rule_flag_ = document["enable"].GetBool();
        if (RegisterPropertyInstance()) {
          registered_property_instance_ = true;
        }

      } else if (mode == "trigger_one_shot") {
        if (!registered_oneshot_instance_) {
          oas->SetStatusCode(400);
          oas->SetResponseBody("OneShot instance is not registered");
          return false;
        }

        oneshot_rule_index_ = document["rule_index"].GetInt();
        TriggerOneShot();

      } else if (mode == "trigger_property") {
        if (!registered_property_instance_) {
          oas->SetStatusCode(400);
          oas->SetResponseBody("Property instance is not registered");
          return false;
        }

        std::string state = document["state"].GetString();
        property_rule_index_ = document["rule_index"].GetInt();

        CancelDelayedEvent(InternalEvent::eTriggeredProperty);
        CancelDelayedEvent(InternalEvent::eStopTriggerProperty);

        stop_property_trigger_flag_ = false;
        TriggerProperty(state);

        // Send StopTriggerProperty event after 15 seconds
        if (state == "Start") {
          PostDelayedEvent(InternalEvent::eStopTriggerProperty, 15 * 1000);
        }

      } else if (mode == "unregister_oneshot_instance") {
        if (!registered_oneshot_instance_) {
          oas->SetStatusCode(400);
          oas->SetResponseBody("OneShot instance is not registered");
          return false;
        }

        UnregisterInstance(oneshot_rule_handler_, _SampleOneShotRuleHandler_Id);
        registered_oneshot_instance_ = false;

      } else if (mode == "unregister_property_instance") {
        if (!registered_property_instance_) {
          oas->SetStatusCode(400);
          oas->SetResponseBody("Property instance is not registered");
          return false;
        }

        CancelDelayedEvent(InternalEvent::eTriggeredProperty);
        CancelDelayedEvent(InternalEvent::eStopTriggerProperty);

        UnregisterInstance(property_rule_handler_, _SamplePropertyRuleHandler_Id);
        registered_property_instance_ = false;

      } else {
        oas->SetStatusCode(400);
        oas->SetResponseBody("requested mode is not supported");
        return false;
      }
      return true;
    }
    oas->SetStatusCode(400);
    oas->SetResponseBody("requested method is not supported");
    return false;
  }
  oas->SetStatusCode(400);
  oas->SetResponseBody("requested path is not supported");
  return false;
}

bool SampleComponent::RegisterOneShotInstance() {
  return RegisterInstance(oneshot_rule_handler_, _SampleOneShotRuleHandler_Id, "SampleOneShotMotion", "OneShot",
                          GetRuleOptions("SampleOneShotMotion", "OneShot", one_shot_rule_flag_));
}

bool SampleComponent::RegisterPropertyInstance() {
  return RegisterInstance(property_rule_handler_, _SamplePropertyRuleHandler_Id, "SamplePropertyMotion", "Property",
                          GetRuleOptions("SamplePropertyMotion", "Property", property_rule_flag_));
}

bool SampleComponent::RegisterInstance(std::unique_ptr<RuleHandler>& handler, ClassID part_id, const std::string& name_suffix, const std::string& policy,
                                       const std::string& rule_options) {
  if (handler) {
    printf("RuleHandler for %s is already registered. Unregister it first.\n", name_suffix.c_str());
    return false;
  }

  handler = std::make_unique<RuleHandler>(GetAppId() + "." + name_suffix);
  AddPart(part_id, handler.get());

  handler->SetChannel(GetChannel());

  RuleHandler::RuleInstanceInfo info;
  info.rule_instance_name = GetAppId() + "." + name_suffix;
  info.instance_type = IRuleManager::RuleInstance::Type::kDetector;
  info.category = IRuleManager::RuleInstance::Category::kGeneral | IRuleManager::RuleInstance::Category::kOpenApp;

  info.attr["Policy"] = policy;
  info.attr["State"] = "Idle";
  info.attr["ChannelSupport"] = "All";
  if (policy == "OneShot" && one_shot_rule_flag_) {
    info.attr["Rule.1.Name"] = policy + "1";
    info.attr["Rule.1.Policy"] = "OneShot";
    info.attr["Rule.1.AlarmState"] = "Idle";
    info.attr["Rule.2.Name"] = policy + "2";
    info.attr["Rule.2.Policy"] = "OneShot";
    info.attr["Rule.2.AlarmState"] = "Idle";
  } else if (policy == "Property" && property_rule_flag_) {
    info.attr["Rule.1.Name"] = policy + "1";
    info.attr["Rule.1.Policy"] = "Property";
    info.attr["Rule.1.AlarmState"] = "Idle";
    info.attr["Rule.2.Name"] = policy + "2";
    info.attr["Rule.2.Policy"] = "Property";
    info.attr["Rule.2.AlarmState"] = "Idle";
  }

  info.dynamic_rule_options = rule_options;
  info.source_options = GetSourceOptions();

  handler->RegisterInstance(info);
  return true;
}

void SampleComponent::TriggerOneShot() {
  IRuleManager::RuleTrigger trigger;
  trigger.set_channel(GetChannel());
  trigger.set_detector(GetAppId() + ".SampleOneShotMotion");
  trigger.set_state("OneShot");

  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  IRuleManager::RuleTrigger::TriggerInfo trigger_info;
  trigger_info.set_timestamp(timestamp);
  if (one_shot_rule_flag_) {
    trigger_info.set_rule_index(oneshot_rule_index_);
  }
  trigger_info.set_alarm_state("OneShot");
  trigger.AddInfo(std::move(trigger_info));

  oneshot_rule_handler_->Trigger(std::move(trigger));
}

void SampleComponent::TriggerProperty(const std::string& state) {
  // If the state is "Start", a Trigger event must be sent every 5 seconds during the triggering period.
  // In the sample app, a Trigger event is sent every 5 seconds for 15 seconds.
  // If the status is "End", a Trigger event is sent once.
  printf("TriggerProperty() called with State: [%s]\n", state.c_str());

  IRuleManager::RuleTrigger trigger;
  trigger.set_channel(GetChannel());
  trigger.set_detector(GetAppId() + ".SamplePropertyMotion");
  trigger.set_state(state);

  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  IRuleManager::RuleTrigger::TriggerInfo trigger_info;
  trigger_info.set_timestamp(timestamp);
  if (property_rule_flag_) {
    trigger_info.set_rule_index(property_rule_index_);
  }
  trigger_info.set_alarm_state(state);
  trigger.AddInfo(std::move(trigger_info));

  property_rule_handler_->Trigger(std::move(trigger));

  if (!stop_property_trigger_flag_ && state == "Start") {
    // Schedule the next TriggerProperty event after 5 seconds
    PostDelayedEvent(InternalEvent::eTriggeredProperty, 5 * 1000);
  }
}

void SampleComponent::UnregisterInstance(std::unique_ptr<RuleHandler>& handler, ClassID part_id) {
  if (!handler) {
    return;
  }

  handler->UnregisterInstance();

  RemovePart(part_id);
  handler.reset();
}

std::string SampleComponent::GetRuleOptions(const std::string& type, const std::string& policy, bool rule_enable) {
  JsonUtility::JsonDocument document;
  auto& alloc = document.GetAllocator();
  document.SetObject();

  JsonUtility::set(document, "Type", type, alloc);
  JsonUtility::set(document, "Status", "Active", alloc);
  JsonUtility::set(document, "Policy", policy, alloc);
  JsonUtility::set(document, "AppName", GetAppId(), alloc);

  if (rule_enable) {
    JsonUtility::ValueType rule_array(JsonUtility::Type::kArrayType);
    JsonUtility::ValueType rule_object(rapidjson::kObjectType);
    JsonUtility::set(rule_object, "Rule", 1, alloc);
    JsonUtility::set(rule_object, "Name", policy + "1", alloc);
    JsonUtility::set(rule_object, "Policy", policy, alloc);
    rule_array.PushBack(rule_object, alloc);
    JsonUtility::ValueType rule_object2(rapidjson::kObjectType);
    JsonUtility::set(rule_object2, "Rule", 2, alloc);
    JsonUtility::set(rule_object2, "Name", policy + "2", alloc);
    JsonUtility::set(rule_object2, "Policy", policy, alloc);
    rule_array.PushBack(rule_object2, alloc);
    document.AddMember("Rule", rule_array, alloc);
  }

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  document.Accept(writer);

  return strbuf.GetString();
}

std::string SampleComponent::GetSourceOptions() { return ""; }

void SampleComponent::RegisterOpenAPIURI() {
  Vector<String> methods;
  methods.push_back("POST");

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/configuration"), GetInstanceName(), methods);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, uriRequest);
}

std::string SampleComponent::GetAppId() {
  if (app_id_.empty()) {
    return GetMyContainer();
  }
  return app_id_;
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}