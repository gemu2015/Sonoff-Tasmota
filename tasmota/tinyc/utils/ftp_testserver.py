#!/usr/bin/env python3
"""ftp_testserver.py -- a tiny FTP server for exercising TinyC's ftpPut/ftpPutStr.

    python3 ftp_testserver.py [--port 2121] [--user esp] [--pw geheim] [--root ./ftp_root]

Speaks exactly the subset the client uses -- USER/PASS, SYST, TYPE, PWD, CWD,
PASV, STOR, APPE, NOOP, QUIT -- and greets with a MULTI-LINE 220, as FRITZ!OS
does, so the client's reply parser is tested on the desk before a NAS is
involved. Every command and every stored byte count is printed. Files land
under --root; a path like /USB-Speicher/esp/klima.csv becomes
<root>/USB-Speicher/esp/klima.csv (directories are created).

⚠️ No security whatsoever: one fixed user, plain text, no chroot escape check
beyond stripping "..". Run it on the LAN for a test, then stop it.
"""
import argparse, os, socket, threading, sys

def handle(conn, addr, cfg):
    def send(s):
        conn.sendall((s + "\r\n").encode()); print("  >", s, flush=True)
    f = conn.makefile("rb")
    send("220-ftp_testserver ready")
    send("220-second greeting line, like FRITZ!OS")
    send("220 go ahead")
    user_ok = False; logged = False; pasv = None; cwd = "/"
    while True:
        raw = f.readline()
        if not raw: break
        line = raw.decode(errors="replace").rstrip("\r\n")
        print(f"{addr[0]} <", line if not line.upper().startswith("PASS") else "PASS ****", flush=True)
        cmd, _, arg = line.partition(" ")
        cmd = cmd.upper()
        if cmd == "USER":
            user_ok = (arg == cfg.user); send("331 password please")
        elif cmd == "PASS":
            logged = user_ok and arg == cfg.pw
            send("230 logged in" if logged else "530 login incorrect")
        elif not logged:
            send("530 please log in")
        elif cmd == "SYST": send("215 UNIX Type: L8")
        elif cmd == "TYPE": send("200 type set")
        elif cmd == "NOOP": send("200 ok")
        elif cmd == "PWD":  send(f'257 "{cwd}"')
        elif cmd == "CWD":  cwd = arg or "/"; send("250 ok")
        elif cmd == "PASV":
            ds = socket.socket(); ds.bind((cfg.bind, 0)); ds.listen(1); ds.settimeout(10)
            ip = conn.getsockname()[0]; port = ds.getsockname()[1]
            pasv = ds
            send("227 Entering Passive Mode (%s,%d,%d)" % (ip.replace(".", ","), port >> 8, port & 255))
        elif cmd in ("STOR", "APPE"):
            if not pasv: send("425 use PASV first"); continue
            rel = arg.lstrip("/").replace("..", "")
            path = os.path.join(cfg.root, rel)
            os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
            send("150 ok, send it")
            try:
                dc, _ = pasv.accept()
            except socket.timeout:
                send("425 no data connection"); pasv.close(); pasv = None; continue
            n = 0
            with open(path, "ab" if cmd == "APPE" else "wb") as out:
                while True:
                    chunk = dc.recv(4096)
                    if not chunk: break
                    out.write(chunk); n += len(chunk)
            dc.close(); pasv.close(); pasv = None
            print(f"  = {cmd} {path}: {n} bytes", flush=True)
            send("226 transfer complete")
        elif cmd == "QUIT":
            send("221 bye"); break
        else:
            send("502 not implemented")
    conn.close()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=2121)
    ap.add_argument("--user", default="esp")
    ap.add_argument("--pw", default="geheim")
    ap.add_argument("--root", default="./ftp_root")
    ap.add_argument("--bind", default="0.0.0.0")
    cfg = ap.parse_args()
    os.makedirs(cfg.root, exist_ok=True)
    srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((cfg.bind, cfg.port)); srv.listen(5)
    print(f"ftp_testserver on port {cfg.port}, user {cfg.user}, root {os.path.abspath(cfg.root)}", flush=True)
    while True:
        conn, addr = srv.accept()
        threading.Thread(target=handle, args=(conn, addr, cfg), daemon=True).start()

if __name__ == "__main__":
    try: main()
    except KeyboardInterrupt: sys.exit(0)
