// Copyright (c) 2026 Hanwha Vision Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "sample_component.h"

#include <i_p_metadata_manager.h>
#include <onvif_json_validator.h>

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
    case static_cast<int32_t>(IPMetadataManager::EEventType::eMetadataRequest):
      ProcessMetadata(event);
      break;
    case static_cast<int32_t>(IPMetadataManager::EEventType::eJsonMetadataRequest):
      ProcessJsonMetadata(event);
      break;
    default:
      Component::ProcessAEvent(event);
      break;
  }
  return true;
}

void SampleComponent::ProcessMetadata(Event* event) {
  if(event == nullptr || event->IsReply()) {
    return;
  }

  auto attachment = event->GetAttachment<IPMetadataManager::MetadataOutput>();
  if(attachment) {
    LOG(GetObjectName(), __func__, __LINE__, "[CHANNEL] : " + std::to_string(attachment->channel()) + " [OUTPUT] : " + attachment->output());
  }
}

void SampleComponent::ProcessJsonMetadata(Event* event) {
  if (event == nullptr || event->IsReply()) {
    return;
  }

  auto attachment = event->GetAttachmentPtr<IPMetadataManager::MetadataOutput>();
  if (attachment) {
    bool validation = false;
    if (!onvif::ValidateOnvifJson(attachment->output())) {
      validation = false;
    }
    validation = true;

    LOG(GetObjectName(), __func__, __LINE__, "[CH] " + std::to_string(attachment->channel()) + " [TOPIC] " + attachment->topic() + " [OUTPUT] " + attachment->output() + " [VALID] " + std::string(validation ? "Success" : "Fail"));
  }
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}
