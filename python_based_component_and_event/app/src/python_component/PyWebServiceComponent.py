# Copyright (c) 2026 Hanwha Vision Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

# Python equivalent of webservice_sample's custom SOAP endpoint registration.
import base64
import json
from pathlib import Path
from urllib.parse import urlsplit

from PyComponentModule import Component, HttpAuthMethod, OpenAppResponseType, OpenAppSerializable


DEFAULT_ENDPOINT = "/SampleWebService/GetImage"
REGISTRATION_ENDPOINT = "/python/webservice/register"
JPEG_PATH = Path("../res/image.jpg")
RESERVED_ENDPOINTS = {"/opensdk", "/home/setup/opensdk/html"}
RESERVED_PREFIXES = ("/opensdk/", "/home/setup/opensdk/html/")
PROTECTED_ENDPOINTS = {
    "/configuration",
    "/home/status",
    "/python/health",
    "/python/write_event_log",
    "/python/analytics",
    "/display_image/startserver",
    "/display_image/stopserver",
    "/event_trigger/configuration",
    "/metadata_sample/status",
    "/send_metadata/configuration",
    "/dynamic_event/configuration",
    "/set_osd_message/configuration",
    "/snapshot_jpeg/configuration",
    REGISTRATION_ENDPOINT,
}
PROTECTED_PREFIXES = ("/sdcard_output_event/",)


class PyWebServiceComponent(Component):
    def __init__(self):
        super().__init__()
        self.registered_endpoints = set()

    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        registration_ready = self._register_endpoint(REGISTRATION_ENDPOINT)
        default_ready = self._register_endpoint(DEFAULT_ENDPOINT, custom=True)
        return registration_ready and default_ready

    def Finalize(self):
        return True

    def ProcessAEvent(self, event):
        if event is None:
            return False

        if event.IsReply():
            return True

        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False

        path = request.GetFCGXParam("PATH_INFO")
        if path == REGISTRATION_ENDPOINT:
            return self._handle_registration(request)
        if path not in self.registered_endpoints:
            return self._unsupported(request)
        if request.GetMethod() != "POST":
            return self._unsupported(request)

        request.SetStatusCode(200)
        request.SetResponseBody(self._build_response_xml(), OpenAppResponseType.XML)
        request.AddResponseHeader("Content-type", "text/xml; charset=utf-8")
        return True

    def _register_endpoint(self, endpoint, custom=False):
        if endpoint in self.registered_endpoints:
            return True

        if custom:
            registered = self.RegisterOpenAPI(
                endpoint,
                ["POST"],
                custom=True,
                auth_type=int(HttpAuthMethod.kDigest),
            )
        else:
            registered = self.RegisterOpenAPI(endpoint, ["POST"])
        if registered:
            self.registered_endpoints.add(endpoint)
        return registered

    def _handle_registration(self, request):
        if request.GetMethod() != "POST":
            return self._unsupported(request)

        try:
            payload = json.loads(request.GetRequestBody() or "{}")
            if not isinstance(payload, dict):
                raise ValueError("request body must be an object")
            endpoint = self._normalize_endpoint(payload.get("endpoint"))
            if endpoint == REGISTRATION_ENDPOINT:
                raise ValueError("endpoint is reserved")
        except (json.JSONDecodeError, TypeError, ValueError) as error:
            request.SetStatusCode(400)
            request.SetResponseBody(str(error), OpenAppResponseType.TEXT)
            return True

        if endpoint in self.registered_endpoints:
            result = {"status": "already_registered", "endpoint": endpoint}
            request.SetStatusCode(200)
        elif not self._register_endpoint(endpoint, custom=True):
            result = {"status": "unavailable", "endpoint": endpoint}
            request.SetStatusCode(503)
        else:
            result = {"status": "accepted", "endpoint": endpoint}
            request.SetStatusCode(202)
        request.SetResponseBody(json.dumps(result), OpenAppResponseType.JSON)
        return True

    @staticmethod
    def _normalize_endpoint(value):
        if not isinstance(value, str):
            raise ValueError("endpoint must be a path")

        endpoint = value
        parsed = urlsplit(endpoint)
        if (
            not endpoint
            or len(endpoint) > 512
            or not endpoint.startswith("/")
            or endpoint.startswith("//")
            or parsed.scheme
            or parsed.netloc
            or parsed.path != endpoint
            or any(ord(char) < 0x20 or ord(char) == 0x7F for char in endpoint)
            or any(char.isspace() for char in endpoint)
            or any(part in (".", "..") for part in endpoint.split("/"))
            or endpoint in RESERVED_ENDPOINTS
            or any(endpoint.startswith(prefix) for prefix in RESERVED_PREFIXES)
            or (endpoint in PROTECTED_ENDPOINTS and endpoint != DEFAULT_ENDPOINT)
            or any(endpoint.startswith(prefix) for prefix in PROTECTED_PREFIXES)
        ):
            raise ValueError("endpoint must be a non-reserved absolute path without query or fragment")
        return endpoint

    @staticmethod
    def _unsupported(request):
        request.SetStatusCode(400)
        request.SetResponseBody("requested path is not supported", OpenAppResponseType.TEXT)
        return False

    @staticmethod
    def _build_response_xml():
        try:
            image = base64.b64encode(JPEG_PATH.read_bytes()).decode("ascii")
        except OSError:
            image = ""

        return f'''<?xml version="1.0" encoding="UTF-8"?>
<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/" xmlns:smp="urn:example:opensdk:webservice-sample">
  <soapenv:Body>
    <smp:GetImageResponse>
      <smp:Base64String>{image}</smp:Base64String>
    </smp:GetImageResponse>
  </soapenv:Body>
</soapenv:Envelope>
'''
