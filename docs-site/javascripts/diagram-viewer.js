// A click on a diagram opens it full-screen, to zoom and pan: the wheel or a
// pinch zooms, a drag moves it, + - 0 zoom in, out and fit, Esc closes.
//
// Material draws each Mermaid diagram into a closed shadow root, which page
// scripts cannot reach, so the roots are noted as Material makes them (this
// file runs before Mermaid, which Material loads later) and the viewer
// copies the drawing from there.
(() => {
  const roots = new WeakMap();
  const attachShadow = Element.prototype.attachShadow;
  Element.prototype.attachShadow = function (init) {
    const root = attachShadow.call(this, init);
    if (this.classList.contains("mermaid")) {
      roots.set(this, root);
      this.tabIndex = 0;
      this.setAttribute("role", "button");
      this.setAttribute("aria-label", "Enlarge the diagram");
      this.title = "Click to enlarge";
    }
    return root;
  };

  const kMinScale = 0.1;
  const kMaxScale = 12;
  const kMaxFitScale = 3;  // a small diagram is not blown up past this
  // Room around a fitted diagram, in pixels: the buttons are at the top,
  // the hint at the bottom
  const kMargin = { top: 72, bottom: 56, side: 48 };

  let viewer = null;  // built on first use

  function build() {
    const dialog = document.createElement("dialog");
    dialog.className = "diagram-viewer";
    dialog.setAttribute("aria-label", "Diagram");
    dialog.innerHTML = `
      <div class="diagram-viewer__stage" tabindex="-1"><div class="diagram-viewer__content"></div></div>
      <div class="diagram-viewer__tools">
        <button type="button" data-action="in" title="Zoom in (+)" aria-label="Zoom in">+</button>
        <button type="button" data-action="out" title="Zoom out (-)" aria-label="Zoom out">&minus;</button>
        <button type="button" data-action="fit" title="Fit to the screen (0)">Fit</button>
        <button type="button" data-action="close" title="Close (Esc)" aria-label="Close">&times;</button>
      </div>
      <p class="diagram-viewer__hint">Scroll or pinch to zoom, drag to move</p>`;
    document.body.append(dialog);

    const stage = dialog.querySelector(".diagram-viewer__stage");
    const content = dialog.querySelector(".diagram-viewer__content");
    // The copy lives in a shadow root of its own, as the original does, so
    // the page's styles leave it alone
    const shadow = content.attachShadow({ mode: "open" });
    const state = {
      dialog, stage, content, shadow, svg: null, opener: null,
      width: 1, height: 1, scale: 1, x: 0, y: 0,
    };

    dialog.querySelector(".diagram-viewer__tools").addEventListener("click", (event) => {
      const action = event.target.closest("button")?.dataset.action;
      const rect = stage.getBoundingClientRect();
      if (action === "in") zoomAt(state, 1.5, rect.width / 2, rect.height / 2);
      if (action === "out") zoomAt(state, 1 / 1.5, rect.width / 2, rect.height / 2);
      if (action === "fit") fit(state);
      if (action === "close") dialog.close();
    });

    dialog.addEventListener("keydown", (event) => {
      // Material's own shortcuts (n and p change the page, s searches)
      // stay behind the viewer; Esc still closes it
      event.stopPropagation();
      const rect = stage.getBoundingClientRect();
      const step = 64;
      const keys = {
        "+": () => zoomAt(state, 1.5, rect.width / 2, rect.height / 2),
        "=": () => zoomAt(state, 1.5, rect.width / 2, rect.height / 2),
        "-": () => zoomAt(state, 1 / 1.5, rect.width / 2, rect.height / 2),
        "0": () => fit(state),
        ArrowLeft: () => moveBy(state, step, 0),
        ArrowRight: () => moveBy(state, -step, 0),
        ArrowUp: () => moveBy(state, 0, step),
        ArrowDown: () => moveBy(state, 0, -step),
      };
      if (keys[event.key]) {
        event.preventDefault();
        keys[event.key]();
      }
    });

    stage.addEventListener("wheel", (event) => {
      event.preventDefault();
      // A pinch on a trackpad arrives as a wheel event with Ctrl held, in
      // smaller steps
      const rate = event.ctrlKey ? 0.01 : 0.002;
      const point = pointIn(stage, event);
      zoomAt(state, Math.exp(-event.deltaY * rate), point.x, point.y);
    }, { passive: false });

    stage.addEventListener("dblclick", (event) => {
      const point = pointIn(stage, event);
      zoomAt(state, 2, point.x, point.y);
    });

    // A drag moves the diagram; two fingers pinch it
    const pointers = new Map();
    let pinch = null;
    stage.addEventListener("pointerdown", (event) => {
      stage.setPointerCapture(event.pointerId);
      pointers.set(event.pointerId, pointIn(stage, event));
      stage.classList.add("is-dragging");
      if (pointers.size === 2) pinch = pinchOf(pointers);
    });
    stage.addEventListener("pointermove", (event) => {
      const last = pointers.get(event.pointerId);
      if (!last) return;
      const point = pointIn(stage, event);
      pointers.set(event.pointerId, point);
      if (pointers.size === 1) {
        moveBy(state, point.x - last.x, point.y - last.y);
      } else if (pinch) {
        const now = pinchOf(pointers);
        moveBy(state, now.x - pinch.x, now.y - pinch.y);
        zoomAt(state, now.distance / pinch.distance, now.x, now.y);
        pinch = now;
      }
    });
    const release = (event) => {
      pointers.delete(event.pointerId);
      pinch = pointers.size === 2 ? pinchOf(pointers) : null;
      if (pointers.size === 0) stage.classList.remove("is-dragging");
    };
    stage.addEventListener("pointerup", release);
    stage.addEventListener("pointercancel", release);

    window.addEventListener("resize", () => {
      if (dialog.open) fit(state);
    });
    dialog.addEventListener("close", () => {
      state.shadow.replaceChildren();
      state.opener?.focus();
    });
    return state;
  }

  function pointIn(stage, event) {
    const rect = stage.getBoundingClientRect();
    return { x: event.clientX - rect.left, y: event.clientY - rect.top };
  }

  function pinchOf(pointers) {
    const [a, b] = [...pointers.values()];
    return {
      x: (a.x + b.x) / 2,
      y: (a.y + b.y) / 2,
      distance: Math.max(1, Math.hypot(a.x - b.x, a.y - b.y)),
    };
  }

  // The drawing is resized rather than scaled with a transform, so the
  // browser draws the vectors sharp at every size
  function place(state) {
    state.svg.setAttribute("width", String(state.width * state.scale));
    state.svg.setAttribute("height", String(state.height * state.scale));
    state.content.style.left = `${state.x}px`;
    state.content.style.top = `${state.y}px`;
  }

  function fit(state) {
    const rect = state.stage.getBoundingClientRect();
    const room = {
      width: rect.width - 2 * kMargin.side,
      height: rect.height - kMargin.top - kMargin.bottom,
    };
    state.scale = Math.min(room.width / state.width, room.height / state.height,
                           kMaxFitScale);
    state.x = kMargin.side + (room.width - state.width * state.scale) / 2;
    state.y = kMargin.top + (room.height - state.height * state.scale) / 2;
    place(state);
  }

  // Zooms by `factor`, keeping the point (x, y) of the stage still
  function zoomAt(state, factor, x, y) {
    const scale = Math.min(kMaxScale, Math.max(kMinScale, state.scale * factor));
    const ratio = scale / state.scale;
    state.x = x - (x - state.x) * ratio;
    state.y = y - (y - state.y) * ratio;
    state.scale = scale;
    place(state);
  }

  function moveBy(state, dx, dy) {
    state.x += dx;
    state.y += dy;
    place(state);
  }

  function open(host) {
    const drawing = roots.get(host)?.querySelector("svg");
    if (!drawing) return;
    viewer ??= build();
    const svg = drawing.cloneNode(true);
    const box = svg.viewBox.baseVal;
    const size = box && box.width ? box : drawing.getBoundingClientRect();
    svg.style.maxWidth = "none";
    svg.removeAttribute("width");
    svg.removeAttribute("height");
    const style = document.createElement("style");
    style.textContent = ":host { display: block; } svg { display: block; }";
    viewer.shadow.replaceChildren(style, svg);
    Object.assign(viewer, { svg, opener: host, width: size.width, height: size.height });
    viewer.dialog.showModal();
    fit(viewer);
    viewer.stage.focus();
  }

  document.addEventListener("click", (event) => {
    const host = event.target.closest?.("div.mermaid");
    if (host && roots.has(host)) open(host);
  });
  document.addEventListener("keydown", (event) => {
    const host = event.target.closest?.("div.mermaid");
    if (host && roots.has(host) && (event.key === "Enter" || event.key === " ")) {
      event.preventDefault();
      open(host);
    }
  });
})();
