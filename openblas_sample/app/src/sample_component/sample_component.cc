#include "sample_component.h"

#include <i_app_dispatcher.h>
#include <dispatcher_serialize.h>

#include <cblas.h>


SampleComponent::SampleComponent() : SampleComponent(kID, kName.data()) {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();

  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
    case static_cast<int32_t>(IAppDispatcher::EEventType::eHttpRequest): {
      ParseHttpEvent(event);
      break;
    }
    default:
      Component::ProcessAEvent(event);
      break;
  }
  return true;
}

void SampleComponent::Start() {
  Component::Start();

  TestOpenBLAS();
}

void SampleComponent::RegisterOpenAPIURI() {
  auto get_puts = Vector<String>{};
  get_puts.push_back("POST");

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0,
     new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/calculation"), GetInstanceName(), get_puts));
}

void SampleComponent::TestOpenBLAS() {
  const int n = 3;

  std::vector<double> x{1.0, 2.0, 3.0};
  std::vector<double> y{4.0, 5.0, 6.0};
  double alpha = 2.0;

  int incx = 1;
  int incy = 1;

  //vector dot product calc
  // cblas_ddot: double precision dot product
  double result = cblas_ddot(n, x.data(),incx, y.data(), incy);

  // expected result : 1.0 * 4.0 + 2.0 * 5.0 + 3.0 + 6.0 = 32
  LOG(GetObjectName(), __func__, __LINE__, "Result : " + std::to_string(result));

  cblas_daxpy(n, alpha, x.data(), incx, y.data(), incy);

  for(auto value : y) {
    LOG(GetObjectName(), __func__, __LINE__, std::to_string(value));
  }
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  if(path_info == "/calculation") {
    if(oas->GetMethod() == "POST") {
      auto body = oas->GetRequestBody();

      LOG(GetObjectName(), __func__, __LINE__, body);

      JsonUtility::JsonDocument document;
      document.Parse(body);
      if(document.HasParseError()) {
        oas->SetStatusCode(STATUS_INVALID_VALUE);
        return true;
      }

      oas->SetStatusCode(200);
      oas->AddResponseHeader("Content-Type", "application/json");
      oas->SetResponseBody(GetResult(document));
      return true;
    }
  }
  oas->SetStatusCode(604);
  oas->SetResponseBody("not supported");
  return true;
}

std::string SampleComponent::GetResult(JsonUtility::JsonDocument& document) {
  if(!document.HasMember("rows") || !document.HasMember("cols") || !document.HasMember("matrix_1") || !document.HasMember("matrix_2")) {
    return "{}";
  }
  
  size_t rows = document["rows"].GetInt();
  size_t cols = document["cols"].GetInt();
  size_t count = rows * cols;

  std::vector<double> matrix_1;
  matrix_1.reserve(count);
  std::vector<double> matrix_2;
  matrix_2.reserve(count);

  auto& matrix_1_arr = document["matrix_1"];
  for(auto it = matrix_1_arr.Begin(); it != matrix_1_arr.End(); ++it) {
    for(auto it2 = it->Begin(); it2 != it->End(); ++it2) {
      matrix_1.push_back(static_cast<double>(it2->GetInt()));
    }
  }

  auto& matrix_2_arr = document["matrix_2"];
  for(auto it = matrix_2_arr.Begin(); it != matrix_2_arr.End(); ++it) {
    for(auto it2 = it->Begin(); it2 != it->End(); ++it2) {
      matrix_2.push_back(static_cast<double>(it2->GetInt()));
    }
  }

  std::vector<double> result(count, 0.0);
  
  cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, rows, cols, cols, 1.0, matrix_1.data(), cols, matrix_2.data(), cols, 0.0, result.data(), cols);

  JsonUtility::JsonDocument res;
  auto& alloc = res.GetAllocator();

  res.SetObject();

  res.AddMember("rows", rows, alloc);
  res.AddMember("cols", cols, alloc);

  JsonUtility::ValueType data_array(JsonUtility::Type::kArrayType);

  for(int i = 0; i < rows; ++i) {
    JsonUtility::ValueType row_array(JsonUtility::Type::kArrayType);
    for(int j = 0; j < cols; ++j) {
      row_array.PushBack(result[i * cols + j], alloc);
    }
    data_array.PushBack(row_array, alloc);
  }

  res.AddMember("result", data_array, alloc);

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  res.Accept(writer);

  return buf.GetString();
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}
