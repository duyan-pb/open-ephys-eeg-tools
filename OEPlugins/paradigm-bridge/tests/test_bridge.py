from paradigm_bridge.bridge import ParadigmBridge


def test_send_trigger_without_trigger_manager_returns_none(monkeypatch):
    """When the TCP connection fails, triggers should gracefully degrade to None."""

    class FakeRecorder:
        def is_connected(self):
            return True

    monkeypatch.setattr(
        "paradigm_bridge.bridge.RecordingController",
        lambda **kwargs: FakeRecorder(),
    )
    monkeypatch.setattr(
        "paradigm_bridge.bridge.TcpTriggerClient",
        lambda **kwargs: (_ for _ in ()).throw(
            ConnectionError("connection refused")
        ),
    )

    bridge = ParadigmBridge(trigger_backend="tcp")
    assert bridge.triggers is None
    assert bridge.send_trigger(line=1, state=1) is None
