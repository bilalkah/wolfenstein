# Images on this site

Pictures live in `docs-site/assets/`: screenshots in `screenshots/`,
animations in `gifs/`, diagrams in `diagrams/`. PNG and GIF files are
stored with Git LFS (see `.gitattributes`), so run `git lfs install`
before adding any.

## A screenshot with a caption

```markdown
<figure markdown="span">
  ![The rail yard, mid-fight](../assets/screenshots/combat-rail-yard.png){ width="640" }
  <figcaption>The rail yard, mid-fight: the HUD, a hit marker, a body.</figcaption>
</figure>
```

- `<figure markdown="span">` lets Markdown work inside the HTML (the
  `md_in_html` extension); the figure is centred.
- `{ width="..." }` (the `attr_list` extension) sets the displayed width
  in pixels; the height follows. The game draws at 1200×900 natively;
  display screenshots at 480 to 720 pixels wide.
- The `![...]` text is the image's description for screen readers and
  search; the `<figcaption>` is what readers see under it.
- Paths are relative to the page: from `engine/renderer.md` it is
  `../assets/...`, from `index.md` it is `assets/...`.
- Clicking an image opens it full size (the `glightbox` plugin). To leave
  one out, add `{ .skip-lightbox }`.

## An animation

```markdown
<figure markdown="span">
  ![A door opening](../assets/gifs/door-open.gif){ width="480" loading="lazy" }
  <figcaption>A door opening.</figcaption>
</figure>
```

Keep GIFs short (a few seconds) and small (under 2 MB): crop to the part
that moves, and use 15 frames a second. A video is smaller for anything
longer:

```markdown
<figure markdown="span">
  <video src="../assets/videos/level-one.mp4" width="640" controls muted loop playsinline></video>
  <figcaption>The first level, played through.</figcaption>
</figure>
```

## Two images side by side

```markdown
<div class="grid" markdown>

![Before](../assets/screenshots/before.png){ width="360" }

![After](../assets/screenshots/after.png){ width="360" }

</div>
```

## Taking screenshots

- **Native:** run the game, pause on the view, and use the system's
  screenshot tool on the window. `--debug` and **P** show the 2D map view.
- **Web:** in a browser, the canvas can be saved with
  `document.querySelector('canvas').toDataURL()` from the console, or
  with a headless browser script.

Screenshots show the game's own art and Freedoom's (BSD licence); never
add screenshots of another game's assets.
