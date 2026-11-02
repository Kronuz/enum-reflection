#!/usr/bin/env python3
"""Mutation check of enum.h's name lookup (CTest `enum_reflection_mutants`, off by default).

Each mutant is one edit of a copy of enum.h. test/names_test.cc is built against the copy and the
section of the test that is meant to catch the edit has to fail (an edit that stops the test from
compiling through a static_assert counts). The unedited header has to pass every section first, and
an edit whose text is not found fails the run, so the list cannot go stale.

usage: mutants.py CXX REPO_DIR DEPS_DIR [compiler flags ...]   (DEPS_DIR holds the fetched libraries, <name>-src)
"""
import glob
import os
import subprocess
import sys
import tempfile

cxx, repo, deps = sys.argv[1], sys.argv[2], sys.argv[3]
flags = sys.argv[4:] + ["-I" + d for d in sorted(glob.glob(os.path.join(deps, "*-src")))]

# (name, section that must fail, [(old text, new text), ...])
MUTANTS = [
    ("verification removed",
     "synthetic",
     [("if (!_enum_probe_equal(e, probe, n)) {", "if (false) {")]),
    ("verification removed after the hash check: the unverified lookup, a forged collision is accepted",
     "forged",
     [("phf_names.lookup(probe.hash)", "phf_names.find(probe.hash)"),
      ("if (!_enum_probe_equal(e, probe, n)) {", "if (false) {"),
      ("phf_names.lookup(_enum_hash_long(name.data(), name.size()))", "phf_names.find(_enum_hash_long(name.data(), name.size()))"),
      ("if (name.size() == e.size && _enum_long_equal(e.data, name.data(), name.size())) {", "if (true) {")]),
    ("length check removed, names of up to 16 bytes",
     "synthetic",
     [("return ((n ^ e.size) | ", "return (")]),
    ("length check removed, names of up to 16 bytes (the length test alone)",
     "pieces",
     [("return ((n ^ e.size) | ", "return (")]),
    ("length check removed, longer names",
     "synthetic",
     [("if (name.size() == e.size && _enum_long_equal(", "if (_enum_long_equal(")]),
    ("length check removed, names of 17 to 24 bytes",
     "synthetic",
     [("	if (n != e.size) {\n		return std::nullopt;\n	}\n	if (((first ^", "	if (((first ^")]),
    ("middle eight bytes not compared, names of 17 to 24 bytes",
     "synthetic",
     [("| (second ^ _enum_load8(e.data + 8))", "")]),
    ("hash of an input of 24 bytes missing its last word",
     "pieces",
     [("	if (n == 24) {\n		return _enum_hash_finish(_enum_hash_step(h, _enum_lower(last)), 0);\n	}\n", "")]),
    ("last eight bytes not compared",
     "synthetic",
     [("| (probe.word[1] ^ e.word[1])) == 0;", ") == 0;")]),
    ("first eight bytes not compared",
     "synthetic",
     [("| (probe.word[0] ^ e.word[0]) |", "|")]),
    ("a name of 1 to 3 bytes not lower-cased (the table)",
     "pieces",
     [("_enum_byte(chars::tolower(p[0]))", "_enum_byte(p[0])")]),
    ("a name of 1 to 3 bytes not lower-cased (the word)",
     "pieces",
     [("return _enum_lower(first | (middle << (8 * (n >> 1))) | (last << (8 * (n - 1))));", "return first | (middle << (8 * (n >> 1))) | (last << (8 * (n - 1)));")]),
    ("last four bytes of a name of 4 to 7 not read",
     "bytes",
     [("tail = word0 = _enum_lower(first | (last << (8 * (n - 4))));", "tail = word0 = _enum_lower(first);")]),
    ("middle byte of a name of 1 to 3 not read",
     "pieces",
     [("return first | (middle << (8 * (n >> 1))) | (last << (8 * (n - 1)));", "return first | (last << (8 * (n - 1)));"),
      ("return _enum_lower(first | (middle << (8 * (n >> 1))) | (last << (8 * (n - 1))));", "return _enum_lower(first | (last << (8 * (n - 1))));")]),
    ("tail window of an entry taken from the wrong place",
     "bytes",
     [("e.word[1] = _enum_lower(_enum_pack(p + n - 8, 8));", "e.word[1] = _enum_lower(_enum_pack(p, 8));")]),
    ("every byte is lower-cased: 0x20 set in all of them",
     "pieces",
     [("return w | ((from_A & ~past_Z & ~w & (0x80 * ones)) >> 2);", "return w | ((0x80 * ones) >> 2);")]),
    ("a pair of words lower-cased up to 'Z' + 1 (vector form)",
     "pieces",
     [("const _enum_bytes16 upper = (_enum_bytes16)((bytes - 'A') <= 25);", "const _enum_bytes16 upper = (_enum_bytes16)((bytes - 'A') <= 26);")]),
    ("'[' lower-cased with the capitals",
     "pieces",
     [("const std::uint64_t past_Z = low7 + (0x25 * ones);", "const std::uint64_t past_Z = low7 + (0x24 * ones);")]),
    ("the entry keeps the case of the name (the input is lower-cased)",
     "variants",
     [("e.word[0] = _enum_lower(_enum_pack(p, n));", "e.word[0] = _enum_pack(p, n);")]),
    ("a slot that is no member's matches the empty input",
     "pieces",
     [("e.word[0] = 0x4141414141414141ULL;", "e.word[0] = 0;")]),
    ("the input is not lower-cased before the comparison of a name of 8 to 16 bytes",
     "variants",
     [("_enum_lower_pair(word0, word1);", "word0 = _enum_lower(word0);")]),
    ("hash of the tail of an input of 8 to 15 bytes",
     "pieces",
     [("tail = (word1 >> 1) >> (63 - 8 * (n & 7));", "tail = (word1 >> 1) >> (62 - 8 * (n & 7));")]),
    ("hash of an input of 16 bytes missing its second word",
     "pieces",
     [("if (n == 16) {", "if (false) {")]),
]


def run(workdir, header_text, section, tag):
    os.makedirs(workdir, exist_ok=True)
    with open(os.path.join(workdir, "enum.h"), "w") as f:
        f.write(header_text)
    exe = os.path.join(workdir, "names_" + tag)
    build = subprocess.run([cxx] + flags + ["-I" + workdir, os.path.join(repo, "test", "names_test.cc"), "-o", exe],
                           capture_output=True, text=True)
    if build.returncode != 0:
        return "static_assert" in build.stderr, "build failed: " + (build.stderr.strip().splitlines() or [""])[-1][:160], True
    out = subprocess.run([exe] + ([section] if section else []), capture_output=True, text=True)
    return out.returncode != 0, (out.stderr.strip().splitlines() or ["(no message)"])[-1][:160], False


header = open(os.path.join(repo, "enum.h")).read()
failed = False
with tempfile.TemporaryDirectory() as tmp:
    ok, msg, _ = run(os.path.join(tmp, "base"), header, "", "base")
    # The baseline has to pass: a failure here is not a kill.
    if ok:
        print("baseline fails: " + msg)
        sys.exit(1)
    for number, (name, section, edits) in enumerate(MUTANTS):
        text = header
        for old, new in edits:
            if text.count(old) != 1:
                print("mutant %d (%s): the text to edit is not in enum.h exactly once: %s" % (number, name, old))
                sys.exit(1)
            text = text.replace(old, new)
        killed, msg, did_not_build = run(os.path.join(tmp, "m%d" % number), text, section, "m%d" % number)
        if did_not_build and not killed:
            print("mutant %d (%s): does not build: %s" % (number, name, msg))
            failed = True
        elif killed:
            print("killed   %-96s by [%s]: %s" % (name, section, msg))
        else:
            print("SURVIVED %s (section %s)" % (name, section))
            failed = True
sys.exit(1 if failed else 0)
