#!/usr/bin/env python3
"""Check EVERYTHING a push would publish - before the push. Complements scripts/pre_push_secrets.py.

Run in the folder you push from (main checkout or a git worktree):
    python3 check_push_full.py [remote/branch]          (default origin/universal)
    python3 check_push_full.py origin/universal --no-mail   (also require an EMPTY author/committer e-mail)
Exit code 0 = clean, 1 = hit or error, 2 = texts clean, but images/PDFs must be looked at by hand.

For every commit that is not yet on the target:
  1. author and committer: name must equal `git config user.name`; with --no-mail the e-mail must be empty.
  2. commit message against the secrets list.
  3. every new/changed file against the secrets list: plain bytes, .gz unpacked, .zip (also .gz inside), PDF streams
     (FlateDecode) unpacked, and the file name itself. A hit that is already in the SAME file on the target is only
     reported as a note (it is not published by this push).
  4. images cannot be read by a script -> listed, look at them before pushing. PDFs too: Chrome PDFs store text as
     glyph ids, after inflating even plain words are not findable - check the HTML source and the images instead.
Secrets list as in pre_push_secrets.py: $SECRETS_LIST, <common git dir>/secrets.txt, ~/.tinyc_secrets, plus the
STA_SSID/STA_PASS values from user_config_override.h ($CREDS_HEADER or the MAIN checkout, also from a worktree).
Only path and LABEL are reported, never a value.

Why: pre_push_secrets.py looks at raw file bytes only (not at author, commit message, .gz/.zip content, PDF text),
and in a git worktree it found neither secrets.txt nor user_config_override.h and skipped the check.
On exFAT drives git refuses to answer without safe.directory; this script passes it and stops on ANY git error,
so an empty answer can never look like "nothing to push".
"""
import gzip, io, os, re, subprocess, sys, zipfile, zlib

IMG = ('.png', '.jpg', '.jpeg', '.gif', '.webp', '.bmp', '.ico')
CWD = os.getcwd().replace('\\', '/')


def git(*a, ok_fail=False):
    r = subprocess.run(['git', '-c', 'safe.directory=' + CWD, *a], capture_output=True)
    if r.returncode != 0:
        if ok_fail:
            return None
        print('ERROR in git ' + ' '.join(a[:3]) + ': ' + r.stderr.decode('utf-8', 'replace').strip()[:300])
        sys.exit(1)
    return r.stdout


def secrets_list():
    common = os.path.abspath(git('rev-parse', '--git-common-dir').decode().strip())
    main_top = os.path.dirname(common)          # main checkout (works from a worktree too)
    out = []
    for cand in (os.environ.get('SECRETS_LIST'), os.path.join(common, 'secrets.txt'),
                 os.path.expanduser('~/.tinyc_secrets')):
        if cand and os.path.isfile(cand):
            for n, z in enumerate(open(cand, encoding='utf-8', errors='replace').read().splitlines(), 1):
                z = z.strip()
                if not z or z.startswith('#'):
                    continue
                lab, sep, w = z.partition('|')
                if not sep:
                    lab, w = f'line {n}', z
                if len(w.strip()) >= 4:
                    out.append((lab.strip(), w.strip().lower().encode()))
            break
    hdr = os.environ.get('CREDS_HEADER') or os.path.join(main_top, 'tasmota', 'user_config_override.h')
    try:
        for m in re.finditer(r'#define\s+(STA_SSID\d|STA_PASS\d)\s+"([^"]+)"', open(hdr, errors='replace').read()):
            if len(m.group(2)) >= 4:
                out.append((m.group(1), m.group(2).lower().encode()))
    except OSError:
        pass
    return out


def hits(data, lst):
    d = data.lower()
    return {lab for lab, w in lst if w in d}


def pdf_streams(data):
    parts = []
    for m in re.finditer(rb'stream\r?\n', data):
        a = m.end()
        e = data.find(b'endstream', a)
        if e < 0:
            continue
        try:
            parts.append(zlib.decompressobj().decompress(data[a:e]))
        except Exception:
            pass
    return b'\n'.join(parts)


def check(name, data, lst, out, manual):
    n = name.lower()
    if n.endswith(IMG):
        manual.append(name)
    t = hits(data, lst) | hits(name.encode('utf-8', 'replace'), lst)
    if n.endswith('.gz'):
        try:
            check(name[:-3] + ' (from .gz)', gzip.decompress(data), lst, out, manual)
        except Exception:
            out.append((name, {'ERROR: .gz cannot be unpacked'}))
    elif n.endswith('.zip'):
        try:
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                for f in z.namelist():
                    if not f.endswith('/'):
                        check(name + ' > ' + f, z.read(f), lst, out, manual)
        except zipfile.BadZipFile:
            out.append((name, {'ERROR: .zip not readable'}))
    elif n.endswith('.pdf'):
        t |= hits(pdf_streams(data), lst)
        manual.append(name + '  (PDF: text not readable - check HTML source + images)')
    if t:
        out.append((name, t))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    no_mail = '--no-mail' in sys.argv
    target = args[0] if args else 'origin/universal'
    lst = secrets_list()
    if not lst:
        print('ERROR: no secrets list found - nothing could be checked')
        return 1
    me = (git('config', 'user.name', ok_fail=True) or b'').decode().strip()
    commits = git('rev-list', 'HEAD', '--not', target).decode().split()
    if not commits:
        print('Nothing to push: HEAD is already on', target)
        return 0
    out, manual, notes = [], [], []
    for c in commits:
        an, ae, cn, ce, text = (git('log', '-1', '--format=%an%x00%ae%x00%cn%x00%ce%x00%B', c)
                                .decode('utf-8', 'replace').split('\x00') + [''] * 5)[:5]
        if me and (an != me or cn != me):
            out.append(('commit ' + c[:9], {'author/committer name is not ' + me}))
        if no_mail and (ae.strip() or ce.strip()):
            out.append(('commit ' + c[:9], {'E-MAIL ADDRESS set (must be empty)'}))
        t = hits((text + an + ae + cn + ce).encode('utf-8', 'replace'), lst)
        if t:
            out.append(('commit text/author ' + c[:9], t))
    paths = {}
    for z in git('rev-list', '--objects', 'HEAD', '--not', target).decode('utf-8', 'replace').splitlines():
        sha, _, p = z.partition(' ')
        if p:
            paths[sha] = p
    nfiles = 0
    for sha, p in paths.items():
        if git('cat-file', '-t', sha).strip() != b'blob':
            continue
        nfiles += 1
        tmp = []
        check(p, git('cat-file', 'blob', sha), lst, tmp, manual)
        if tmp and not p.lower().endswith(('.gz', '.zip', '.pdf')):
            old = git('show', target + ':' + p, ok_fail=True) or b''
            already = hits(old, lst)
            for name, t in tmp:
                new = {x for x in t if x not in already}
                if new:
                    out.append((name, new))
                if t - new:
                    notes.append((name, t - new))
        else:
            out.extend(tmp)
    print(f'{len(commits)} commit(s), {nfiles} file(s) checked, {len(lst)} search strings')
    for c in commits:
        print('  ' + git('log', '-1', '--format=%h %an <%ae> %s', c).decode('utf-8', 'replace').strip())
    for name, t in notes:
        print(f'  note: {name}: {", ".join(sorted(t))} is already in this file on {target} - not added by this push')
    if out:
        print('\nHITS - DO NOT PUSH:')
        for name, t in out:
            print(f'  {name}: {", ".join(sorted(t))}')
        return 1
    if manual:
        print('\nTexts clean. Look at these BY HAND before pushing:')
        for b in sorted(set(manual)):
            print('  ' + b)
        return 2
    print('\nclean')
    return 0


if __name__ == '__main__':
    sys.exit(main())
