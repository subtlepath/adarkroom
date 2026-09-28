# The web flasher

A static site that writes A Dark Room onto an Xteink over Web Serial, published to GitHub Pages
(<https://subtlepath.github.io/adarkroom/>) by `.github/workflows/pages.yml` whenever `public/`
changes on `main`. Everything it serves is in `public/`:

- `index.html`, `style.css`: the page, in the browser game's own look.
- `flash.js`: chooses the image, checks the chip, keeps the save (leaves the NVS partition out of
  the write), reads a backup, writes and verifies, with [esptool-js](https://github.com/espressif/esptool-js)
  (`vendor/`, Apache 2.0) and `md5.js`.
- `firmware/`: the factory images and `manifest.json`, written by `make web` from the repository
  root. They are committed, so the site publishes as it is.
- `shots/`: screens from `make shots`.

`make web-dev` (from the repository root) serves it on http://localhost:8000; Web Serial works on
localhost as on https. Every path in it is relative, so it works under `/adarkroom/` as at a root.

The first time: in the repository's settings, Pages → Source → "GitHub Actions".
