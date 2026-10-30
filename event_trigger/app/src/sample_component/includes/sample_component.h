#pragma once
#include <event_schedulable.h>
#include <thread_safe_component.h>
#include <typedef_base.h>
#include <rule_handler.h>

enum class _EPTestComponent {
  _eBegin = GET_LAYER_UID(_ELayer::_ePApplication),
  _eSampleComponent,
  _eSampleOneShotRuleHandler,
  _eSamplePropertyRuleHandler,
  _eEnd,
};

constexpr ClassID _SampleComponent_Id = GET_CLASS_UID(_EPTestComponent::_eSampleComponent);
constexpr ClassID _SampleOneShotRuleHandler_Id = GET_CLASS_UID(_EPTestComponent::_eSampleOneShotRuleHandler);
constexpr ClassID _SamplePropertyRuleHandler_Id = GET_CLASS_UID(_EPTestComponent::_eSamplePropertyRuleHandler);

class SampleComponent : public ThreadSafeComponent, public EventSchedulable {
 public:
  SampleComponent();
  virtual ~SampleComponent();

  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;
  virtual void RegisterOpenAPIURI();
  //void Start() override;

 private:
  bool RegisterInstance(std::unique_ptr<RuleHandler>& handler, ClassID part_id, const std::string& name_suffix, const std::string& policy,
                        const std::string& rule_options);

  bool RegisterOneShotInstance();
  bool RegisterPropertyInstance();

  void TriggerOneShot();
  void TriggerProperty(const std::string& state);

  void UnregisterInstance(std::unique_ptr<RuleHandler>& handler, ClassID part_id);

  std::string GetRuleOptions(const std::string& type, const std::string& policy, bool rule_enable);
  std::string GetSourceOptions();

  bool ParseHttpEvent(Event* event);

  std::string GetAppId();
private:
  std::string app_id_;
  std::unique_ptr<RuleHandler> oneshot_rule_handler_;
  std::unique_ptr<RuleHandler> property_rule_handler_;
  int oneshot_rule_index_ = 1;
  int property_rule_index_ = 1;

  bool registered_oneshot_instance_ = false;
  bool registered_property_instance_ = false;
  bool one_shot_rule_flag_ = false;
  bool property_rule_flag_ = false;
  bool stop_property_trigger_flag_ = false;
};