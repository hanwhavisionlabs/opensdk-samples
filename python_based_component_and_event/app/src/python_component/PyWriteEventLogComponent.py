# Send an event log through the native LogManager event transport.
from PyComponentModule import Component, OpenAppResponseType, OpenAppSerializable, Log, LogType, LogDetailType, LogManagerEvent

import json


class PyWriteEventLogComponent(Component):
    def PreInitialize(self):
        return True

    def Initialize(self):
        return True

    def PostInitialize(self):
        self.RegisterOpenAPI("/python/write_event_log", ["POST"])
        return True

    def Finalize(self):
        return True

    def ProcessAEvent(self, event):
        if event is None:
            return False

        if event.IsReply():
            return True

        request = event.GetAttachmentPtr(OpenAppSerializable)
        if request is None:
            return False

        if request.GetObjectName() != "OpenAppSerializable":
            return False

        if request.GetFCGXParam("PATH_INFO") != "/python/write_event_log":
            return False

        if request.GetMethod() != "POST":
            request.SetStatusCode(400)
            request.SetResponseBody("requested method is not supported", OpenAppResponseType.TEXT)
            return True

        try:
            body = json.loads(request.GetRequestBody())
            message = body["log"]
        except (ValueError, KeyError, TypeError):
            request.SetStatusCode(400)
            request.SetResponseBody("request body parse error", OpenAppResponseType.TEXT)
            return True

        # Ownership transfers to native event transport; the payload must not
        # be reused after this call because serialization may be deferred.
        log = Log(LogType.EVENT_LOG, LogDetailType.EVENT_OPENAPP, 0, str(message))
        if not self.SendNoReplyEvent("LogManager", int(LogManagerEvent.eWrite), 0, log):
            request.SetStatusCode(503)
            request.SetResponseBody('{"result":"unavailable"}', OpenAppResponseType.JSON)
            return True

        request.SetStatusCode(200)
        request.SetResponseBody('{"result":"ok"}', OpenAppResponseType.JSON)
        return True
