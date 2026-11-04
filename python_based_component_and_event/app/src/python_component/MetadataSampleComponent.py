# Copyright (c) 2026 Hanwha Vision Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

# Receive MetadataManager output attachments and expose the recent values.
import json

from PyComponentModule import Component, IPMetadataManager, OpenAppResponseType, OpenAppSerializable


class MetadataSampleComponent(Component):
    def __init__(self):
        super().__init__()
        self.metadata_outputs = []

    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        self.RegisterOpenAPI("/metadata_sample/status", ["GET"])
        return True

    def Finalize(self):
        return True

    def _remember_output(self, event, attachment, event_name):
        self.metadata_outputs.append(
            {
                "event_type": event_name,
                "channel": attachment.channel(),
                "topic": attachment.topic(),
                "output": attachment.output(),
                "output_size": attachment.output_size(),
                "event_channel": event.GetChannel(),
            }
        )
        if len(self.metadata_outputs) > 10:
            self.metadata_outputs.pop(0)

    def ProcessAEvent(self, event):
        if event is None:
            return False

        metadata_events = (
            (IPMetadataManager.EEventType.eMetadataRequest, "eMetadataRequest"),
            (IPMetadataManager.EEventType.eJsonMetadataRequest, "eJsonMetadataRequest"),
        )
        for event_type, event_name in metadata_events:
            if event.GetType() != int(event_type):
                continue
            if event.IsReply():
                return True

            # MetadataOutput is nested in the native IPMetadataManager binding.
            metadata_output = IPMetadataManager.MetadataOutput
            if event_type == IPMetadataManager.EEventType.eJsonMetadataRequest:
                attachment = event.GetAttachmentPtr(metadata_output)
            else:
                attachment = event.GetAttachment(metadata_output)
            if attachment is not None:
                self._remember_output(event, attachment, event_name)
            return True

        if event.IsReply():
            return True

        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False
        if request.GetFCGXParam("PATH_INFO") != "/metadata_sample/status":
            return False
        if request.GetMethod() != "GET":
            request.SetStatusCode(405)
            request.SetResponseBody("Method Not Allowed", OpenAppResponseType.TEXT)
            return True

        request.SetStatusCode(200)
        request.SetResponseBody(
            json.dumps(
                {
                    "status": "ok",
                    "component": "MetadataSampleComponent",
                    "received": len(self.metadata_outputs),
                    "metadata": self.metadata_outputs,
                }
            ),
            OpenAppResponseType.JSON,
        )
        return True
