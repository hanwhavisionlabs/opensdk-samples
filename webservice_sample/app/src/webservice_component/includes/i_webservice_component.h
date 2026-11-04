// Copyright (c) 2026 Hanwha Vision Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <typedef_base.h>

#include <set>
#include <unordered_map>

#include "json_utility.h"
#include "serializable_json.h"

enum class _EPTestComponent {
  _eBegin = GET_LAYER_UID(_ELayer::_ePApplication),
  _eWebServiceComponent,
  _eEnd,
};

constexpr ClassID _WebServiceComponent_Id = GET_CLASS_UID(_EPTestComponent::_eWebServiceComponent);

class IWebServiceComponent {
 public:
  enum class EEventType { eBegin = _WebServiceComponent_Id, eRequestSunapi, eRequestSunapiDone, eEnd };
  enum class eEventFunction {
    eFTP_FILE_UPLOAD,
    eSEND_EMAIL_NOTIFICATION,
  };

  struct ManifestInfo {
   public:
    std::string app_name;
    std::string version;
    std::vector<std::string> permissions;
  };
};
