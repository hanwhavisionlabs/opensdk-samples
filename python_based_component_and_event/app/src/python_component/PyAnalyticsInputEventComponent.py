# Copyright (c) 2026 Hanwha Vision Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

import json

from PyComponentModule import (
    AnalyticsEventType,
    Component,
    OpenAppSerializable,
    OpenAppResponseType,
    String,
)


class PyAnalyticsInputEventComponent(Component):
    def __init__(self):
        # PyComponentModule.Component.__init__() must be called explicitly
        # when overriding __init__, otherwise component creation fails with
        # "TypeError: PyComponentModule.Component.__init__() must be called
        # when overriding __init__".
        super().__init__()
        # Most recent 10 motion-detection (or other analytics) events.
        self.event_infos = []

    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        return self.RegisterOpenAPI("/python/analytics", ["GET"])

    def Finalize(self):
        return True

    def ProcessAEvent(self, event):
        if event is None:
            return False

        if event.GetType() == int(AnalyticsEventType.eNotifyEventOccured):
            # Analytics events arrive through the Blob argument (raw
            # serialized JSON string), not the BaseObject argument used by
            # OpenAPI HTTP requests. Deserialize the payload before parsing:
            # GetBlobArgument() returns the raw Blob, and
            # String.DeserializeBaseObject() deserializes it into a plain
            # string for json.loads().
            s = String("")
            s.DeserializeBaseObject(event.GetBlobArgument())
            try:
                payload = json.loads(s.c_str())
            except (ValueError, TypeError):
                return True

            info = {
                "event_type": payload.get("EventType", ""),
                "message_time": payload.get("MessageTime", ""),
                "state": payload.get("State", False),
                "channel": str(payload["Channel"]) if "Channel" in payload else "None",
            }

            if len(self.event_infos) > 9:
                self.event_infos.pop(0)
            self.event_infos.append(info)
            return True

        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None:
            return False

        if request.GetObjectName() != "OpenAppSerializable":
            return False

        path = request.GetFCGXParam("PATH_INFO")
        if path != "/python/analytics":
            return False

        if request.GetMethod() != "GET":
            request.SetStatusCode(400)
            request.SetResponseBody("requested method is not supported", OpenAppResponseType.TEXT)
            return True

        request.SetStatusCode(200)
        request.SetResponseBody(json.dumps({"infos": self.event_infos}), OpenAppResponseType.JSON)
        return True
