#!/usr/bin/env python3
"""Monitor TinyC/Scripter UDP multicast variables on 239.255.255.250:1999
Usage: python3 udp_monitor.py [seconds]   (default: 60, Ctrl-C to stop early)
"""

import socket
import struct
import sys
import time
import os
import csv
import json
from collections import OrderedDict
from urllib.request import urlopen
from urllib.error import URLError
from concurrent.futures import ThreadPoolExecutor, as_completed

MCAST_GRP = '239.255.255.250'
MCAST_PORT = 1999

def parse_packet(data):
    """Parse one UDP packet, yield (name, value_str, payload_type) tuples."""
    text = data.decode('latin-1')
    lines = text.split('\n')
    for line in lines:
        line = line.strip()
        if not line:
            continue
        if line.startswith('=>'):
            line = line[2:]
        # Find delimiter: '=' (ASCII) or ':' (binary)
        eq = line.find('=')
        col = line.find(':')
        if eq >= 0 and (col < 0 or eq < col):
            name = line[:eq]
            val = line[eq+1:]
            yield (name, val, 'ascii')
        elif col >= 0:
            name = line[:col]
            raw = data[data.find(line.encode('latin-1')) + col + 1:]
            if len(raw) > 4:
                # Check for array: 2-byte LE length header + N*4 bytes
                alen = raw[0] | (raw[1] << 8)
                remaining = len(raw) - 2
                if alen > 0 and remaining == alen * 4:
                    floats = []
                    for i in range(alen):
                        f = struct.unpack('<f', raw[2 + i*4 : 2 + i*4 + 4])[0]
                        floats.append(f)
                    val = '[' + ', '.join(f'{f:.2f}' for f in floats[:8])
                    if alen > 8:
                        val += f', ... ({alen} total)'
                    val += ']'
                    yield (name, val, f'bin float[{alen}]')
                    continue
            if len(raw) >= 4:
                f = struct.unpack('<f', raw[:4])[0]
                yield (name, f'{f:.2f}', 'bin float')

def get_device_name(ip):
    """Query Tasmota device for its friendly name. Returns name or None."""
    try:
        url = f'http://{ip}/cm?cmnd=DeviceName'
        resp = urlopen(url, timeout=5)
        data = json.loads(resp.read())
        return data.get('DeviceName', None)
    except Exception:
        pass
    # Fallback: try Status 5 for hostname
    try:
        url = f'http://{ip}/cm?cmnd=Status%205'
        resp = urlopen(url, timeout=5)
        data = json.loads(resp.read())
        net = data.get('StatusNET', {})
        return net.get('Hostname', None)
    except Exception:
        return None

def resolve_device_names(ips):
    """Query all sender IPs in parallel for their friendly names."""
    names = {}
    print(f"Resolving {len(ips)} device names ...", end='', flush=True)
    with ThreadPoolExecutor(max_workers=8) as pool:
        futures = {pool.submit(get_device_name, ip): ip for ip in ips}
        for future in as_completed(futures):
            ip = futures[future]
            name = future.result()
            if name:
                names[ip] = name
                print(f".", end='', flush=True)
    print(f" {len(names)} found")
    return names

def main():
    duration = int(sys.argv[1]) if len(sys.argv) > 1 else 60

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, 'SO_REUSEPORT'):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    sock.bind(('', MCAST_PORT))
    mreq = struct.pack('4s4s', socket.inet_aton(MCAST_GRP), socket.inet_aton('0.0.0.0'))
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    sock.settimeout(2.0)

    print(f"Listening on {MCAST_GRP}:{MCAST_PORT} for {duration}s (Ctrl-C to stop) ...\n")

    # (source_ip, name) -> {value, source, type, timestamps[]}
    seen = OrderedDict()
    end_time = time.time() + duration

    try:
        while time.time() < end_time:
            try:
                data, addr = sock.recvfrom(2048)
            except socket.timeout:
                continue
            src = addr[0]
            now = time.time()
            for name, val, ptype in parse_packet(data):
                key = (src, name)
                if key in seen:
                    seen[key]['value'] = val
                    seen[key]['timestamps'].append(now)
                else:
                    seen[key] = {'name': name, 'value': val, 'source': src, 'type': ptype, 'timestamps': [now]}
    except KeyboardInterrupt:
        pass

    sock.close()

    if not seen:
        print("No variables received.")
        return

    # Collect unique sender IPs and resolve device names
    sender_ips = sorted(set(info['source'] for info in seen.values()),
                        key=lambda ip: tuple(int(p) for p in ip.split('.')))
    device_names = resolve_device_names(sender_ips)

    # Sort by sender IP (numeric), then var name
    def ip_sort_key(item):
        info = item[1]
        parts = info['source'].split('.')
        return (tuple(int(p) for p in parts), info['name'])

    sorted_items = sorted(seen.items(), key=ip_sort_key)

    # Collect all packet timestamps per sender for device-level interval
    device_timestamps = {}
    for key, info in seen.items():
        src = info['source']
        if src not in device_timestamps:
            device_timestamps[src] = []
        device_timestamps[src].extend(info['timestamps'])

    device_intervals = {}
    for src, ts in device_timestamps.items():
        ts.sort()
        if len(ts) >= 2:
            diffs = [ts[i+1] - ts[i] for i in range(len(ts)-1)]
            burst_gaps = [d for d in diffs if d > 0.3]
            if burst_gaps:
                avg = sum(burst_gaps) / len(burst_gaps)
                device_intervals[src] = f"{avg:.1f}s"
            else:
                device_intervals[src] = "continuous"
        else:
            device_intervals[src] = "once"

    # Detect name clashes: same var name from multiple senders
    name_senders = {}
    for key, info in seen.items():
        vname = info['name']
        if vname not in name_senders:
            name_senders[vname] = set()
        name_senders[vname].add(info['source'])
    clashes = {n: senders for n, senders in name_senders.items() if len(senders) > 1}

    # Print grouped by sender
    last_src = None
    for key, info in sorted_items:
        src = info['source']
        name = info['name']
        if src != last_src:
            if last_src is not None:
                print()
            interval = device_intervals.get(src, "?")
            count = sum(1 for _, i in seen.items() if i['source'] == src)
            dname = device_names.get(src, "")
            label = f"{src}  ({dname})" if dname else src
            print(f"--- {label}  ({count} vars, sends every {interval}) ---")
            print(f"  {'Name':<16} {'Type':<16} {'Value':<30}")
            last_src = src
        clash = " ** CLASH" if name in clashes else ""
        print(f"  {name:<16} {info['type']:<16} {info['value']:<30}{clash}")

    # Print clash summary
    if clashes:
        print(f"\n{'='*60}")
        print(f"WARNING: {len(clashes)} variable name clash(es) detected!")
        print(f"{'='*60}")
        for vname, senders in sorted(clashes.items()):
            ips = ', '.join(sorted(senders, key=lambda ip: tuple(int(p) for p in ip.split('.'))))
            names_str = ', '.join(f"{ip} ({device_names.get(ip, '?')})" for ip in sorted(senders, key=lambda ip: tuple(int(p) for p in ip.split('.'))))
            print(f"  '{vname}' sent by: {names_str}")

    print(f"\n{len(seen)} variables from {len(device_timestamps)} devices")

    # Export CSV for Numbers/Excel
    csv_path = os.path.expanduser("~/Desktop/udp_variables.csv")
    with open(csv_path, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(["Sender IP", "Device Name", "Variable", "Type", "Value", "Send Interval", "Receive Count", "Name Clash"])
        for key, info in sorted_items:
            src = info['source']
            name = info['name']
            count = len(info['timestamps'])
            interval = device_intervals.get(src, "?")
            dname = device_names.get(src, "")
            clash = "YES" if name in clashes else ""
            w.writerow([src, dname, name, info['type'], info['value'], interval, count, clash])
    print(f"\nCSV saved to {csv_path}")

if __name__ == '__main__':
    main()
