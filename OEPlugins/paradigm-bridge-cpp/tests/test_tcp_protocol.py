import os
import socket

import pytest


HOST = os.getenv("PARADIGM_BRIDGE_TEST_HOST")
PORT = int(os.getenv("PARADIGM_BRIDGE_TEST_PORT", "0"))
AUTH_TOKEN = os.getenv("PARADIGM_BRIDGE_TEST_TOKEN")


def _send(sock: socket.socket, command: str) -> str:
    sock.sendall((command + "\n").encode("utf-8"))
    return sock.recv(4096).decode("utf-8").strip()


@pytest.fixture()
def bridge_socket():
    if not HOST or not PORT:
        pytest.skip("Set PARADIGM_BRIDGE_TEST_HOST and PARADIGM_BRIDGE_TEST_PORT to run integration tests.")

    with socket.create_connection((HOST, PORT), timeout=2.0) as sock:
        if AUTH_TOKEN:
            assert _send(sock, f"AUTH {AUTH_TOKEN}").startswith("OK AUTH")
        yield sock


def test_ping(bridge_socket):
    assert _send(bridge_socket, "PING") == "OK PONG"


def test_status(bridge_socket):
    response = _send(bridge_socket, "STATUS")
    assert response.startswith("OK ")
    assert "ACQUISITION=" in response
    assert "RECORDING=" in response


def test_trigger_validation(bridge_socket):
    assert _send(bridge_socket, "TRIGGER 8 1") == "ERROR line must be 0-7"
    assert _send(bridge_socket, "TRIGGER x 1") == "ERROR line must be an integer"
    assert _send(bridge_socket, "TRIGGER 1 2") == "ERROR state must be 0 or 1"
