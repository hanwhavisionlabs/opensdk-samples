# Send SD-card control requests and retain asynchronous native replies.
import json

from PyComponentModule import (
    AppMsgParam,
    Component,
    OpenAppSerializable,
    OpenAppResponseType,
    OpenPlatformAppEventType,
    String,
)


class SdcardOutputEventComponent(Component):
    def __init__(self):
        super().__init__()
        self.last_reply = None
        self.slot_status = []
        self.sd_card_app_path = ""
        self.stop_result = ""
        self.format_result = ""
        self.exclusive_mode = False

    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        methods = ("GET", "POST")
        paths = (
            "/sdcard_output_event/getsdstatus",
            "/sdcard_output_event/getsdstatusresult",
            "/sdcard_output_event/startsdcard",
            "/sdcard_output_event/getsdcardpath",
            "/sdcard_output_event/stopsdcard",
            "/sdcard_output_event/stopresult",
            "/sdcard_output_event/formatsdcard",
            "/sdcard_output_event/formatresult",
            "/sdcard_output_event/setexclusivemode",
            "/sdcard_output_event/getexclusivemodestatus",
            "/sdcard_output_event/getexclusivemoderesult",
        )
        return all(self.RegisterOpenAPI(path, methods) for path in paths)

    def Finalize(self):
        return True

    @staticmethod
    def _response(request, code, payload, response_type=OpenAppResponseType.JSON):
        request.SetStatusCode(code)
        body = payload if isinstance(payload, str) else json.dumps(payload)
        request.SetResponseBody(body, response_type)
        return True

    @staticmethod
    def _deserialize_string(event):
        value = String("")
        value.DeserializeBaseObject(event.GetBlobArgument())
        return value.c_str()

    @staticmethod
    def _deserialize_app_message(event):
        value = AppMsgParam()
        value.DeserializeBaseObject(event.GetBlobArgument())
        return value.GetMessage()

    def _remember_reply(self, event):
        reply_type = event.GetReplyType()
        event_types = OpenPlatformAppEventType
        exclusive_reply = (
            event.GetType() == int(event_types.eGetExclusiveModeStatusDone)
            or reply_type == int(event_types.eGetExclusiveModeStatusDone)
        )
        payload = self._deserialize_app_message(event) if exclusive_reply else self._deserialize_string(event)
        self.last_reply = {"type": reply_type, "payload": payload}
        if reply_type == int(event_types.eGetSDCardStatus):
            try:
                status = json.loads(payload)
            except (TypeError, ValueError, json.JSONDecodeError):
                status = {}
            slots = status.get("SDCardSlots", []) if isinstance(status, dict) else []
            self.slot_status = [
                {
                    "slot_index": item.get("SlotIndex"),
                    "is_present": item.get("IsPresent"),
                    "is_mounted": item.get("IsMounted"),
                }
                for item in slots
                if isinstance(item, dict)
            ]
        elif reply_type == int(event_types.eStartSDCard):
            self.sd_card_app_path = payload
        elif reply_type == int(event_types.eStopSDCard):
            self.stop_result = payload
        elif reply_type == int(event_types.eFormatSDCard):
            self.format_result = payload
        elif exclusive_reply:
            try:
                value = json.loads(payload).get("status", self.exclusive_mode)
            except (AttributeError, TypeError, ValueError, json.JSONDecodeError):
                value = self.exclusive_mode
            self.exclusive_mode = bool(value)

    def _send_reply(self, event_type, payload=None, reply_type=None):
        if reply_type is None:
            reply_type = event_type
        if payload is None:
            return self.SendReplyEvent("OpenPlatform", int(event_type), 0)
        return self.SendReplyEvent("OpenPlatform", int(event_type), 0, payload, int(reply_type))

    def _app_id(self, body):
        app_id = body.get("app_id") or self.GetInstanceName()
        if not isinstance(app_id, str) or not app_id:
            raise ValueError("missing required field: app_id")
        return app_id

    @staticmethod
    def _slot(body):
        try:
            slot = int(body.get("sdcard", 0))
        except (TypeError, ValueError):
            raise ValueError("invalid field: sdcard")
        if slot not in (0, 1):
            raise ValueError("sdcard must be 0 or 1")
        return slot

    def _app_message(self, body, message):
        return AppMsgParam(self._app_id(body), json.dumps(message))

    def _handle(self, path, body):
        name = path.rsplit("/", 1)[1]
        event_types = OpenPlatformAppEventType
        if name == "getsdstatus":
            if not self._send_reply(event_types.eGetSDCardStatus):
                raise RuntimeError("OpenPlatform rejected SD-card status request")
            return {"status": "accepted", "effect": "queued_reply"}
        if name == "startsdcard":
            payload = self._app_message(body, {"SDCard": self._slot(body)})
            if not self._send_reply(event_types.eStartSDCard, payload):
                raise RuntimeError("OpenPlatform rejected SD-card start request")
            return {"status": "accepted", "effect": "queued_reply"}
        if name == "stopsdcard":
            payload = self._app_message(body, {"SDCard": self._slot(body)})
            if not self._send_reply(event_types.eStopSDCard, payload):
                raise RuntimeError("OpenPlatform rejected SD-card stop request")
            return {"status": "accepted", "effect": "queued_reply"}
        if name == "formatsdcard":
            payload = self._app_message(body, {"SDCard": self._slot(body)})
            if not self._send_reply(event_types.eFormatSDCard, payload):
                raise RuntimeError("OpenPlatform rejected SD-card format request")
            return {"status": "accepted", "effect": "queued_reply"}
        if name == "setexclusivemode":
            value = body.get("exclusive_mode", "True")
            if isinstance(value, bool):
                enabled = value
            elif value in ("True", "False"):
                enabled = value == "True"
            else:
                raise ValueError("exclusive_mode must be True or False")
            payload = self._app_message(body, {"use": enabled})
            if not self.SendNoReplyEvent(
                "OpenPlatform", int(event_types.eExclusiveMode), 0, payload
            ):
                raise RuntimeError("OpenPlatform rejected exclusive-mode request")
            self.exclusive_mode = enabled
            return {"status": "accepted", "effect": "queued_no_reply"}
        if name == "getexclusivemodestatus":
            payload = self._app_message(body, {})
            if not self._send_reply(
                event_types.eGetExclusiveModeStatus,
                payload,
                event_types.eGetExclusiveModeStatusDone,
            ):
                raise RuntimeError("OpenPlatform rejected exclusive-mode status request")
            return {"status": "accepted", "effect": "queued_reply"}
        raise ValueError("requested mode is not supported")

    def _result(self, path):
        name = path.rsplit("/", 1)[1]
        if name == "getsdstatusresult":
            return {"slots": self.slot_status, "reply": self.last_reply}
        if name == "getsdcardpath":
            return {"path": self.sd_card_app_path}
        if name == "stopresult":
            return {"result": self.stop_result}
        if name == "formatresult":
            return {"result": self.format_result}
        if name == "getexclusivemoderesult":
            return {"Mode": "True" if self.exclusive_mode else "False"}
        raise ValueError("requested result is not supported")

    def ProcessAEvent(self, event):
        if event is None:
            return False
        if event.IsReply():
            self._remember_reply(event)
            return True

        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False
        path = request.GetFCGXParam("PATH_INFO")
        if not path.startswith("/sdcard_output_event/"):
            return False

        result_paths = {
            "getsdstatusresult", "getsdcardpath", "stopresult",
            "formatresult", "getexclusivemoderesult",
        }
        name = path.rsplit("/", 1)[1]
        if name in result_paths:
            return self._response(request, 200, self._result(path))
        if request.GetMethod() not in ("GET", "POST"):
            return self._response(request, 405, "Method Not Allowed", OpenAppResponseType.TEXT)
        try:
            body = json.loads(request.GetRequestBody() or "{}")
            if not isinstance(body, dict):
                raise ValueError("request body must be an object")
            return self._response(request, 202, self._handle(path, body))
        except json.JSONDecodeError:
            return self._response(request, 400, "request body parse error", OpenAppResponseType.TEXT)
        except ValueError as error:
            return self._response(request, 400, str(error), OpenAppResponseType.TEXT)
        except RuntimeError as error:
            return self._response(request, 503, {"status": "unavailable", "reason": str(error)})
