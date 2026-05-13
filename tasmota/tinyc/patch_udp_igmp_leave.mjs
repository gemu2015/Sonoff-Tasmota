#!/usr/bin/env node
// patch_udp_igmp_leave.mjs — teach the IDE's compileUdpFunc about mode 10
//
// Firmware TC_RELEASE 1.6.1 adds SYS_UDP_FUNC case 10:
//   udp(10, mcast_ip)  → calls igmp_leavegroup() at the LwIP netif level
//
// The runtime accepts the new mode unconditionally (it's just a switch case),
// but the IDE compiler still rejects mode 10 in compileUdpFunc with
// "udp() mode must be 0-9, got 10" and refuses to emit the bytecode.
// This patch adds the case 10 branch and bumps the legal-range error text.
//
// Idempotent: detects already-patched state and exits clean.
//
// Usage: node patch_udp_igmp_leave.mjs [path/to/tinyc_ide.html.gz]
//        Default path: ./tinyc_ide.html.gz (same dir as this script)

import { readFileSync, writeFileSync, existsSync, copyFileSync } from 'node:fs';
import { gunzipSync, gzipSync } from 'node:zlib';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const target = process.argv[2] || join(__dirname, 'tinyc_ide.html.gz');

if (!existsSync(target)) {
  console.error(`ERROR: ${target} not found`);
  process.exit(1);
}

const gz = readFileSync(target);
const src = gunzipSync(gz).toString('utf8');

// Idempotency check
if (src.includes('case 10: // udp(10, mcast_ip)')) {
  console.log(`OK: ${target} already patched — case 10 (udp IGMP-leave) present, nothing to do.`);
  process.exit(0);
}

// ── Patch 1: add case 10 branch right before the default: in compileUdpFunc.
// Anchor on the unique case 9 block; insert before the trailing `default:`.
const c9Anchor = `            case 9: // udp(9, mcast_ip, port) → join multicast group on udp_port
                if (nargs !== 3) throw new CodeGenError("udp(9, mcast_ip, port) expects 3 arguments", node.line);
                this.emitStringArg(node.args[1]);  // mcast_ip (char[] or string literal)
                this.compileExpr(node.args[2]);    // port (int)
                break;
            default:
                throw new CodeGenError(\`udp() mode must be 0-9, got \${mode}\`, node.line);`;

const c9Replacement = `            case 9: // udp(9, mcast_ip, port) → join multicast group on udp_port
                if (nargs !== 3) throw new CodeGenError("udp(9, mcast_ip, port) expects 3 arguments", node.line);
                this.emitStringArg(node.args[1]);  // mcast_ip (char[] or string literal)
                this.compileExpr(node.args[2]);    // port (int)
                break;
            case 10: // udp(10, mcast_ip) → IGMP-Leave on the netif (socket-independent)
                if (nargs !== 2) throw new CodeGenError("udp(10, mcast_ip) expects 2 arguments", node.line);
                this.emitStringArg(node.args[1]);  // mcast_ip (char[] or string literal)
                break;
            default:
                throw new CodeGenError(\`udp() mode must be 0-10, got \${mode}\`, node.line);`;

if (!src.includes(c9Anchor)) {
  console.error('ERROR: case 9 anchor block not found — IDE source may have changed.');
  console.error('Look for compileUdpFunc() in the unpacked tinyc_ide.html and patch manually.');
  process.exit(2);
}

const patched = src.replace(c9Anchor, c9Replacement);

if (patched === src) {
  console.error('ERROR: replacement produced no change (regex / string-literal mismatch)');
  process.exit(3);
}

// ── Patch 2: bump the literal-int parser hint above the switch (the parser
// reads mode from an IntLiteral and throws a similar 0-9 range message).
// Look for it and update if present; if not, no-op (the switch's default
// already catches out-of-range).
const parserAnchor = `must be a literal integer 0-9`;
const parserReplacement = `must be a literal integer 0-10`;
const patched2 = patched.includes(parserAnchor)
  ? patched.replace(parserAnchor, parserReplacement)
  : patched;

// Backup, regzip, write
const bakPath = target + '.bak';
if (!existsSync(bakPath)) copyFileSync(target, bakPath);

const out = gzipSync(Buffer.from(patched2, 'utf8'), { level: 9 });
writeFileSync(target, out);

console.log(`OK: patched ${target}`);
console.log(`    backup: ${bakPath}`);
console.log(`    size: ${gz.length} → ${out.length} bytes`);
console.log(`    new mode 10: udp(10, mcast_ip) → IGMP-Leave at netif level`);
