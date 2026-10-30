# Register and trigger native event-rule detector instances without Python timers.
import json
import os
import time
from PyComponentModule import Component, NativeRuleHandlerOwner, OpenAppResponseType, OpenAppSerializable, RuleHandler, RuleInstance, RuleTrigger

class EventTriggerComponent(Component):

    def __init__(self):
        super().__init__()
        self.owners = {"oneshot": None, "property": None}
        self.names = {"oneshot": "", "property": ""}
        self.enabled = {"oneshot": False, "property": False}

    def PreInitialize(self):
        return True
    def Initialize(self):
        return True
    def PostInitialize(self):
        return self.RegisterOpenAPI("/event_trigger/configuration", ["POST"])
    def Finalize(self):
        result = True
        for owner in self.owners.values():
            if owner is None:
                continue
            try:
                owner.CancelAllDelayedTriggers()
                owner.Teardown()
            except Exception:
                result = False
        self.owners = {"oneshot": None, "property": None}
        return result
    @staticmethod
    def _response(request, code, payload, response_type=OpenAppResponseType.JSON):
        request.SetStatusCode(code)
        request.SetResponseBody(payload if isinstance(payload, str) else json.dumps(payload), response_type)
        return True
    def _app_id(self, body, request):
        app_id = body.get("app_id") or os.getenv("WISENET_APP_ID")
        if not isinstance(app_id, str) or not app_id:
            request_uri = request.GetFCGXParam("REQUEST_URI")
            marker = "/opensdk/"
            if marker in request_uri:
                app_id = request_uri.split(marker, 1)[1].split("/", 1)[0]
        if not isinstance(app_id, str) or not app_id:
            raise RuntimeError("AppID is unavailable; provide app_id or WISENET_APP_ID")
        return app_id
    @staticmethod
    def _rule_index(body):
        value = body.get("rule_index", 1)
        if isinstance(value, bool):
            raise ValueError("invalid field: rule_index")
        try:
            value = int(value)
        except (TypeError, ValueError):
            raise ValueError("invalid field: rule_index")
        if value not in (1, 2):
            raise ValueError("rule_index must be 1 or 2")
        return value
    @staticmethod
    def _rule_options(app_id, detector_type, policy, enabled):
        options = {"Type": detector_type, "Status": "Active", "Policy": policy, "AppName": app_id}
        if enabled:
            options["Rule"] = [
                {"Rule": 1, "Name": "Rule1", "Policy": policy},
                {"Rule": 2, "Name": "Rule2", "Policy": policy},
            ]
        return json.dumps(options, separators=(",", ":"))
    def _register(self, kind, app_id, suffix, policy, enabled):
        name = "%s.%s" % (app_id, suffix)
        owner = NativeRuleHandlerOwner(self, name)
        category = int(RuleInstance.Category.kGeneral | RuleInstance.Category.kOpenApp)
        owner.SetChannel(int(self.GetChannel()))
        owner.SetCategory(category)
        info = RuleHandler.RuleInstanceInfo()
        info.rule_instance_name, info.instance_type, info.category = name, RuleInstance.Type.kDetector, category
        info.attr = {"Policy": policy, "State": "Idle", "ChannelSupport": "All"}
        if enabled:
            info.attr.update({
                "Rule.1.Name": "Rule1", "Rule.1.Policy": policy, "Rule.1.AlarmState": "Idle",
                "Rule.2.Name": "Rule2", "Rule.2.Policy": policy, "Rule.2.AlarmState": "Idle",
            })
        info.dynamic_rule_options = self._rule_options(app_id, suffix, policy, enabled)
        info.source_options = ""
        try:
            owner.RegisterInstance(info)
        except Exception:
            owner.Teardown()
            raise
        if not owner.IsRegistered():
            owner.Teardown()
            raise RuntimeError("RuleManager rejected registration for %s" % name)
        self.owners[kind], self.names[kind], self.enabled[kind] = owner, name, enabled
    def _owner(self, kind):
        owner = self.owners[kind]
        if owner is None or not owner.IsRegistered():
            raise ValueError("%s rule instance is not registered" % kind)
        if not owner.IsAttached():
            raise RuntimeError("%s rule instance is not registered" % kind)
        return owner
    def _build_trigger(self, detector, state, enabled, rule_index):
        trigger = RuleTrigger()
        trigger.set_channel(int(self.GetChannel()))
        trigger.set_detector(detector)
        trigger.set_state(state)
        info = RuleTrigger.TriggerInfo()
        info.set_timestamp(int(time.time() * 1000))
        if enabled:
            info.set_rule_index(rule_index)
        info.set_alarm_state(state)
        trigger.AddInfo(info)
        return trigger
    def _trigger(self, kind, state, rule_index):
        owner = self._owner(kind)
        trigger = self._build_trigger(self.names[kind], state, self.enabled[kind], rule_index)
        owner.Trigger(trigger)
        return trigger

    def _trigger_property(self, state, rule_index):
        owner = self._owner("property")
        owner.CancelAllDelayedTriggers()
        start_trigger = self._trigger("property", state, rule_index)
        if state != "Start":
            return
        end_trigger = self._build_trigger(
            self.names["property"], "End", self.enabled["property"], rule_index
        )
        base_id = int(owner.GetPartId())
        for trigger_id, delay, trigger, label in (
            (base_id + 1, 5000, start_trigger, "5-second"),
            (base_id + 2, 10000, start_trigger, "10-second"),
            (base_id + 3, 15000, end_trigger, "property end"),
        ):
            if not owner.PostDelayedTrigger(trigger_id, delay, trigger):
                owner.CancelAllDelayedTriggers()
                raise RuntimeError("native scheduling of the %s trigger failed" % label)
    def _unregister(self, kind):
        owner = self._owner(kind)
        owner.CancelAllDelayedTriggers()
        try:
            # Check the fire-and-forget dispatch before native teardown hides it.
            owner.UnregisterInstance()
            if owner.IsRegistered():
                raise RuntimeError("RuleManager rejected %s unregistration" % kind)
        finally:
            # Teardown is idempotent and also detaches the native ComponentPart.
            owner.Teardown()
            self.owners[kind] = None
    def _handle(self, body, request):
        mode = body.get("mode")
        registrations = {
            "register_oneshot_instance": ("oneshot", "SampleOneShotMotion", "OneShot"),
            "register_property_instance": ("property", "SamplePropertyMotion", "Property"),
        }
        if mode in registrations:
            kind, suffix, policy = registrations[mode]
            if self.owners[kind] is not None:
                raise ValueError("%s instance is already registered" % policy)
            enabled = body.get("enable", False)
            if not isinstance(enabled, bool):
                raise ValueError("invalid field: enable")
            self._register(kind, self._app_id(body, request), suffix, policy, enabled)
            return {"status": "accepted", "mode": mode, "effect": "queued_no_native_reply"}
        if mode == "trigger_one_shot":
            self._trigger("oneshot", "OneShot", self._rule_index(body))
            return {"status": "accepted", "mode": mode, "effect": "queued_no_native_reply"}
        if mode == "trigger_property":
            state = body.get("state")
            if state not in ("Start", "End"):
                raise ValueError("state must be Start or End")
            self._trigger_property(state, self._rule_index(body))
            return {"status": "accepted", "mode": mode, "effect": "queued_no_native_reply"}
        unregister_modes = {"unregister_oneshot_instance": "oneshot", "unregister_property_instance": "property"}
        if mode in unregister_modes:
            self._unregister(unregister_modes[mode])
            return {"status": "accepted", "mode": mode, "effect": "queued_no_native_reply"}
        raise ValueError("requested mode is not supported")
    def ProcessAEvent(self, event):
        if event is None:
            return False
        if event.IsReply():
            return True
        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False
        if request.GetFCGXParam("PATH_INFO") != "/event_trigger/configuration":
            return False
        if request.GetMethod() != "POST":
            return self._response(request, 405, "Method Not Allowed", OpenAppResponseType.TEXT)
        try:
            body = json.loads(request.GetRequestBody())
            if not isinstance(body, dict):
                raise ValueError("request body must be an object")
            return self._response(request, 202, self._handle(body, request))
        except json.JSONDecodeError:
            return self._response(request, 400, "request body parse error", OpenAppResponseType.TEXT)
        except ValueError as error:
            return self._response(request, 400, str(error), OpenAppResponseType.TEXT)
        except (AttributeError, NotImplementedError) as error:
            return self._response(request, 501, {"status": "unsupported", "reason": str(error)})
        except RuntimeError as error:
            return self._response(request, 503, {"status": "unavailable", "reason": str(error)})
