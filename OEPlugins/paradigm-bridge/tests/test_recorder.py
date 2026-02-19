import requests

from paradigm_bridge.recorder import RecordingController


def test_get_retries_then_succeeds(monkeypatch):
    controller = RecordingController(max_retries=3, timeout=0.01)
    calls = {"count": 0}

    class Response:
        def raise_for_status(self):
            return None

        def json(self):
            return {"mode": "IDLE"}

    def fake_get(url, timeout):
        calls["count"] += 1
        if calls["count"] < 3:
            raise requests.exceptions.ConnectionError("offline")
        return Response()

    monkeypatch.setattr(requests, "get", fake_get)

    result = controller._get("/api/status")
    assert result["mode"] == "IDLE"
    assert calls["count"] == 3


def test_start_recording_sends_name_then_record(monkeypatch):
    controller = RecordingController(max_retries=1, timeout=0.01)
    put_calls = []

    class Response:
        def __init__(self, payload):
            self.payload = payload

        def raise_for_status(self):
            return None

        def json(self):
            if self.payload == {"mode": "RECORD"}:
                return {"mode": "RECORD"}
            return {}

    def fake_put(url, json, timeout):
        put_calls.append((url, json))
        return Response(json)

    monkeypatch.setattr(requests, "put", fake_put)

    mode = controller.start_recording("session_01")
    assert mode == "RECORD"
    assert put_calls[0][1] == {"prepend_text": "session_01"}
    assert put_calls[1][1] == {"mode": "RECORD"}
