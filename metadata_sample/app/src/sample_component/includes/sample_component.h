#pragma once

#include "component.h"
#include "i_sample_component.h"

class SampleComponent : public Component, public ISampleComponent {
 public:
  SampleComponent();
  SampleComponent(ClassID id, const char* name);
  virtual ~SampleComponent();
  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;

 private:
  void ProcessMetadata(Event* event);
  void ProcessJsonMetadata(Event* event);
};
