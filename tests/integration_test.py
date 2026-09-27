"""Real loopback sockets and subprocesses; no third-party Python packages."""
import concurrent.futures
import contextlib
import json
import os
from pathlib import Path
import queue
import re
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest

SERVER, CLIENT = sys.argv[1:3]
del sys.argv[1:3]
MAX_MESSAGE = 1024 * 1024


def exact(sock, size):
    result = bytearray()
    while len(result) < size:
        chunk = sock.recv(size - len(result))
        if not chunk:
            raise EOFError("Peer closed during frame")
        result.extend(chunk)
    return bytes(result)


def frame(payload):
    return struct.pack("!I", len(payload)) + payload


def read_frame(sock):
    size, = struct.unpack("!I", exact(sock, 4))
    if size > MAX_MESSAGE:
        raise ValueError("Oversized reply")
    return exact(sock, size)


def connected(port):
    return socket.create_connection(("127.0.0.1", port), timeout=3)


def closed(sock):
    try:
        return sock.recv(1) == b""
    except ConnectionResetError:
        return True


class Server:
    def __init__(self, port=0, max_connections=5, timeout=1000):
        self.temp = tempfile.TemporaryDirectory()
        config = Path(self.temp.name) / "server.json"
        config.write_text(json.dumps({"maxConnectionLimit": max_connections,
                                      "timeoutDuration": timeout}))
        self.errors = tempfile.TemporaryFile(mode="w+t")
        try:
            self.process = subprocess.Popen([SERVER, str(port), str(config)],
                                            stdout=subprocess.PIPE, stderr=self.errors, text=True)
        except BaseException:
            self.errors.close()
            self.temp.cleanup()
            raise
        ready = queue.Queue()
        threading.Thread(target=lambda: ready.put(self.process.stdout.readline()), daemon=True).start()
        try:
            line = ready.get(timeout=5)
            match = re.search(r"0\.0\.0\.0:(\d+)", line)
            if not match:
                self.errors.seek(0)
                raise RuntimeError("Server did not start: " + self.errors.read())
            self.port = int(match.group(1))
        except BaseException:
            self.close()
            raise

    def close(self):
        if getattr(self, "_closed", False):
            return
        self._closed = True
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=3)
        self.process.stdout.close()
        self.errors.seek(0)
        diagnostics = self.errors.read()
        self.errors.close()
        self.temp.cleanup()
        if os.name != "nt" and self.process.returncode != 0:
            raise AssertionError("Server failed during shutdown: " + diagnostics)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class Client:
    def __init__(self, port, count=5, interval=0, size=64, timeout=500):
        self.temp = tempfile.TemporaryDirectory()
        self.error_path = Path(self.temp.name) / "client.log"
        self.errors = self.error_path.open("w+t")
        try:
            self.process = subprocess.Popen(
                [CLIENT, "127.0.0.1", str(port), str(count), str(interval), str(size), str(timeout)],
                stdout=subprocess.PIPE, stderr=self.errors, text=True)
        except BaseException:
            self.errors.close()
            self.temp.cleanup()
            raise

    def wait_log(self, text, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            # A separate handle keeps polling from moving the child's write offset.
            output = self.error_path.read_text()
            if text in output:
                return
            if self.process.poll() is not None:
                raise AssertionError("Client exited before log appeared: " + output)
            time.sleep(.01)
        raise AssertionError("Missing client log: " + text)

    def result(self, timeout=8):
        output, _ = self.process.communicate(timeout=timeout)
        if self.process.returncode:
            raise AssertionError(self.error_path.read_text())
        return json.loads(output)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=3)
        self.process.stdout.close()
        self.errors.close()
        self.temp.cleanup()


@contextlib.contextmanager
def fake_server(handler):
    """Independent wire implementation, with worker exceptions propagated."""
    with socket.socket() as listener, concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(5)
        future = pool.submit(handler, listener)
        try:
            yield listener.getsockname()[1]
        finally:
            future.result(timeout=6)


class IntegrationTests(unittest.TestCase):
    def test_fragmented_and_coalesced_frames(self):
        with Server() as server, connected(server.port) as sock:
            payload = b"binary\x00payload\xff"
            packet = frame(payload)
            for byte in packet[:4]:
                sock.sendall(bytes([byte]))
                time.sleep(.005)
            sock.sendall(packet[4:7])
            sock.sendall(packet[7:])
            self.assertEqual(read_frame(sock), payload)
            sock.sendall(frame(b"first") + frame(b"") + frame(b"third"))
            self.assertEqual([read_frame(sock) for _ in range(3)], [b"first", b"", b"third"])

    def test_oversized_header_and_maximum_payload(self):
        with Server(timeout=3000) as server:
            for size in [MAX_MESSAGE + 1, 0xFFFFFFFF]:
                with connected(server.port) as sock:
                    sock.sendall(struct.pack("!I", size))
                    self.assertTrue(closed(sock))
            with connected(server.port) as sock:
                payload = bytes(range(256)) * (MAX_MESSAGE // 256)
                sock.sendall(frame(payload))
                self.assertEqual(read_frame(sock), payload)

    def test_multiple_clients_connection_limit_and_slot_reuse(self):
        with Server(max_connections=2, timeout=3000) as server:
            with connected(server.port) as first, connected(server.port) as second:
                for sock in [first, second]:
                    sock.sendall(frame(b"ready"))
                    self.assertEqual(read_frame(sock), b"ready")
                with connected(server.port) as third:
                    self.assertTrue(closed(third))
                first.close()
                deadline = time.monotonic() + 2
                while True:
                    with connected(server.port) as replacement:
                        try:
                            replacement.sendall(frame(b"replacement"))
                            self.assertEqual(read_frame(replacement), b"replacement")
                            break
                        except (EOFError, ConnectionError):
                            if time.monotonic() > deadline:
                                raise
                    time.sleep(.01)
                second.sendall(frame(b"still-alive"))
                self.assertEqual(read_frame(second), b"still-alive")

    def test_partial_header_and_body_timeout(self):
        with Server(timeout=150) as server:
            for partial in [b"\x00", struct.pack("!I", 10) + b"a"]:
                with connected(server.port) as sock:
                    started = time.monotonic()
                    sock.sendall(partial)
                    self.assertTrue(closed(sock))
                    self.assertLess(time.monotonic() - started, 2)
            with connected(server.port) as sock:
                sock.sendall(frame(b"healthy"))
                self.assertEqual(read_frame(sock), b"healthy")

    def test_client_handles_fragmented_replies(self):
        def handler(listener):
            with listener.accept()[0] as sock:
                sock.settimeout(3)
                for _ in range(3):
                    payload = read_frame(sock)
                    header = frame(payload)[:4]
                    for byte in header:
                        sock.sendall(bytes([byte]))
                        time.sleep(.005)
                    sock.sendall(payload[:7])
                    sock.sendall(payload[7:])
        with fake_server(handler) as port, Client(port, count=3) as client:
            result = client.result()
            self.assertEqual(result["messages"], 3)
            self.assertEqual(result["failures"], 0)
            latency = result["latency_ms"]
            self.assertEqual(latency["sample_count"], 3)
            self.assertGreater(latency["min"], 0)
            self.assertLessEqual(latency["p50"], latency["p95"])
            self.assertLessEqual(latency["p95"], latency["p99"])

    def test_client_started_before_server(self):
        with socket.socket() as reserved:
            reserved.bind(("127.0.0.1", 0))
            port = reserved.getsockname()[1]
            with Client(port) as client:
                client.wait_log("retrying")
                reserved.close()
                with Server(port=port):
                    result = client.result()
                    self.assertEqual(result["messages"], 5)
                    self.assertGreaterEqual(result["failures"], 1)

    def test_client_recovers_after_server_restart(self):
        server = Server(timeout=3000)
        try:
            port = server.port
            with Client(port, count=100, interval=20) as client:
                client.wait_log("Connected")
                server.close()
                server = None
                client.wait_log("retrying")
                with Server(port=port, timeout=3000):
                    result = client.result()
                    self.assertEqual(result["messages"], 100)
                    self.assertGreaterEqual(result["reconnects"], 1)
                    self.assertGreaterEqual(result["failures"], 1)
        finally:
            if server is not None:
                server.close()

    def test_silent_peer_timeout_and_retry(self):
        def handler(listener):
            with listener.accept()[0] as first:
                first.settimeout(3)
                read_frame(first)
                self.assertTrue(closed(first))  # Client must time out while awaiting the echo.
            with listener.accept()[0] as second:
                second.settimeout(3)
                second.sendall(frame(read_frame(second)))
        with fake_server(handler) as port, Client(port, count=1, timeout=150) as client:
            result = client.result()
            self.assertEqual(result["messages"], 1)
            self.assertGreaterEqual(result["failures"], 1)
            self.assertGreaterEqual(result["reconnects"], 1)

    def test_empty_client_payload(self):
        with Server() as server, Client(server.port, count=3, size=0) as client:
            self.assertEqual(client.result()["messages"], 3)

    def test_invalid_arguments_and_configuration(self):
        for arguments in [["127.0.0.1", "-1"], ["127.0.0.1", "70000"],
                          ["127.0.0.1", "123x"], ["127.0.0.1", "0"],
                          ["127.0.0.1", "12345", "1", "0", "64", "0"]]:
            result = subprocess.run([CLIENT] + arguments, capture_output=True, text=True, timeout=3)
            self.assertNotEqual(result.returncode, 0)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "invalid.json"
            for config in ["[]", "{", '{"maxConnectionLimit":0}',
                           '{"timeoutDuration":-1}', '{"timeoutDuration":1.5}',
                           '{"timeoutDuration":18446744073709551615}']:
                path.write_text(config)
                result = subprocess.run([SERVER, "0", str(path)], capture_output=True,
                                        text=True, timeout=3)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Server error:", result.stderr)
            path.unlink()
            result = subprocess.run([SERVER, "0", str(path)], capture_output=True, text=True, timeout=3)
            self.assertNotEqual(result.returncode, 0)

    @unittest.skipIf(os.name == "nt", "Windows terminate() does not send a POSIX signal")
    def test_interrupt_cancels_reconnect_and_prints_statistics(self):
        with socket.socket() as reserved:
            reserved.bind(("127.0.0.1", 0))
            with Client(reserved.getsockname()[1], count=0) as client:
                client.wait_log("retrying")
                client.process.send_signal(signal.SIGINT)
                result = client.result()
                self.assertEqual(result["messages"], 0)
                self.assertEqual(result["latency_ms"]["sample_count"], 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
