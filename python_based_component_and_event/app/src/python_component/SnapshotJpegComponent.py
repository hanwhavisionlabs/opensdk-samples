# Request a camera JPEG snapshot through the native OpenPlatform event.
import json
import os

from PyComponentModule import Component, OpenAppResponseType, OpenAppSerializable, OpenPlatformAppEventType, String


class SnapshotJpegComponent(Component):
    def __init__(self):
        super().__init__()
        self.app_id = os.getenv("WISENET_APP_ID", "python_based_component_and_event")

    def PreInitialize(self): return True
    def Initialize(self): return True
    def PostInitialize(self): return self.RegisterOpenAPI("/snapshot_jpeg/configuration", ["POST"])

    def ProcessAEvent(self, event):
        if event is None:
            return False
        info_event = getattr(__import__("PyComponentModule"), "LifeCycleManagerOpenAppEventType", None)
        if info_event is not None and event.GetType() == int(info_event.eInformAppInfo):
            try:
                info = String("")
                info.DeserializeBaseObject(event.GetBlobArgument())
                self.app_id = json.loads(info.c_str()).get("AppId", self.app_id)
            except (ValueError, TypeError, json.JSONDecodeError):
                pass
            return True
        if event.IsReply():
            return True
        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None or request.GetObjectName() != "OpenAppSerializable":
            return False
        if request.GetFCGXParam("PATH_INFO") != "/snapshot_jpeg/configuration":
            return False
        if request.GetMethod() != "POST":
            request.SetStatusCode(405)
            request.SetResponseBody("Method Not Allowed", OpenAppResponseType.TEXT)
            return True
        try:
            body = json.loads(request.GetRequestBody() or "{}")
            path = body.get("jpeg_path", "../res/test.jpg")
            if not isinstance(path, str) or not path or path.startswith("/") or ".." not in path:
                raise ValueError("jpeg_path must be a relative app path")
            payload = json.dumps({"jpeg_path": path, "channel": "0", "app_name": self.app_id})
            reply = self.SendReplyEventWait("OpenPlatform", int(OpenPlatformAppEventType.eAppSnapshotJpeg), 0, String(payload))
            if reply is None:
                raise RuntimeError("snapshot request was not completed")
            full_path = os.path.abspath(os.path.join(os.getcwd(), path))
            with open(full_path, "rb") as image_file:
                image = image_file.read()
            request.SetStatusCode(200)
            request.SetResponseBodyBytes(image, OpenAppResponseType.FILE)
            request.AddResponseHeader("Content-type", "image/jpeg")
            return True
        except (json.JSONDecodeError, ValueError) as error:
            request.SetStatusCode(400)
            request.SetResponseBody(str(error), OpenAppResponseType.TEXT)
            return True
        except (OSError, RuntimeError) as error:
            request.SetStatusCode(503)
            request.SetResponseBody(json.dumps({"status": "unavailable", "reason": str(error)}), OpenAppResponseType.JSON)
            return True

    def Finalize(self): return True
