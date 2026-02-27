from paradigm_bridge.tcp_trigger import TcpTriggerClient


class FakeSocket:
    def __init__(self, responses):
        self._responses = [r.encode("utf-8") for r in responses]
        self.sent = []
        self.connected_to = None
        self.timeout = None
        self.closed = False

    def settimeout(self, timeout):
        self.timeout = timeout

    def connect(self, addr):
        self.connected_to = addr

    def sendall(self, data):
        self.sent.append(data.decode("utf-8"))

    def recv(self, _n):
        if not self._responses:
            return b""
        return self._responses.pop(0)

    def close(self):
        self.closed = True


def test_send_point_uses_server_pulse(monkeypatch):
    fake = FakeSocket(["OK PULSE 2\n"])

    import paradigm_bridge.tcp_trigger as tcp_mod
    monkeypatch.setattr(tcp_mod.socket, "socket", lambda *args, **kwargs: fake)

    client = TcpTriggerClient(auto_connect=True)
    resp = client.send_point(line=2)

    assert resp == "OK PULSE 2"
    assert fake.connected_to == ("127.0.0.1", 5557)
    assert fake.sent[-1] == "PULSE 2\n"
    client.close()
    assert fake.closed is True


def test_send_point_falls_back_on_old_server(monkeypatch):
    fake = FakeSocket([
        "ERROR unknown command: PULSE 2\n",
        "OK TRIGGER 2 1\n",
        "OK TRIGGER 2 0\n",
    ])

    import paradigm_bridge.tcp_trigger as tcp_mod
    monkeypatch.setattr(tcp_mod.socket, "socket", lambda *args, **kwargs: fake)

    client = TcpTriggerClient(auto_connect=True)
    resp = client.send_point(line=2)

    assert resp == "OK TRIGGER 2 0"
    assert fake.sent[0] == "PULSE 2\n"
    assert fake.sent[1] == "TRIGGER 2 1\n"
    assert fake.sent[2] == "TRIGGER 2 0\n"
    client.close()
