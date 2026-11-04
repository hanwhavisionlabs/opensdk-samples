# Copyright (c) 2026 Hanwha Vision Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

# Register dynamic-event payloads only when the native dispatcher enums exist.
import json
import time

import PyComponentModule as py_api
from PyComponentModule import Component, IPMetadataManager, OpenAppResponseType, OpenAppSerializable, String, StringMetadata


class DynamicEventComponent(Component):
    APP_ID = "python_based_component_and_event"
    EVENT_NAME = "TestSingleEvent"

    def __init__(self):
        super().__init__()
        self.app_id = self.APP_ID
        self.schema_infos = {}
        self.metadata_send_count = 0

    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        self.RegisterOpenAPI("/dynamic_event/configuration", ["POST"])
        return True

    def Finalize(self):
        return True

    @staticmethod
    def _event_enum(member):
        enum_names = (
            "EventstatusCGIDispatcherEventType",
            "OpenSDKCGIDispatcherEventType",
            "EventstatusCGIEvent",
            "EventStatusCGIEvent",
            "EventstatusCGIEventType",
            "OpenSDKCGIEvent",
            "OpenSDKCGIEventType",
            "MetadataRequestEventType",
        )
        for enum_name in enum_names:
            enum_type = getattr(py_api, enum_name, None)
            value = getattr(enum_type, member, None) if enum_type is not None else None
            if value is not None:
                return value
        return None

    @staticmethod
    def _response(request, code, payload, response_type=OpenAppResponseType.JSON):
        request.SetStatusCode(code)
        body = payload if isinstance(payload, str) else json.dumps(payload)
        request.SetResponseBody(body, response_type)
        return True

    def _metadata_schema(self):
        event_name = "OpenSDK.%s.%s" % (self.app_id, self.EVENT_NAME)
        event_topic = "tns1:OpenApp/%s/%s" % (self.app_id, self.EVENT_NAME)
        schema = (
            "<tns1:OpenApp><dynamic_event>"
            "<TestSingleEvent wstop:topic=\"true\"><tt:MessageDescription IsProperty=\"true\">"
            "<tt:Source><tt:SimpleItemDescription Name=\"VideoSourceToken\" Type=\"tt:ReferenceToken\"/>"
            "<tt:SimpleItemDescription Name=\"RuleName\" Type=\"xsd:string\"/></tt:Source>"
            "<tt:Data><tt:SimpleItemDescription Name=\"State\" Type=\"xsd:boolean\"/>"
            "</tt:Data></tt:MessageDescription></TestSingleEvent></dynamic_event></tns1:OpenApp>"
        )
        return json.dumps(
            {
                "PROPRIETARY": {"EventName": event_name, "EventTopic": event_topic, "EventSchema": schema},
                "ONVIF": {"EventName": event_name, "EventTopic": event_topic, "EventSchema": schema},
            }
        )

    def _event_status_schema(self):
        event_name = "OpenSDK.%s.%s" % (self.app_id, self.EVENT_NAME)
        return json.dumps(
            {
                "JSON": {
                    "type": "object",
                    "properties": {
                        "Time": {"type": "string"},
                        "EventName": {"enum": [event_name]},
                        "Source": {"type": "object"},
                        "Data": {"type": "object", "properties": {"State": {"type": "boolean"}}},
                    },
                },
                "TEXT": {"SCHEME": "Name=%s" % event_name},
            }
        )

    def _meta_frame_schema(self):
        return json.dumps({"AppID": self.app_id, "Schema": "", "Encoding": "base64"})

    def _meta_frame_capability(self):
        return json.dumps(
            {
                "AppID": self.app_id,
                "Capabilities": [
                    {"xpath": "//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:Class/tt:Type", "type": "xs:string"},
                    {"xpath": "//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:Class/tt:Type/@Likelihood", "type": "xs:float", "minimum": 0.0, "maximum": 1.0},
                ],
            }
        )

    def _channel_event(self, channel, state):
        event_name = "OpenSDK.%s.%s" % (self.app_id, self.EVENT_NAME)
        return json.dumps(
            {
                "ChannelEvent": {
                    "Channel": channel,
                    "State": state,
                    "Time": time.strftime("%Y-%m-%dT%H:%M:%S.000+00:00", time.gmtime()),
                    "EventName": event_name,
                    "Source": {
                        "Channel": channel,
                        "AppName": self.app_id,
                        "AppID": self.app_id,
                        "AppEvent": self.EVENT_NAME,
                        "Type": "Event",
                        "RuleIndex": 1,
                        "VideoSourceToken": "vs-%s" % channel,
                        "RuleName": "Rule-1",
                    },
                    "Data": {"State": state},
                }
            }
        )

    def _send_text(self, receiver, event_type, text):
        if event_type is None:
            raise NotImplementedError("required native dispatcher event enum is not exposed by PyComponentModule")
        if not self.SendNoReplyEvent(receiver, int(event_type), 0, String(text)):
            raise RuntimeError("native dispatcher rejected the request")

    def _send_raw_metadata(self, body):
        event_type = self._event_enum("eRequestRawMetadata")
        if event_type is None:
            raise NotImplementedError("MetadataManager eRequestRawMetadata is not exposed by PyComponentModule")
        data = body.get("data")
        if not isinstance(data, str) or not data:
            raise ValueError("missing required field: data")
        try:
            channel = int(body.get("channel", self.GetChannel()))
            timestamp = int(body.get("timestamp", time.time() * 1000))
        except (TypeError, ValueError):
            raise ValueError("invalid channel or timestamp")
        if timestamp < 0:
            raise ValueError("invalid channel or timestamp")
        app_id = body.get("app_id") or self.app_id
        if not isinstance(app_id, str) or not app_id:
            raise ValueError("missing required field: app_id")
        metadata = StringMetadata(channel, timestamp)
        metadata.Set(data)
        request = IPMetadataManager.StringMetadataRequest(metadata)
        request.SetAppID(app_id)
        if not self.SendNoReplyEvent("MetadataManager", int(event_type), 0, request):
            raise RuntimeError("MetadataManager rejected raw metadata")

    def _handle_mode(self, body):
        mode = body.get("mode")
        if mode == "dynamic_event":
            self._send_raw_metadata(body)
            self.metadata_send_count += 1
            return {"status": "ok", "mode": mode, "sent": True}

        schema_modes = {
            "metadata_schema": ("OpenEventDispatcher", "eMetadataSchema", self._metadata_schema),
            "event_status_schema": ("OpenEventDispatcher", "eEventstatusSchema", self._event_status_schema),
            "meta_frame_schema": ("Stub::Dispatcher::OpenSDK", "eMetaFrameSchema", self._meta_frame_schema),
            "meta_frame_capability": ("Stub::Dispatcher::OpenSDK", "eMetaFrameCapability", self._meta_frame_capability),
        }
        if mode in schema_modes:
            receiver, event_name, create_schema = schema_modes[mode]
            schema = create_schema()
            self._send_text(receiver, self._event_enum(event_name), schema)
            self.schema_infos[mode] = schema
            return {"status": [{"setting_type": mode, "schema": schema}]}

        if mode == "event_status_change":
            event_type = self._event_enum("eEventStatusChanged")
            if event_type is None:
                raise NotImplementedError("Event Status Changed enum is not exposed by PyComponentModule")
            channel = int(body.get("channel", self.GetChannel()))
            state = body.get("state")
            if not isinstance(state, bool):
                state = self.metadata_send_count % 2 == 1
            self._send_text("OpenEventDispatcher", event_type, self._channel_event(channel, state))
            return {"status": "ok", "mode": mode, "sent": True}

        raise ValueError("requested mode is not supported")

    def ProcessAEvent(self, event):
        if event is None:
            return False
        info_event = getattr(py_api, "LifeCycleManagerOpenAppEventType", None)
        if info_event is not None and event.GetType() == int(info_event.eInformAppInfo):
            try:
                info = String("")
                info.DeserializeBaseObject(event.GetBlobArgument())
                self.app_id = json.loads(info.c_str()).get("AppId", self.app_id)
            except (TypeError, ValueError, json.JSONDecodeError):
                pass
            return True
        if event.IsReply():
            return True
        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False
        if request.GetFCGXParam("PATH_INFO") != "/dynamic_event/configuration":
            return False
        if request.GetMethod() != "POST":
            return self._response(request, 405, "Method Not Allowed", OpenAppResponseType.TEXT)
        try:
            body = json.loads(request.GetRequestBody())
            if not isinstance(body, dict):
                raise ValueError("request body must be an object")
            result = self._handle_mode(body)
        except json.JSONDecodeError:
            return self._response(request, 400, "request body parse error", OpenAppResponseType.TEXT)
        except ValueError as error:
            return self._response(request, 400, str(error), OpenAppResponseType.TEXT)
        except NotImplementedError as error:
            return self._response(request, 501, {"status": "unsupported", "reason": str(error)})
        except RuntimeError as error:
            return self._response(request, 502, {"status": "unavailable", "reason": str(error)})
        return self._response(request, 200, result)
