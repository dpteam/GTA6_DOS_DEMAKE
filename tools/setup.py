"""One-time setup: fetch DJGPP, CWSDPMI and the Unifont source.

Everything lands in vendor/ and is safe to re-run. If a neighbouring DOSYA
project already downloaded these, they are reused instead of pulling 130 MB
over the wire again.
"""
import gzip
import os
import sys
import urllib.request
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vendor  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VENDOR = os.path.join(ROOT, "vendor")

DJGPP_URL = ("https://github.com/andrewwutw/build-djgpp/releases/download/"
             "v3.4/djgpp-mingw-gcc1220-standalone.zip")
CWSDPMI_URL = "https://www.delorie.com/pub/djgpp/current/v2misc/csdpmi7b.zip"
UNIFONT_URL = ("https://unifoundry.com/pub/unifont/unifont-15.1.05/"
               "font-builds/unifont-15.1.05.hex.gz")


def fetch(url, dest):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        print("  have", os.path.basename(dest))
        return
    print("  downloading", url)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    urllib.request.urlretrieve(url, dest)
    print("  got %d bytes" % os.path.getsize(dest))


def unzip(src, dest):
    marker = os.path.join(dest, ".done")
    if os.path.exists(marker):
        print("  have", os.path.basename(dest))
        return
    print("  extracting", os.path.basename(src))
    with zipfile.ZipFile(src) as z:
        z.extractall(dest)
    open(marker, "w").close()


def have(path):
    return os.path.exists(path)


def main():
    # A sibling DOSYA checkout is the cheapest possible download.
    sib = vendor.SIBLING
    if os.path.isdir(os.path.join(sib, "djgpp", "djgpp")) and \
            not os.path.isdir(os.path.join(VENDOR, "djgpp")):
        print("[0/3] reusing vendor/ from %s" % sib)
        print("      (set GTA_VENDOR to use a different one)")

    os.makedirs(VENDOR, exist_ok=True)

    print("[1/3] DJGPP")
    if have(vendor.djgpp_dir()):
        print("  have djgpp")
    else:
        z = os.path.join(VENDOR, "djgpp.zip")
        fetch(DJGPP_URL, z)
        unzip(z, os.path.join(VENDOR, "djgpp"))

    print("[2/3] CWSDPMI")
    if have(vendor.cwsdpmi() or ""):
        print("  have CWSDPMI")
    else:
        z = os.path.join(VENDOR, "csdpmi.zip")
        fetch(CWSDPMI_URL, z)
        unzip(z, os.path.join(VENDOR, "csdpmi"))

    print("[3/3] Unifont")
    uf = vendor.unifont()
    if uf:
        print("  have", os.path.basename(uf))
    else:
        dest = os.path.join(VENDOR, "unifont.hex.gz")
        fetch(UNIFONT_URL, dest)
        # sanity: it must actually be gzip, not an HTML error page
        try:
            with gzip.open(dest, "rt", encoding="utf-8") as f:
                f.readline()
        except Exception as e:
            raise SystemExit("unifont download looks broken: %s" % e)

    print()
    print("setup done. next: python tools/build.py dos")


if __name__ == "__main__":
    main()
