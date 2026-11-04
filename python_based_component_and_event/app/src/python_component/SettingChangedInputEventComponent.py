# Copyright (c) 2026 Hanwha Vision Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

import json
from datetime import datetime, timezone

from PyComponentModule import Component, OpenAppResponseType, OpenAppSerializable, OpenPlatformAppEventType, String


class SettingChangedInputEventComponent(Component):
    def __init__(self):
        super().__init__()
        self.setting_changed_info = {
            setting_type: {"latest_changed": "", "channel": ""}
            for setting_type in (
                "Network Interface",
                "Network Port",
                "Video Profile",
                "Audio",
                "Image",
                "Video Rotation",
                "WiseStream",
                "Exposure AI",
                "Record",
                "Language",
                "Time",
                "Analytics",
            )
        }

    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        return self.RegisterOpenAPI("/configuration", ["GET"])

    def Finalize(self):
        return True

    def ProcessAEvent(self, event):
        if event is None:
            return False

        setting_type = self._setting_type_for_event(event.GetType())
        if setting_type is not None:
            channel = str(event.GetChannel())
            if setting_type == "Analytics":
                payload_string = String("")
                payload_string.DeserializeBaseObject(event.GetBlobArgument())
                try:
                    payload = json.loads(payload_string.c_str())
                except (ValueError, TypeError):
                    return True
                if "EventType" not in payload or "Channel" not in payload:
                    return True
                channel = str(payload["Channel"])

            self.setting_changed_info[setting_type] = {
                "latest_changed": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S"),
                "channel": (
                    channel
                    if setting_type not in ("Network Interface", "Network Port", "Language", "Time")
                    else "None"
                ),
            }
            return True

        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False
        if request.GetFCGXParam("PATH_INFO") != "/configuration":
            return False
        if request.GetMethod() != "GET":
            request.SetStatusCode(400)
            request.SetResponseBody("requested method is not supported", OpenAppResponseType.TEXT)
            return True

        status = [
            {
                "setting_type": setting_type,
                "latest_changed": info["latest_changed"],
                "channel": info["channel"],
            }
            for setting_type, info in self.setting_changed_info.items()
        ]
        request.SetStatusCode(200)
        request.SetResponseBody(json.dumps({"status": status}), OpenAppResponseType.JSON)
        return True

    @staticmethod
    def _setting_type_for_event(event_type):
        event_types = OpenPlatformAppEventType
        setting_events = {
            int(event_types.eNetworkSettingChanged): "Network Interface",
            int(event_types.eNetworkPortChanged): "Network Port",
            int(event_types.eProfileSettingChanged): "Video Profile",
            int(event_types.eAudioSettingChanged): "Audio",
            int(event_types.eImageSettingChanged): "Image",
            int(event_types.eUpdateVideoRotation): "Video Rotation",
            int(event_types.eWiseStreamSettingChanged): "WiseStream",
            int(event_types.eExposureAISettingChanged): "Exposure AI",
            int(event_types.eRecordSettingChanged): "Record",
            int(event_types.eUpdateSystemSetting): "Language",
            int(event_types.eTimeSettingChanged): "Time",
            int(event_types.eAnalyticsSettingChanged): "Analytics",
        }
        return setting_events.get(int(event_type))
