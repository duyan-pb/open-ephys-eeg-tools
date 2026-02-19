from paradigm_bridge.bridge import ParadigmBridge


def test_send_trigger_without_trigger_manager_returns_none(monkeypatch):
    class FakeRecorder:
        def is_connected(self):
            return True

    monkeypatch.setattr("paradigm_bridge.bridge.RecordingController", lambda **kwargs: FakeRecorder())
    monkeypatch.setattr("paradigm_bridge.bridge.TriggerManager", lambda **kwargs: (_ for _ in ()).throw(RuntimeError("no zmq")))

    bridge = ParadigmBridge(enable_triggers=True)
    assert bridge.triggers is None
    assert bridge.send_trigger(line=1, state=1) is None
