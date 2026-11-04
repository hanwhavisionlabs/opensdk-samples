# Copyright (c) 2026 Hanwha Vision Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

# HomeComponent exposes safe static registration and liveness status.
import json

from PyComponentModule import Component, OpenAppResponseType, OpenAppSerializable


class HomeComponent(Component):
    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        return True

    def Start(self):
        self.RegisterOpenAPI("/home/status", ["GET"])
        self.RegisterOpenAPI("/python/health", ["GET"])

    def Finalize(self):
        return True

    def ProcessAEvent(self, event):
        if event is None:
            return False

        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None:
            return False

        if request.GetObjectName() != "OpenAppSerializable":
            return False

        path = request.GetFCGXParam("PATH_INFO")
        if path not in ("/home/status", "/python/health"):
            return False

        if request.GetMethod() != "GET":
            request.SetStatusCode(405)
            request.SetResponseBody("Method Not Allowed", OpenAppResponseType.TEXT)
            return True

        response = {
            "status": "ok",
            "component": "HomeComponent",
            "features": {
                "registration": {"status": "ok", "check": "static"},
                "liveness": {"status": "ok", "check": "static"},
                "downstream": {
                    "status": "not_run",
                    "reason": "Downstream actions are not invoked.",
                },
                "hardware": {
                    "status": "unsupported",
                    "reason": "Hardware actions are not performed.",
                },
            },
        }
        request.SetStatusCode(200)
        request.SetResponseBody(json.dumps(response), OpenAppResponseType.JSON)
        return True
