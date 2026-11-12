"""Doc 78, M0a: runs inside a container; answers every TCP connection on 17789 (IPv4, and IPv6 if the container has
it) with the peer address it sees, as one line of plain HTTP."""
import socket
import threading


def serve(family, addr):
    s = socket.socket(family, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if family == socket.AF_INET6:
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
    s.bind((addr, 17789))
    s.listen(16)
    while True:
        c, peer = s.accept()
        try:
            c.recv(1024)
            body = ('peer=%s\n' % peer[0]).encode()
            c.sendall(b'HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n' % len(body) + body)
        finally:
            c.close()


threading.Thread(target=serve, args=(socket.AF_INET, '0.0.0.0'), daemon=True).start()
try:
    serve(socket.AF_INET6, '::')
except OSError as error:
    print('no IPv6 listener in the container:', error, flush=True)
    threading.Event().wait()
