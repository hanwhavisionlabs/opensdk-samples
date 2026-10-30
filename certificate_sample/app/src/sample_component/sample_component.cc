#include "sample_component.h"

#include <i_p_open_platform_manager.h>
#include <open_platform_define.h>
#include <filesystem>
#include <unistd.h>

#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"
#include "lifecycle_manager_interface.h"

using namespace openplatform;

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  bool result = true;
  switch (event->GetType()) {
    case (int32_t)IAppDispatcher::EEventType::eHttpRequest: {
      result = ParseHttpEvent(event);
      break;
    }
    default:
      result = Component::ProcessAEvent(event);
      break;
  }
  return result;
}

// ---------------------------------------------------------------------------
// HTTP request dispatcher
// ---------------------------------------------------------------------------
bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto* oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");
  auto method = oas->GetMethod();

  // ----- POST /cert/load-cert -----
  if (path_info == "/cert/load-cert" && method == "POST") {
    auto body = oas->GetRequestBody();
    JsonUtility::JsonDocument doc;
    doc.Parse(body);
    if (doc.HasParseError() || !doc.HasMember("to_path")) {
      oas->SetStatusCode(400);
      oas->SetResponseBody(R"({"error":"required: to_path"})");
      return false;
    }
    std::string result;
    bool ok = HandleLoadDeviceCert(doc["to_path"].GetString(), result);
    oas->SetStatusCode(ok ? 200 : 500);
    oas->SetResponseBody(R"({"result":")" + result + R"("})");
    return ok;
  }

  // ----- POST /cert/sign -----
  if (path_info == "/cert/sign" && method == "POST") {
    auto body = oas->GetRequestBody();
    JsonUtility::JsonDocument doc;
    doc.Parse(body);
    if (doc.HasParseError() || !doc.HasMember("name") || !doc.HasMember("data")) {
      oas->SetStatusCode(400);
      oas->SetResponseBody("{\"error\":\"required: name, data (and optionally digest, pass)\"}");
      return false;
    }
    std::string digest = doc.HasMember("digest") ? doc["digest"].GetString() : "SHA256";
    std::string pass = doc.HasMember("pass") ? doc["pass"].GetString() : "";
    std::string result;
    std::string encoded_data = Base64Encode(doc["data"].GetString());

    bool ok = HandleCreateDigitalSignature(doc["name"].GetString(), encoded_data.c_str(), digest, result);
    oas->SetStatusCode(ok ? 200 : 500);
    oas->SetResponseBody(R"({"signature":")" + result + R"("})");
    return ok;
  }

  oas->SetStatusCode(404);
  oas->SetResponseBody(R"({"error":"path not found"})");
  return false;
}

bool SampleComponent::HandleLoadDeviceCert(const std::string& to_path, std::string& result) {
  std::error_code ec;
  std::filesystem::create_directories(to_path, ec);
  if (ec) {
    DebugLog("[CertSample] LoadDeviceCert: failed to create dir %s: %s", to_path.c_str(), ec.message().c_str());
    return false;
  }
  auto* req = new ("CertLoadArg") CertLoadArg("default", to_path, /*to_name=*/"", false, 0644);
  auto reply = SendReplyEventWaitFor(kCertTimeoutMs, "OpenPlatform", static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eLoadDeviceCert), 0, req);
  if (!reply) {
    DebugLog("[CertSample] LoadDeviceCert: timeout/error");
    return false;
  }

  auto* response = reply->GetAttachmentPtr<String>();
  if (!response) {
    DebugLog("[CertSample] LoadDeviceCert: invalid reply");
    return false;
  }
  result = response->c_str();
  DebugLog("[CertSample] LoadDeviceCert: %s", result.c_str());
  return !result.empty();
}

bool SampleComponent::HandleCreateDigitalSignature(const std::string& name, const std::string& data, const std::string& digest, std::string& result) {
  auto* req = new ("CreateDigitalSignatureArg") CreateDigitalSignatureArg(name, data, digest, "");
  auto reply =
      SendReplyEventWaitFor(kCertTimeoutMs, "OpenPlatform", static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eCreateDigitalSignature), 0, req);
  if (!reply) {
    DebugLog("[CertSample] CreateDigitalSignature: timeout/error");
    return false;
  }

  auto* response = reply->GetAttachmentPtr<String>();
  if (!response) {
    DebugLog("[CertSample] CreateDigitalSignature: invalid reply");
    return false;
  }
  result = response->c_str();
  DebugLog("[CertSample] CreateDigitalSignature: name=%s, digest=%s -> sig_len=%zu", name.c_str(), digest.c_str(), result.size());
  return !result.empty();
}

// ---------------------------------------------------------------------------
// URI registration
// ---------------------------------------------------------------------------
void SampleComponent::RegisterOpenAPIURI() {
  auto reg = [&](const char* path, std::initializer_list<const char*> ms) {
    Vector<String> methods;
    for (auto m : ms) methods.push_back(m);
    auto* r = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String(path), GetInstanceName(), methods);
    SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, r);
  };

  reg("/cert/load-cert", {"POST"});
  reg("/cert/sign", {"POST"});
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}