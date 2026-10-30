#pragma once

#include "typedef_application.h"

constexpr ClassID _SampleComponent_Id = GET_CLASS_UID(_ELayer_Application::_eSampleComponent);

class ISampleComponent {
 public:
  enum class EEventType { eBegin = _SampleComponent_Id, eEnd };

  struct SlotStatus {
    int slot_index;
    bool is_present;
    bool is_mounted;
  };
};
