# Copyright (c) 2026 Hanwha Vision Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

# Build native StreamMetadata requests without hiding unavailable runtime inputs.
import json
import time

import PyComponentModule as py_api
from PyComponentModule import (
    Component,
    FrameMetadataItem,
    IPMetadataManager,
    OpenAppSerializable,
    Metadata,
    OpenAppResponseType,
    StreamMetadata,
    String,
)
from PyComponentModule.metadata.common import Object, ObjectCategory


class SendMetadataComponent(Component):
    def __init__(self):
        super().__init__()
        self.app_id = ""

    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        self.RegisterOpenAPI("/send_metadata/configuration", ["POST"])
        return True

    def Finalize(self):
        return True

    @staticmethod
    def _event_enum(member):
        for enum_name in ("MetadataRequestEventType", "LifeCycleManagerOpenAppEventType"):
            enum_type = getattr(py_api, enum_name, None)
            value = getattr(enum_type, member, None) if enum_type is not None else None
            if value is not None:
                return value
        return None

    @staticmethod
    def _number(body, name, converter):
        if name not in body:
            raise ValueError("missing required field: " + name)
        try:
            value = converter(body[name])
        except (TypeError, ValueError):
            raise ValueError("invalid field: " + name)
        if isinstance(value, bool):
            raise ValueError("invalid field: " + name)
        return value

    @staticmethod
    def _text(body, name):
        value = body.get(name)
        if not isinstance(value, str) or not value:
            raise ValueError("missing required field: " + name)
        return value

    @staticmethod
    def _bool(value):
        if isinstance(value, bool):
            return value
        if isinstance(value, str):
            lowered = value.lower()
            if lowered in ("1", "true", "yes", "on"):
                return True
            if lowered in ("0", "false", "no", "off"):
                return False
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            return bool(value)
        raise ValueError("invalid field: element_flag")

    @staticmethod
    def _timestamp(body):
        value = body.get("timestamp", int(time.time() * 1000))
        try:
            value = int(value)
        except (TypeError, ValueError):
            raise ValueError("invalid field: timestamp")
        if value < 0:
            raise ValueError("invalid field: timestamp")
        return value

    def _build_event_metadata(self, body, timestamp):
        item = py_api.EventMetadataItem()
        item.set_topic(self._text(body, "topic_str"))
        item.set_timestamp(timestamp)
        item.set_property(str(body.get("property", "")))
        item.add_source(
            py_api.EventMetadataItem.SimpleItem(
                self._text(body, "source_key"), self._text(body, "source_value")
            )
        )
        item.add_data(
            py_api.EventMetadataItem.SimpleItem(
                self._text(body, "data_key"), self._text(body, "data_value")
            )
        )

        if "element_flag" in body and self._bool(body["element_flag"]):
            candidate = py_api.EventMetadataItem.Candidate()
            candidate.type_name = self._text(body, "element_type_name")
            candidate.likelihood = self._number(body, "element_likelihood", float)
            if not 0.0 <= candidate.likelihood <= 1.0:
                raise ValueError("invalid field: element_likelihood")

            element = py_api.EventMetadataItem.ElementItem()
            element.element_name = self._text(body, "element_name")
            element.descriptor_name = self._text(body, "element_descriptor_name")
            element.element_data = str(body.get("element_data", ""))
            element.add_candidate(candidate)
            item.add_element(element)
        return item

    @staticmethod
    def _category(value):
        if isinstance(value, str) and not value.isdigit():
            category = getattr(ObjectCategory, value, None)
            if category is None:
                raise ValueError("invalid field: category")
            return category
        try:
            category_index = int(value)
        except (TypeError, ValueError):
            raise ValueError("invalid field: category")
        categories = (
            ObjectCategory.kUnknown,
            ObjectCategory.kFace,
            ObjectCategory.kHuman,
            ObjectCategory.kVehicle,
            ObjectCategory.kAnimal,
            ObjectCategory.kBicycle,
            ObjectCategory.kCar,
            ObjectCategory.kMotorCycle,
            ObjectCategory.kBus,
            ObjectCategory.kTruck,
            ObjectCategory.kTrain,
            ObjectCategory.kOther,
            ObjectCategory.kLicensePlate,
            ObjectCategory.kHead,
            ObjectCategory.kGroup,
            ObjectCategory.kShoppingCart,
            ObjectCategory.kForklift,
            ObjectCategory.kHeavyDutyTruck,
            ObjectCategory.kMidDutyTruck,
            ObjectCategory.kLightDutyTruck,
            ObjectCategory.kSchoolBus,
            ObjectCategory.kEmployeeShuttle,
            ObjectCategory.kVessel,
            ObjectCategory.kFire,
            ObjectCategory.kSmoke,
        )
        if category_index < 0 or category_index >= len(categories):
            raise ValueError("invalid field: category")
        return categories[category_index]

    def _resolution(self, body):
        width = body.get("width", body.get("max_width"))
        height = body.get("height", body.get("max_height"))
        if width is None or height is None:
            raise RuntimeError(
                "maximum resolution is unavailable: SUNAPI requester details are not bound; "
                "provide width and height"
            )
        try:
            width, height = int(width), int(height)
        except (TypeError, ValueError):
            raise ValueError("invalid frame resolution")
        if width <= 0 or height <= 0:
            raise ValueError("invalid frame resolution")
        return width, height

    def _build_frame_metadata(self, body, timestamp):
        width, height = self._resolution(body)
        frame = FrameMetadataItem()
        frame.set_resolution(width, height)
        frame.set_timestamp(timestamp)
        if body.get("media_source_token") is not None:
            frame.set_media_source_token(str(body["media_source_token"]))
        if body.get("video_source_token") is not None:
            frame.set_video_source_token(str(body["video_source_token"]))

        send_object = Object()
        send_object.object_id = self._number(body, "object_id", int)
        send_object.parent_id = self._number(body, "parent_id", int)
        send_object.timestamp = timestamp
        send_object.likelihood = self._number(body, "likelihood", float)
        if not 0.0 <= send_object.likelihood <= 1.0:
            raise ValueError("invalid field: likelihood")
        send_object.category = self._category(body.get("category"))
        send_object.attr_num = int(body.get("attr_num", 1))
        for field in ("sx", "sy", "ex", "ey"):
            if field not in body:
                raise RuntimeError("Rect binding is unavailable")
            setattr(send_object.rect, field, self._number(body, field, int))
        frame.add_object(send_object)
        return frame

    def _send(self, body, metadata):
        event_type = self._event_enum("eRequestMetadata")
        if event_type is None:
            raise NotImplementedError("MetadataManager eRequestMetadata is not exposed by PyComponentModule")
        app_id = body.get("app_id") or self.app_id
        if not isinstance(app_id, str) or not app_id:
            raise RuntimeError("app_id is unavailable: the lifecycle-manager app-info event is not bound")

        builder = IPMetadataManager.StreamMetadataRequest.builder()
        builder.set_app_id(app_id)
        builder.set_stream_metadata(metadata)
        request = builder.build()
        if request is None:
            raise RuntimeError("StreamMetadataRequest builder returned no request")
        if not self.SendNoReplyEvent("MetadataManager", int(event_type), 0, request):
            raise RuntimeError("MetadataManager rejected the metadata request")

    def ProcessAEvent(self, event):
        if event is None:
            return False

        if event.IsReply():
            return True

        # Capture the app id when a future bridge exposes the native info enum.
        info_event = self._event_enum("eInformAppInfo")
        if info_event is not None and event.GetType() == int(info_event):
            try:
                text = String("")
                text.DeserializeBaseObject(event.GetBlobArgument())
                info = json.loads(text.c_str())
                self.app_id = info.get("AppId") or info.get("AppID") or self.app_id
            except (TypeError, ValueError, json.JSONDecodeError):
                pass
            return True

        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False
        if request.GetFCGXParam("PATH_INFO") != "/send_metadata/configuration":
            return False
        if request.GetMethod() != "POST":
            request.SetStatusCode(405)
            request.SetResponseBody("Method Not Allowed", OpenAppResponseType.TEXT)
            return True

        try:
            body = json.loads(request.GetRequestBody())
            if not isinstance(body, dict):
                raise ValueError("request body must be an object")
            mode = body.get("mode")
            if mode not in ("event_metadata", "frame_metadata"):
                raise ValueError("requested mode is not supported")
            timestamp = self._timestamp(body)
            channel = int(body.get("channel", self.GetChannel()))
            if mode == "event_metadata":
                metadata = StreamMetadata(channel, timestamp)
                metadata.add_event_metadata(self._build_event_metadata(body, timestamp))
                metadata.NeedBroadcasting(True)
            else:
                metadata = StreamMetadata(channel, timestamp, Metadata.MetadataType.kAI)
                metadata.set_frame_metadata(self._build_frame_metadata(body, timestamp))
            self._send(body, metadata)
        except json.JSONDecodeError:
            request.SetStatusCode(400)
            request.SetResponseBody("request body parse error", OpenAppResponseType.TEXT)
            return True
        except ValueError as error:
            request.SetStatusCode(400)
            request.SetResponseBody(str(error), OpenAppResponseType.TEXT)
            return True
        except NotImplementedError as error:
            request.SetStatusCode(501)
            request.SetResponseBody(json.dumps({"status": "unsupported", "reason": str(error)}), OpenAppResponseType.JSON)
            return True
        except RuntimeError as error:
            request.SetStatusCode(503)
            request.SetResponseBody(json.dumps({"status": "unavailable", "reason": str(error)}), OpenAppResponseType.JSON)
            return True

        request.SetStatusCode(200)
        request.SetResponseBody(json.dumps({"status": "ok", "sent": True}), OpenAppResponseType.JSON)
        return True
