#include "webservice_component.h"

#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"
#include "i_p_open_platform_manager.h"

#define JPEG_FILE_PATH "/tmp/download/"

WebServiceComponent::WebServiceComponent() : WebServiceComponent(_WebServiceComponent_Id, "WebServiceComponent") {}

WebServiceComponent::WebServiceComponent(ClassID id, const char* name) : Component(id, name) {}

WebServiceComponent::~WebServiceComponent() {}

bool WebServiceComponent::Initialize() {
  RegisterOpenAPIURI();
  std::string manifest_path = "../../config/app_manifest.json";
  ParseManifest(manifest_path, manifest_);

  if (!std::filesystem::exists(JPEG_FILE_PATH)) {
    if (std::filesystem::create_directory(JPEG_FILE_PATH)) {
      std::cout << "Directory created successfully.\n";
    } else {
      std::cout << "Failed to create directory!\n";
    }
  }
  return Component::Initialize();
}

bool WebServiceComponent::ParseManifest(const std::string& manifest_path, ManifestInfo& info) {
  std::ifstream manifest_stream(manifest_path.c_str());
  if (!manifest_stream.is_open()) {
    DebugLog("Fail to parse the app manifest file of open app!(%s)", info.app_name.c_str());
    return false;
  }

  std::stringstream ss;
  JsonUtility::JsonDocument doc = PrepareJson();

  ss << manifest_stream.rdbuf();
  doc.Parse(ss.str());

  if (doc.HasMember("Permission")) {
    if (doc["Permission"].IsArray()) {
      info.permissions.clear();
      for (auto& p : doc["Permission"].GetArray()) {
        info.permissions.push_back(p.GetString());
      }
    }
  }

  JsonUtility::get(doc, "AppName", info.app_name);
  JsonUtility::get(doc, "AppVersion", info.version);

  return true;
}

bool WebServiceComponent::ProcessAEvent(Event* event) {
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

bool WebServiceComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");
  if (path_info == "/ANPRClientService/BigPictureRequest.svc") {
    if (oas->GetMethod() == "POST") {
      oas->SetStatusCode(200);
      oas->SetResponseBody(requestHttpService());
      oas->AddResponseHeader("Content-type", "text/xml; charset=utf-8");
      return true;
    }
    oas->SetStatusCode(400);
    oas->SetResponseBody("requested path is not supported");
    return false;
  }
  oas->SetStatusCode(400);
  oas->SetResponseBody("requested path is not supported");
  return false;
}

std::string WebServiceComponent::requestHttpService() {
  std::string response_body = "";
  response_body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  response_body += "<soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:cit=\"http://www.cityringclientservice.ae\">\n";
  response_body += "  <soapenv:Body>\n";
  response_body += "    <cit:GetBigPictureResponse>\n";
  response_body += "      <cit:SiteId>3742</cit:SiteId>\n";
  response_body += "      <cit:AccountId>2889</cit:AccountId>\n";
  response_body += "      <cit:TransactionId>1004346</cit:TransactionId>\n";
  response_body += "      <cit:Base64String>" + MakeBase64TestString() + "</cit:Base64String>\n";
  response_body += "    </cit:GetBigPictureResponse>\n";
  response_body += "  </soapenv:Body>\n";
  response_body += "</soapenv:Envelope>\n";
  return response_body;
}

std::string WebServiceComponent::MakeBase64TestString() {
  static int counter = 0;
  std::string test_string = "base64 encoding Test  String " + std::to_string(++counter);
  return base64_encode(test_string.c_str(), test_string.size());
}

std::string WebServiceComponent::base64_encode(const char* in, size_t n) {
  std::string out;
  int val = 0, valb = -6;
  for (int i = 0; i < n; ++i) {
    val = (val << 8) + in[i];
    valb += 8;

    while (valb >= 0) {
      out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }

  if (valb > -6) {
    out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
  }

  while (out.size() % 4) {
    out.push_back('=');
  }
  return out;
}

void WebServiceComponent::RegisterOpenAPIURI() {
  auto* attach = new ("OpenAPIRegistrar") IAppDispatcher::OpenAPIRegistrar(String("/ANPRClientService/BigPictureRequest.svc"), GetInstanceName(), "POST");
  attach->CustomSetting(true, netsvc::IHttpdService::AuthMethod::kNone);
  SendNoReplyEvent("AppDispatcher", (int32_t)IAppDispatcher::EEventType::eRegisterCommand, 0, attach);
}

extern "C" {
WebServiceComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("WebServiceComponent") WebServiceComponent();
}

void destroy_component(WebServiceComponent* ptr) { delete ptr; }
}