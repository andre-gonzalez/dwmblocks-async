# Passwordless package pre-download for `dwm_packages`

The `bar-functions/dwm_packages` block pre-downloads pending repo upgrades on
unmetered networks with `yay -Syuw` (download only, no install) so the actual
upgrade is fast. This requires a one-time sudoers rule so it can run unattended.

## Why it prompts for a password

`yay -Syuw` writes downloaded packages into the root-owned cache
`/var/cache/pacman/pkg`, so it runs `sudo pacman ...` under the hood. Capturing
the exact root calls (`yay -Syuw --noconfirm --sudo <wrapper>`) shows:

```
pacman -S -w -y      --noconfirm --config /etc/pacman.conf --
pacman -S -y -u -w   --noconfirm --config /etc/pacman.conf -- <pkgs>
pacman -D --asdeps   --noconfirm --config /etc/pacman.conf -- <pkgs>
```

The fix is a **NOPASSWD sudoers rule**. Package names are dynamic, so the rule
uses trailing wildcards.

## Steps

**1. Create the drop-in with `visudo`** (validates syntax on save — never edit
with a plain editor):

```sh
sudo visudo -f /etc/sudoers.d/dwm-packages
```

**2. Paste this and save:**

```
# Passwordless package pre-download for the dwmblocks packages block.
# -w = download only (no install), so this cannot install/remove anything.
frank ALL=(root) NOPASSWD: /usr/bin/pacman -S -w *, \
                           /usr/bin/pacman -S -y -u -w *, \
                           /usr/bin/pacman -D --asdeps *
```

**3. Lock down the file's permissions** (sudo refuses world-writable drop-ins):

```sh
sudo chmod 0440 /etc/sudoers.d/dwm-packages
```

**4. Validate the whole sudoers set:**

```sh
sudo visudo -c
```

Expect `/etc/sudoers.d/dwm-packages: parsed OK`.

**5. Test it runs without a prompt:**

```sh
yay -Syuw --noconfirm
```

If it downloads without asking for a password, the bar script will now
pre-download silently on unmetered networks.

## Notes

- **Why these three lines, not just `pacman`:** scoping to the exact flag-sets
  keeps the grant download-only (`-w` never installs) plus the harmless
  `-D --asdeps` reason-marker. The broader, simpler alternative is one line —
  `frank ALL=(root) NOPASSWD: /usr/bin/pacman` — but that lets *any* pacman
  operation (including installs/removals) run as root passwordless. The scoped
  version above is recommended.
- **Flag order matters:** sudoers matches argv in order. The rule mirrors what
  yay actually emitted (`-S -w -y` and `-S -y -u -w`). If a future yay version
  reorders flags and it starts prompting again, re-run the capture
  (`yay -Syuw --noconfirm --sudo <wrapper>`) and adjust.
- **Only sudo-free alternative:** download into a user-owned `--cachedir`
  instead — but then that dir must also be a `CacheDir` in `/etc/pacman.conf` or
  packages get fetched twice at install. The sudoers rule is cleaner.

## Related

- The update count in `dwm_packages` uses an inline replacement for
  `pacman-contrib`'s `checkupdates` (a private temp database synced via
  `fakeroot`), so no extra package is needed for counting.