#pragma once

#include <typedef_base.h>

#include <set>
#include <string>
#include <unordered_map>

#include "json_utility.h"
#include "serializable_json.h"

enum class _EPTestComponent {
  _eBegin = GET_LAYER_UID(_ELayer::_ePApplication),
  _eSampleComponent,
  _eEnd,
};

constexpr ClassID _SampleComponent_Id = GET_CLASS_UID(_EPTestComponent::_eSampleComponent);

class ISampleComponent {
  public:
  enum class EEventType { eBegin = _SampleComponent_Id, eEnd };
};
