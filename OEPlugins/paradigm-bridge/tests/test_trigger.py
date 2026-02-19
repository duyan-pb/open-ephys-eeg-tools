from paradigm_bridge.trigger import TriggerManager


class FakeSocket:
    def __init__(self):
        self.RCVTIMEO = None
        self.connected_url = None
        self.sent = []
        self.closed = False

    def connect(self, url):
        self.connected_url = url

    def send_string(self, message):
        self.sent.append(message)

    def recv(self):
        return b"ok"

    def close(self):
        self.closed = True


class FakeContext:
    def __init__(self):
        self.socket_instance = FakeSocket()
        self.terminated = False

    def socket(self, kind):
        return self.socket_instance

    def term(self):
        self.terminated = True


class FakeZmqModule:
    REQ = 1

    def __init__(self):
        self.context_instance = FakeContext()

    def Context(self):
        return self.context_instance


def test_send_and_pulse(monkeypatch):
    fake_zmq = FakeZmqModule()

    import paradigm_bridge.trigger as trigger_module
    monkeypatch.setattr(trigger_module, "_ensure_zmq", lambda: fake_zmq)

    tm = TriggerManager(address="127.0.0.1", port=5556, recv_timeout_ms=123)
    assert tm._socket.connected_url == "tcp://127.0.0.1:5556"
    assert tm._socket.RCVTIMEO == 123

    assert tm.send(line=3, state=1) == "ok"
    tm.pulse(line=7, duration_ms=0.0)

    assert tm._socket.sent[0] == "TTL Line=3 State=1"
    assert tm._socket.sent[1] == "TTL Line=7 State=1"
    assert tm._socket.sent[2] == "TTL Line=7 State=0"

    tm.close()
    assert fake_zmq.context_instance.terminated is True
    assert tm._socket.closed is True
