"""Copy the manuscript with edition values filled in (@CODE_URL@, @VERSION@)."""
from pathlib import Path

PLACEHOLDER_URL = "[code URL to be added]"

def code_url(cfg):
    return cfg.get("code_url") or PLACEHOLDER_URL

def prepare(ms_dir, dest, cfg):
    dest = Path(dest); dest.mkdir(parents=True, exist_ok=True)
    for old in dest.glob("*.md"):
        old.unlink()
    for p in sorted(Path(ms_dir).glob("*.md")):
        s = p.read_text().replace("@CODE_URL@", code_url(cfg)).replace("@VERSION@", str(cfg["version"]))
        (dest / p.name).write_text(s)
    return dest
