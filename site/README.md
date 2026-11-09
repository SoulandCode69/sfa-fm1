# SFA public site

`index.html` is one self-contained page: no build step, no scripts, no external fonts, no tracking. It can go on any static
host (sinfetish.com later) by copying this folder.

Before publishing:

* Replace the "coming soon" lines with real download links once a release is cut (`tools/release.py` makes the ECB zip).
* Add a contact address under About.
* Check every claim on the page against the repo. The page says nothing about price, date or availability of the box beyond
  "plan, not a product yet"; keep it that way until that is true.
* The FM-1 note in the footer is a trademark courtesy; keep it.

Preview locally: `tools/sfa console` then open `http://localhost:8590/site/`.
