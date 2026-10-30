# Send multiline external OSD settings through SRMgrVideo.
import json
from PyComponentModule import Component, MultilineExternalOsdDeliveryObject, MultilineOsdItem, OpenAppResponseType, OpenAppSerializable, StreamRequesterManagerVideoEventType


class SetOsdMessageComponent(Component):
    def __init__(self):
        super().__init__()
        self.item = None

    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        return self.RegisterOpenAPI("/set_osd_message/configuration", ["POST"])

    def _send(self, item):
        delivery = MultilineExternalOsdDeliveryObject()
        delivery.AddOsdItem(item)
        if not self.SendNoReplyEvent("SRMgrVideo", int(StreamRequesterManagerVideoEventType.eExternalMultiLineOsd), 0, delivery):
            raise RuntimeError("SRMgrVideo rejected OSD request")

    def ProcessAEvent(self, event):
        if event is None or event.IsReply():
            return bool(event)
        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False
        if request.GetFCGXParam("PATH_INFO") != "/set_osd_message/configuration":
            return False
        if request.GetMethod() != "POST":
            request.SetStatusCode(405)
            request.SetResponseBody("Method Not Allowed", OpenAppResponseType.TEXT)
            return True
        try:
            body = json.loads(request.GetRequestBody() or "{}")
            item = MultilineOsdItem()
            item.Enable = bool(body.get("enable", True))
            item.Index = int(body.get("index", 101))
            item.PositionX = int(body.get("position_x", 0))
            item.PositionY = int(body.get("position_y", 0))
            item.OSDPosition = str(body.get("osd_position", "TopLeft"))
            item.OSDType = str(body.get("osd_type", "Title"))
            item.FontSize = str(body.get("font_size", "Small"))
            item.OSDColor = str(body.get("osd_color", "White"))
            item.Transparency = str(body.get("transparency", "Off"))
            item.OSD = str(body.get("message", ""))
            self._send(item)
            self.item = item
            request.SetStatusCode(202)
            request.SetResponseBody(json.dumps({"status": "accepted", "effect": "queued_no_native_reply"}), OpenAppResponseType.JSON)
        except (json.JSONDecodeError, TypeError, ValueError) as error:
            request.SetStatusCode(400)
            request.SetResponseBody(str(error), OpenAppResponseType.TEXT)
        except RuntimeError as error:
            request.SetStatusCode(503)
            request.SetResponseBody(json.dumps({"status": "unavailable", "reason": str(error)}), OpenAppResponseType.JSON)
        return True

    def Finalize(self):
        if self.item is not None:
            disabled = MultilineOsdItem()
            disabled.Index = self.item.Index
            try:
                self._send(disabled)
            except RuntimeError:
                return False
        return True
