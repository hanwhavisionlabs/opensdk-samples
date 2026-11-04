# Copyright (c) 2026 Hanwha Vision Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

# Send the bundled JPEG through the native TCP server managed by Network Manager.
import json
import os

from PyComponentModule import (
    AppNetworkEventType,
    Component,
    NetworkBufferData,
    OpenAppSerializable,
    NetworkServiceType,
    NetworkConfig,
    NetworkSocketType,
    OpenAppResponseType,
)


class DisplayImageComponent(Component):
    def __init__(self):
        super().__init__()
        self.client_id = -1
        self.server_port = None

    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        return (
            self.RegisterOpenAPI("/display_image/startserver", ["POST"])
            and self.RegisterOpenAPI("/display_image/stopserver", ["POST"])
        )

    def Finalize(self):
        self.client_id = -1
        self.server_port = None
        return True

    @staticmethod
    def _respond(request, code, payload, response_type=OpenAppResponseType.JSON):
        request.SetStatusCode(code)
        body = payload if isinstance(payload, str) else json.dumps(payload)
        request.SetResponseBody(body, response_type)
        return True

    def _image_path(self):
        candidates = []
        model_path = self.GetModelPath()
        if model_path:
            candidates.append(os.path.abspath(os.path.join(model_path, "..", "image.jpg")))
        candidates.append(os.path.abspath(os.path.join(os.getcwd(), "../res/image.jpg")))
        candidates.append(os.path.abspath(os.path.join(os.getcwd(), "res/image.jpg")))

        for path in candidates:
            if os.path.isfile(path):
                return path
        return None

    def _http_image(self):
        image_path = self._image_path()
        if image_path is None:
            raise RuntimeError("JPEG image asset is unavailable at ../res/image.jpg")

        try:
            with open(image_path, "rb") as image_file:
                image = image_file.read()
        except OSError as error:
            raise RuntimeError("JPEG image asset cannot be read: %s" % error)
        if not image:
            raise RuntimeError("JPEG image asset is empty: %s" % image_path)

        header = (
            b"HTTP/1.1 200 OK\r\n"
            + b"Content-Length: "
            + str(len(image)).encode("ascii")
            + b"\r\nContent-Type: image/jpeg\r\n"
            + b"Cross-Origin-Resource-Policy: cross-origin\r\n\r\n"
        )
        return header + image

    def _start_server(self, body):
        port_value = body.get("port")
        if isinstance(port_value, bool):
            raise ValueError("invalid field: port")
        try:
            port = int(port_value)
        except (TypeError, ValueError):
            raise ValueError("invalid field: port")
        if port < 1 or port > 65535:
            raise ValueError("invalid field: port")

        # Check the effect's input before asking Network Manager to start.
        self._http_image()
        config = NetworkConfig(
            int(NetworkServiceType.eServer),
            int(NetworkSocketType.eTCP),
            "",
            port,
        )
        if not self.SendNoReplyEvent(
            "AppNetworkManager", int(AppNetworkEventType.eStartService), 0, config
        ):
            raise RuntimeError("Network Manager rejected the TCP server request")
        self.server_port = port

    def _stop_server(self):
        if not self.SendNoReplyEvent(
            "AppNetworkManager", int(AppNetworkEventType.eStopService), 0, None
        ):
            raise RuntimeError("Network Manager rejected the TCP stop request")
        self.client_id = -1
        self.server_port = None

    def _send_image(self):
        if self.client_id <= 0:
            raise RuntimeError("TCP client is unavailable")
        response = self._http_image()
        data = NetworkBufferData(response, len(response), self.client_id)
        if not self.SendNoReplyEvent(
            "AppNetworkManager", int(AppNetworkEventType.eSendData), 0, data
        ):
            raise RuntimeError("Network Manager rejected the JPEG buffer")

    def ProcessAEvent(self, event):
        if event is None:
            return False
        if event.IsReply():
            return True

        event_type = event.GetType()
        if event_type == int(AppNetworkEventType.eNewClientConnected):
            self.client_id = int(event.GetArgument())
            if self.client_id <= 0:
                raise RuntimeError("Network Manager returned an invalid TCP client")
            self._send_image()
            return True

        if event_type == int(AppNetworkEventType.eClientDisconnected):
            if int(event.GetArgument()) == self.client_id:
                self.client_id = -1
            return True

        if event_type in (
            int(AppNetworkEventType.eClientData),
            int(AppNetworkEventType.eServerData),
        ):
            return True

        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False

        path = request.GetFCGXParam("PATH_INFO")
        if path not in ("/display_image/startserver", "/display_image/stopserver"):
            return False
        if request.GetMethod() != "POST":
            return self._respond(request, 405, "Method Not Allowed", OpenAppResponseType.TEXT)

        try:
            body = json.loads(request.GetRequestBody() or "{}")
            if not isinstance(body, dict):
                raise ValueError("request body must be an object")
            if path.endswith("/startserver"):
                self._start_server(body)
                return self._respond(
                    request,
                    202,
                    {
                        "status": "accepted",
                        "effect": "queued_no_native_reply",
                        "service": "tcp_server",
                        "port": self.server_port,
                    },
                )
            self._stop_server()
            return self._respond(
                request,
                202,
                {"status": "accepted", "effect": "queued_no_native_reply", "service": "stopped"},
            )
        except json.JSONDecodeError:
            return self._respond(request, 400, "request body parse error", OpenAppResponseType.TEXT)
        except ValueError as error:
            return self._respond(request, 400, str(error), OpenAppResponseType.TEXT)
        except RuntimeError as error:
            return self._respond(request, 503, {"status": "unavailable", "reason": str(error)})
